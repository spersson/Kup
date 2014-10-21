/***************************************************************************
 *   Copyright Simon Persson                                               *
 *   simonpersson1@gmail.com                                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "mergedvfs.h"
#include "vfshelpers.h"

#include <QDebug>
#include <KLocale>

#include <git2/branch.h>
#include <sys/stat.h>

typedef QMap<QString, MergedNode *> NameMap;
typedef QMapIterator<QString, MergedNode *> NameMapIterator;

git_repository *MergedNode::mRepository = NULL;

bool mergedNodeLessThan(const MergedNode *a, const MergedNode *b) {
	if(a->isDirectory() != b->isDirectory()) {
		return a->isDirectory();
	}
	return a->objectName() < b->objectName();
}

bool versionGreaterThan(const VersionData *a, const VersionData *b) {
	return a->mModifiedDate > b->mModifiedDate;
}


MergedNode::MergedNode(QObject *pParent, const QString &pName, uint pMode)
   :QObject(pParent)
{
	mSubNodes = NULL;
	setObjectName(pName);
	mMode = pMode;
}

void MergedNode::getBupUrl(int pVersionIndex, KUrl *pComplete, QString *pRepoPath,
                           QString *pBranchName, quint64 *pCommitTime, QString *pPathInRepo) const {
	QList<const MergedNode *> lStack;
	const MergedNode *lNode = this;
	while(lNode != NULL) {
		lStack.append(lNode);
		lNode = qobject_cast<const MergedNode *>(lNode->parent());
	}
	const MergedRepository *lRepo = qobject_cast<const MergedRepository *>(lStack.takeLast());
	if(pComplete) {
		pComplete->setProtocol(QLatin1String("bup"));
		pComplete->addPath(lRepo->objectName());
		pComplete->addPath(lRepo->mBranchName);
		pComplete->addPath(vfsTimeToString(mVersionList.at(pVersionIndex)->mCommitTime));
	}
	if(pRepoPath) {
		*pRepoPath = lRepo->objectName();
	}
	if(pBranchName) {
		*pBranchName = lRepo->mBranchName;
	}
	if(pCommitTime) {
		*pCommitTime = mVersionList.at(pVersionIndex)->mCommitTime;
	}
	if(pPathInRepo) {
		pPathInRepo->clear();
	}
	while(!lStack.isEmpty()) {
		QString lPathComponent = lStack.takeLast()->objectName();
		if(pComplete) {
			pComplete->addPath(lPathComponent);
		}
		if(pPathInRepo) {
			pPathInRepo->append(QLatin1Char('/'));
			pPathInRepo->append(lPathComponent);
		}
	}
}

MergedNodeList &MergedNode::subNodes() {
	if(mSubNodes == NULL) {
		mSubNodes = new MergedNodeList();
		if(S_ISDIR(mMode)) {
			generateSubNodes();
		}
	}
	return *mSubNodes;
}

void MergedNode::generateSubNodes() {
	NameMap lSubNodeMap;
	foreach(VersionData *lCurrentVersion, mVersionList) {
		git_tree *lTree;
		if(0 != git_tree_lookup(&lTree, mRepository, &lCurrentVersion->mOid)) {
			continue;
		}
		git_blob *lMetadataBlob = NULL;
		VintStream *lMetadataStream = NULL;
		const git_tree_entry *lTreeEntry = git_tree_entry_byname(lTree, ".bupm");
		if(lTreeEntry != NULL && 0 == git_blob_lookup(&lMetadataBlob, mRepository, git_tree_entry_id(lTreeEntry))) {
			lMetadataStream = new VintStream(git_blob_rawcontent(lMetadataBlob), git_blob_rawsize(lMetadataBlob), this);
			Metadata lMetadata;
			readMetadata(*lMetadataStream, lMetadata); // the first entry is metadata for the directory itself, discard it.
		}

		uint lEntryCount = git_tree_entrycount(lTree);
		for(uint i = 0; i < lEntryCount; ++i) {
			uint lMode;
			const git_oid *lOid;
			QString lName;
			bool lChunked;
			const git_tree_entry *lTreeEntry = git_tree_entry_byindex(lTree, i);
			getEntryAttributes(lTreeEntry, lMode, lChunked, lOid, lName);
			if(lName == QLatin1String(".bupm")) {
				continue;
			}

			MergedNode *lSubNode = lSubNodeMap.value(lName, NULL);
			if(lSubNode == NULL) {
				lSubNode = new MergedNode(this, lName, lMode);
				lSubNodeMap.insert(lName, lSubNode);
				mSubNodes->append(lSubNode);
			} else if((S_IFMT & lMode) != (S_IFMT & lSubNode->mMode)) {
				if(S_ISDIR(lMode)) {
					lName.append(i18nc("added after folder name in some cases", " (folder)"));
				} else if(S_ISLNK(lMode)) {
					lName.append(i18nc("added after file name in some cases", " (symlink)"));
				} else {
					lName.append(i18nc("added after file name in some cases", " (file)"));
				}
				lSubNode = lSubNodeMap.value(lName, NULL);
				if(lSubNode == NULL) {
					lSubNode = new MergedNode(this, lName, lMode);
					lSubNodeMap.insert(lName, lSubNode);
					mSubNodes->append(lSubNode);
				}
			}
			bool lAlreadySeen = false;
			foreach(VersionData *lVersion, lSubNode->mVersionList) {
				if(lVersion->mOid == *lOid) {
					lAlreadySeen = true;
					break;
				}
			}
			if(S_ISDIR(lMode)) {
				if(!lAlreadySeen) {
					lSubNode->mVersionList.append(new VersionData(lOid, lCurrentVersion->mCommitTime,
					                                              lCurrentVersion->mModifiedDate, 0));
				}
			} else {
				quint64 lModifiedDate;
				Metadata lMetadata;
				if(lMetadataStream != NULL && 0 == readMetadata(*lMetadataStream, lMetadata)) {
					lModifiedDate = lMetadata.mMtime;
				} else {
					lModifiedDate = lCurrentVersion->mModifiedDate;
				}
				if(!lAlreadySeen) {
					lSubNode->mVersionList.append(new VersionData(lChunked, lOid,
					                                              lCurrentVersion->mCommitTime,
					                                              lModifiedDate));
				}
			}
		}
		if(lMetadataStream != NULL) {
			delete lMetadataStream;
			git_blob_free(lMetadataBlob);
		}
		git_tree_free(lTree);
	}
	qSort(mSubNodes->begin(), mSubNodes->end(), mergedNodeLessThan);
	foreach(MergedNode *lNode, *mSubNodes) {
		qSort(lNode->mVersionList.begin(), lNode->mVersionList.end(), versionGreaterThan);
	}
}

MergedRepository::MergedRepository(QObject *pParent, const QString &pRepositoryPath, const QString &pBranchName)
   : MergedNode(pParent, pRepositoryPath, DEFAULT_MODE_DIRECTORY), mBranchName(pBranchName)
{
	if(!objectName().endsWith(QLatin1Char('/'))) {
		setObjectName(objectName() + QLatin1Char('/'));
	}
}

MergedRepository::~MergedRepository() {
	if(mRepository != NULL) {
		git_repository_free(mRepository);
	}
}

bool MergedRepository::open() {
	if(0 != git_repository_open(&mRepository, objectName().toLocal8Bit())) {
		qWarning() << "could not open repository " << objectName();
		mRepository = NULL;
		return false;
	}
	return true;
}

bool MergedRepository::readBranch() {
	if(mRepository == NULL) {
		return false;
	}
	git_revwalk *lRevisionWalker;
	if(0 != git_revwalk_new(&lRevisionWalker, mRepository)) {
		qWarning() << "could not create a revision walker in repository " << objectName();
		return false;
	}

	QString lCompleteBranchName = QString::fromLatin1("refs/heads/");
	lCompleteBranchName.append(mBranchName);
	if(0 != git_revwalk_push_ref(lRevisionWalker, lCompleteBranchName.toLocal8Bit())) {
		qWarning() << "Unable to read branch \"" << mBranchName << "\" in repository " << objectName();
		git_revwalk_free(lRevisionWalker);
		return false;
	}
	bool lEmptyList = true;
	git_oid lOid;
	while(0 == git_revwalk_next(&lOid, lRevisionWalker)) {
		git_commit *lCommit;
		if(0 != git_commit_lookup(&lCommit, mRepository, &lOid)) {
			continue;
		}
		git_time_t lTime = git_commit_time(lCommit);
		mVersionList.append(new VersionData(git_commit_tree_id(lCommit), lTime, lTime, 0));
		lEmptyList = false;
		git_commit_free(lCommit);
	}
	git_revwalk_free(lRevisionWalker);
	return !lEmptyList;
}

uint qHash(git_oid pOid) {
	return qHash(QByteArray::fromRawData((char *)pOid.id, GIT_OID_RAWSZ));
}


bool operator ==(const git_oid &pOidA, const git_oid &pOidB) {
	QByteArray a = QByteArray::fromRawData((char *)pOidA.id, GIT_OID_RAWSZ);
	QByteArray b = QByteArray::fromRawData((char *)pOidB.id, GIT_OID_RAWSZ);
	return a == b;
}


quint64 VersionData::size() {
	if(mSizeIsValid) {
		return mSize;
	}
	if(mChunkedFile) {
		mSize = calculateChunkFileSize(&mOid, MergedNode::mRepository);
	} else {
		git_blob *lBlob;
		if(0 == git_blob_lookup(&lBlob, MergedNode::mRepository, &mOid)) {
			mSize = git_blob_rawsize(lBlob);
			git_blob_free(lBlob);
		} else {
			mSize = 0;
		}
	}
	mSizeIsValid = true;
	return mSize;
}

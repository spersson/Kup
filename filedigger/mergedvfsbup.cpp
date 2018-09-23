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

#include <KLocalizedString>
#include <KMessageBox>

#include <QDBusInterface>
#include <QDir>

#include <git2/branch.h>
#include <sys/stat.h>

#include "mergedvfsbup.h"
#include "vfshelpers.h"
#include "kupdaemon.h"
#include "kupfiledigger_debug.h"

git_repository *MergedNodeBup::mRepository = nullptr;

bool mergedNodeLessThan(const MergedNode *a, const MergedNode *b) {
	if(a->isDirectory() != b->isDirectory()) {
		return a->isDirectory();
	}
	return a->objectName() < b->objectName();
}

bool versionGreaterThan(const VersionData *a, const VersionData *b) {
	return a->mModifiedDate > b->mModifiedDate;
}

MergedNodeBup::MergedNodeBup(QObject *pParent, const QString &pName, uint pMode) :
	MergedNode(pParent, pName, pMode)
{
}

void MergedNodeBup::generateSubNodes()
{
	NameMap lSubNodeMap;
	foreach(VersionData *_lCurrentVersion, mVersionList) {
		VersionDataBup* lCurrentVersion = static_cast<VersionDataBup*>(_lCurrentVersion);
		git_tree *lTree;
		if(0 != git_tree_lookup(&lTree, mRepository, &lCurrentVersion->mOid)) {
			askForIntegrityCheck();
			continue; // try to be fault tolerant by not aborting...
		}
		git_blob *lMetadataBlob = nullptr;
		VintStream *lMetadataStream = nullptr;
		const git_tree_entry *lTreeEntry = git_tree_entry_byname(lTree, ".bupm");
		if(lTreeEntry != nullptr && 0 == git_blob_lookup(&lMetadataBlob, mRepository, git_tree_entry_id(lTreeEntry))) {
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
			if(lName == QStringLiteral(".bupm")) {
				continue;
			}

			MergedNode *lSubNode = lSubNodeMap.value(lName, nullptr);
			if(lSubNode == nullptr) {
				lSubNode = new MergedNodeBup(this, lName, lMode);
				lSubNodeMap.insert(lName, lSubNode);
				mSubNodes->append(lSubNode);
			} else if((S_IFMT & lMode) != (S_IFMT & lSubNode->mMode)) {
				if(S_ISDIR(lMode)) {
					lName.append(xi18nc("added after folder name in some cases", " (folder)"));
				} else if(S_ISLNK(lMode)) {
					lName.append(xi18nc("added after file name in some cases", " (symlink)"));
				} else {
					lName.append(xi18nc("added after file name in some cases", " (file)"));
				}
				lSubNode = lSubNodeMap.value(lName, nullptr);
				if(lSubNode == nullptr) {
					lSubNode = new MergedNodeBup(this, lName, lMode);
					lSubNodeMap.insert(lName, lSubNode);
					mSubNodes->append(lSubNode);
				}
			}
			bool lAlreadySeen = false;
			foreach(VersionData *_lVersion, lSubNode->mVersionList) {
				VersionDataBup *lVersion = static_cast<VersionDataBup*>(_lVersion);
				if(lVersion->mOid == *lOid) {
					lAlreadySeen = true;
					break;
				}
			}
			if(S_ISDIR(lMode)) {
				if(!lAlreadySeen) {
					lSubNode->mVersionList.append(new VersionDataBup(lOid, lCurrentVersion->mCommitTime,
																  lCurrentVersion->mModifiedDate, 0));
				}
			} else {
				quint64 lModifiedDate;
				Metadata lMetadata;
				if(lMetadataStream != nullptr && 0 == readMetadata(*lMetadataStream, lMetadata)) {
					lModifiedDate = lMetadata.mMtime;
				} else {
					lModifiedDate = lCurrentVersion->mModifiedDate;
				}
				if(!lAlreadySeen) {
					lSubNode->mVersionList.append(new VersionDataBup(lChunked, lOid,
																  lCurrentVersion->mCommitTime,
																  lModifiedDate));
				}
			}
		}
		if(lMetadataStream != nullptr) {
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

void MergedNodeBup::getUrl(int pVersionIndex, QUrl *pComplete, QString *pRepoPath, QString *pBranchName, quint64 *pCommitTime, QString *pPathInRepo) const
{
	QList<const MergedNode *> lStack;
	const MergedNode *lNode = this;
	while(lNode != nullptr) {
		lStack.append(lNode);
		lNode = qobject_cast<const MergedNode *>(lNode->parent());
	}
	const MergedRepositoryBup *lRepo = qobject_cast<const MergedRepositoryBup *>(lStack.takeLast()->parent());
	if(pComplete) {
		pComplete->setUrl("bup://" + lRepo->rootNode()->objectName() + '/' + lRepo->mBranchName + '/' +
						  vfsTimeToString(mVersionList.at(pVersionIndex)->mCommitTime));
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
			pComplete->setPath(pComplete->path() + '/' + lPathComponent);
		}
		if(pPathInRepo) {
			pPathInRepo->append(QLatin1Char('/'));
			pPathInRepo->append(lPathComponent);
		}
	}
}

void MergedNodeBup::askForIntegrityCheck()
{
	int lAnswer = KMessageBox::questionYesNo(nullptr, xi18nc("@info messagebox",
															 "Could not read this backup archive. Perhaps some files "
															 "have become corrupted. Do you want to run an integrity "
															 "check to test this?"));
	if(lAnswer == KMessageBox::Yes) {
		QDBusInterface lInterface(KUP_DBUS_SERVICE_NAME, KUP_DBUS_OBJECT_PATH);
		if(lInterface.isValid()) {
			lInterface.call(QStringLiteral("runIntegrityCheck"),
							QDir::cleanPath(QString::fromLocal8Bit(git_repository_path(MergedNodeBup::mRepository))));
		}
	}
}

MergedRepositoryBup::MergedRepositoryBup(QObject *pParent, const QString &pRepositoryPath, const QString &pBranchName)
   : MergedRepository(pParent),
	 mBranchName(pBranchName)
   , mRoot(new MergedNodeBup(this, pRepositoryPath, DEFAULT_MODE_DIRECTORY))
{
	if(!objectName().endsWith(QLatin1Char('/'))) {
		setObjectName(objectName() + QLatin1Char('/'));
	}
}

MergedRepositoryBup::~MergedRepositoryBup() {
	if(MergedNodeBup::mRepository != nullptr) {
		git_repository_free(MergedNodeBup::mRepository);
	}
}

bool MergedRepositoryBup::open() {
	if(0 != git_repository_open(&MergedNodeBup::mRepository, rootNode()->objectName().toLocal8Bit())) {
		qCWarning(KUPFILEDIGGER) << "could not open repository " << objectName();
		MergedNodeBup::mRepository = nullptr;
		return false;
	}
	return true;
}

bool MergedRepositoryBup::readBranch() {
	if(MergedNodeBup::mRepository == nullptr) {
		return false;
	}
	git_revwalk *lRevisionWalker;
	if(0 != git_revwalk_new(&lRevisionWalker, MergedNodeBup::mRepository)) {
		qCWarning(KUPFILEDIGGER) << "could not create a revision walker in repository " << objectName();
		return false;
	}

	QString lCompleteBranchName = QStringLiteral("refs/heads/");
	lCompleteBranchName.append(mBranchName);
	if(0 != git_revwalk_push_ref(lRevisionWalker, lCompleteBranchName.toLocal8Bit())) {
		qCWarning(KUPFILEDIGGER) << "Unable to read branch " << mBranchName << " in repository " << objectName();
		git_revwalk_free(lRevisionWalker);
		return false;
	}
	bool lEmptyList = true;
	git_oid lOid;
	while(0 == git_revwalk_next(&lOid, lRevisionWalker)) {
		git_commit *lCommit;
		if(0 != git_commit_lookup(&lCommit, MergedNodeBup::mRepository, &lOid)) {
			continue;
		}
		git_time_t lTime = git_commit_time(lCommit);
		rootNode()->mVersionList.append(new VersionDataBup(git_commit_tree_id(lCommit), lTime, lTime, 0));
		lEmptyList = false;
		git_commit_free(lCommit);
	}
	git_revwalk_free(lRevisionWalker);
	return !lEmptyList;
}

bool MergedRepositoryBup::permissionsOk() {
	if(MergedNodeBup::mRepository == nullptr) {
		return false;
	}
	QDir lRepoDir(objectName());
	if(!lRepoDir.exists()) {
		return false;
	}
	QList<QDir> lDirectories;
	lDirectories << lRepoDir;
	while(!lDirectories.isEmpty()) {
		QDir lDir = lDirectories.takeFirst();
		foreach(QFileInfo lFileInfo, lDir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
			if(!lFileInfo.isReadable()) {
				return false;
			}
			if(lFileInfo.isDir()) {
				lDirectories << QDir(lFileInfo.absoluteFilePath());
			}
		}
	}
	return true;
}

MergedNode *MergedRepositoryBup::rootNode() const
{
	return mRoot;
}

uint qHash(git_oid pOid) {
	return qHash(QByteArray::fromRawData((char *)pOid.id, GIT_OID_RAWSZ));
}


bool operator ==(const git_oid &pOidA, const git_oid &pOidB) {
	QByteArray a = QByteArray::fromRawData((char *)pOidA.id, GIT_OID_RAWSZ);
	QByteArray b = QByteArray::fromRawData((char *)pOidB.id, GIT_OID_RAWSZ);
	return a == b;
}

quint64 VersionDataBup::size()
{
	if(mSizeIsValid) {
		return mSize;
	}
	if(mChunkedFile) {
		mSize = calculateChunkFileSize(&mOid, MergedNodeBup::mRepository);
	} else {
		git_blob *lBlob;
		if(0 == git_blob_lookup(&lBlob, MergedNodeBup::mRepository, &mOid)) {
			mSize = git_blob_rawsize(lBlob);
			git_blob_free(lBlob);
		} else {
			mSize = 0;
		}
	}
	mSizeIsValid = true;
	return mSize;
}

#include "bupvfs.h"

#include <git2/blob.h>
#include <git2/branch.h>

#include <sys/stat.h>

#include <QDebug>
#include <QMimeDatabase>

git_revwalk *Node::mRevisionWalker = NULL;
git_repository *Node::mRepository = NULL;

Node::Node(QObject *pParent, const QString &pName, quint64 pMode)
   :QObject(pParent), Metadata(pMode)
{
	setObjectName(pName);
}

int Node::readMetadata(VintStream &pMetadataStream) {
	return ::readMetadata(pMetadataStream, *this);
}

Node *Node::resolve(const QString &pPath, bool pFollowLinks) {
	Node *lParentNode = this;
	QString lTarget = pPath;
	if(lTarget.startsWith(QLatin1Char('/'))) {
		lTarget.remove(0, 1);
		lParentNode = parentCommit();
	}
	return lParentNode->resolve(lTarget.split(QLatin1Char('/'), QString::SkipEmptyParts), pFollowLinks);
}

Node *Node::resolve(const QStringList &pPathList, bool pFollowLinks) {
	Node *lNode = this;
	foreach(QString lPathComponent, pPathList) {
		if(lPathComponent == QLatin1String(".")) {
			continue;
		} else if(lPathComponent == QLatin1String("..")) {
			lNode = qobject_cast<Node *>(lNode->parent());
		} else {
			Directory *lDir = qobject_cast<Directory *>(lNode);
			if(lDir == NULL) {
				return NULL;
			}
			lNode = lDir->subNodes().value(lPathComponent, NULL);
		}
		if(lNode == NULL) {
			return NULL;
		}
	}
	if(pFollowLinks && !lNode->mSymlinkTarget.isEmpty()) {
		return qobject_cast<Node *>(lNode->parent())->resolve(lNode->mSymlinkTarget, true);
	}
	return lNode;
}

QString Node::completePath() {
	QString lCompletePath;
	Node *lNode = this;
	while(lNode != NULL) {
		Node *lNewNode = qobject_cast<Node *>(lNode->parent());
		if(lNewNode	== NULL) { //this must be the repository, already starts and ends with slash.
			QString lObjectName = lNode->objectName();
			lObjectName.chop(1);
			lCompletePath.prepend(lObjectName);
		} else {
			lCompletePath.prepend(lNode->objectName());
			lCompletePath.prepend(QLatin1String("/"));
		}
		lNode = lNewNode;
	}
	return lCompletePath;
}

Node *Node::parentCommit() {
	Node *lNode = this;
	while(lNode != NULL && qobject_cast<Branch *>(lNode->parent()) == NULL) {
		lNode = qobject_cast<Node *>(lNode->parent());
	}
	return lNode;
}

//Node *Node::parentRepository() {
//	Node *lNode = this;
//	while(lNode->parent() != NULL && qobject_cast<Repository *>(lNode) == NULL) {
//		lNode = qobject_cast<Node *>(lNode->parent());
//	}
//	return lNode;
//}

Directory::Directory(QObject *pParent, const QString &pName, quint64 pMode)
   :Node(pParent, pName, pMode)
{
	mSubNodes = NULL;
	mMimeType = QString::fromLatin1("inode/directory");
}

NodeMap Directory::subNodes() {
	if(mSubNodes == NULL) {
		mSubNodes = new NodeMap();
		generateSubNodes();
	}
	return *mSubNodes;
}

int File::readMetadata(VintStream &pMetadataStream) {
	int lRetVal = Node::readMetadata(pMetadataStream);
	QByteArray lContent, lNextData;
	seek(0);
	while(lContent.size() < 1000 && 0 == read(lNextData)) {
		lContent.append(lNextData);
	}
	seek(0);
	QMimeDatabase db;
	if(!lContent.isEmpty()) {
		mMimeType = db.mimeTypeForFileNameAndData(objectName(), lContent).name();
	} else {
		mMimeType = db.mimeTypeForFile(objectName()).name();
	}
	return lRetVal;
}

BlobFile::BlobFile(Node *pParent, const git_oid *pOid, const QString &pName, quint64 pMode)
   : File(pParent, pName, pMode)
{
	mOid = *pOid;
	mBlob = NULL;
}

BlobFile::~BlobFile() {
	if(mBlob != NULL) {
		git_blob_free(mBlob);
	}
}

int BlobFile::read(QByteArray &pChunk, int pReadSize) {
	if(mOffset >= size()) {
		return KIO::ERR_NO_CONTENT;
	}
	git_blob *lBlob = cachedBlob();
	if(lBlob == NULL) {
		return KIO::ERR_COULD_NOT_READ;
	}
	int lAvailableSize = size() - mOffset;
	int lReadSize = lAvailableSize;
	if(pReadSize > 0 && pReadSize < lAvailableSize) {
		lReadSize = pReadSize;
	}
	pChunk = QByteArray::fromRawData(((char *)git_blob_rawcontent(lBlob)) + mOffset, lReadSize);
	mOffset += lReadSize;
	return 0;
}

git_blob *BlobFile::cachedBlob() {
	if(mBlob == NULL) {
		git_blob_lookup(&mBlob, mRepository, &mOid);
	}
	return mBlob;
}

quint64 BlobFile::calculateSize() {
	git_blob *lBlob = cachedBlob();
	if(lBlob == NULL) {
		return 0;
	}
	return git_blob_rawsize(lBlob);
}

ChunkFile::ChunkFile(Node *pParent, const git_oid *pOid, const QString &pName, quint64 pMode)
   : File(pParent, pName, pMode)
{
	mOid = *pOid;
	mValidSeekPosition = false;
	mCurrentBlob = NULL;
	seek(0);
}

ChunkFile::~ChunkFile() {
	if(mCurrentBlob != NULL) {
		git_blob_free(mCurrentBlob);
	}
}

int ChunkFile::seek(quint64 pOffset) {
	if(pOffset >= size()) {
		return KIO::ERR_COULD_NOT_SEEK;
	}
	if(mOffset == pOffset && mValidSeekPosition) {
		return 0; // nothing to do, success
	}

	mOffset = pOffset;
	mValidSeekPosition = false;

	while(!mPositionStack.isEmpty()) {
		delete mPositionStack.takeLast();
	}
	if(mCurrentBlob != NULL) {
		git_blob_free(mCurrentBlob);
		mCurrentBlob = NULL;
	}

	git_tree *lTree;
	if(0 != git_tree_lookup(&lTree, mRepository, &mOid)) {
		return KIO::ERR_COULD_NOT_SEEK;
	}

	TreePosition *lCurrentPos = new TreePosition(lTree);
	mPositionStack.append(lCurrentPos);
	quint64 lLocalOffset = mOffset;
	while(true) {
		uint lLower = 0;
		const git_tree_entry *lLowerEntry = git_tree_entry_byindex(lCurrentPos->mTree, lLower);
		uint lLowerOffset = 0;
		uint lUpper = git_tree_entrycount(lCurrentPos->mTree);

		while(lUpper - lLower > 1) {
			uint lToCheck = lLower + (lUpper - lLower)/2;
			const git_tree_entry *lCheckEntry = git_tree_entry_byindex(lCurrentPos->mTree, lToCheck);
			quint64 lCheckOffset;
			if(!offsetFromName(lCheckEntry, lCheckOffset)) {
				return KIO::ERR_COULD_NOT_SEEK;
			}
			if(lCheckOffset > lLocalOffset) {
				lUpper = lToCheck;
			} else {
				lLower = lToCheck;
				lLowerEntry = lCheckEntry;
				lLowerOffset = lCheckOffset;
			}
		}
		lCurrentPos->mIndex = lLower;
		// the remainder of the offset will be a local offset into the blob or into the subtree.
		lLocalOffset -= lLowerOffset;

		if(S_ISDIR(git_tree_entry_filemode(lLowerEntry))) {
			git_tree *lTree;
			if(0 != git_tree_lookup(&lTree, mRepository, git_tree_entry_id(lLowerEntry))) {
				return KIO::ERR_COULD_NOT_SEEK;
			}
			lCurrentPos = new TreePosition(lTree);
			mPositionStack.append(lCurrentPos);
		} else {
			lCurrentPos->mSkipSize = lLocalOffset;
			break;
		}
	}
	mValidSeekPosition = true;
	return 0; // success.
}

int ChunkFile::read(QByteArray &pChunk, int pReadSize) {
	if(mOffset >= size()) {
		return KIO::ERR_NO_CONTENT;
	}
	if(!mValidSeekPosition) {
		return KIO::ERR_COULD_NOT_READ;
	}

	TreePosition *lCurrentPos = mPositionStack.last();
	if(mCurrentBlob != NULL && lCurrentPos->mSkipSize == 0) {
		// skipsize has been reset, this means current blob has been exhausted. Free it
		// now as we're about to fetch a new one.
		git_blob_free(mCurrentBlob);
		mCurrentBlob = NULL;
	}

	if(mCurrentBlob == NULL) {
		const git_tree_entry *lTreeEntry = git_tree_entry_byindex(lCurrentPos->mTree, lCurrentPos->mIndex);
		if(0 != git_blob_lookup(&mCurrentBlob, mRepository, git_tree_entry_id(lTreeEntry))) {
			return KIO::ERR_COULD_NOT_READ;
		}
	}

	int lTotalSize = git_blob_rawsize(mCurrentBlob);
	int lAvailableSize = lTotalSize - lCurrentPos->mSkipSize;
	if(lAvailableSize < 0) { // this must mean a corrupt bup tree somehow
		return KIO::ERR_COULD_NOT_READ;
	}
	int lReadSize = lAvailableSize;
	if(pReadSize > 0 && pReadSize < lAvailableSize) {
		lReadSize = pReadSize;
	}
	pChunk = QByteArray::fromRawData(((char *)git_blob_rawcontent(mCurrentBlob)) + lCurrentPos->mSkipSize, lReadSize);
	mOffset += lReadSize;
	lCurrentPos->mSkipSize += lReadSize;

	// check if it's time to find next blob.
	if(lCurrentPos->mSkipSize == lTotalSize) {
		lCurrentPos->mSkipSize = 0;
		lCurrentPos->mIndex++;
		while(true) {
			if(lCurrentPos->mIndex < git_tree_entrycount(lCurrentPos->mTree)) {
				const git_tree_entry *lTreeEntry = git_tree_entry_byindex(lCurrentPos->mTree, lCurrentPos->mIndex);
				if(S_ISDIR(git_tree_entry_filemode(lTreeEntry))) {
					git_tree *lTree;
					if(0 != git_tree_lookup(&lTree, mRepository, git_tree_entry_id(lTreeEntry))) {
						return KIO::ERR_COULD_NOT_READ;
					}
					lCurrentPos = new TreePosition(lTree); // will have index and skipsize initialized to zero.
					mPositionStack.append(lCurrentPos);
				} else {
					// it's a blob
					break;
				}
			} else {
				delete mPositionStack.takeLast();
				if(mPositionStack.isEmpty()) {
					Q_ASSERT(mOffset == size());
					break;
				}
				lCurrentPos = mPositionStack.last();
				lCurrentPos->mIndex++;
			}
		}
	}
	return 0; // success.
}

quint64 ChunkFile::calculateSize() {
	return calculateChunkFileSize(&mOid, mRepository);
}

ChunkFile::TreePosition::TreePosition(git_tree *pTree) {
	mTree = pTree;
	mIndex = 0;
	mSkipSize = 0;
}

ChunkFile::TreePosition::~TreePosition() {
	git_tree_free(mTree);
}

ArchivedDirectory::ArchivedDirectory(Node *pParent, const git_oid *pOid, const QString &pName, quint64 pMode)
   : Directory(pParent, pName, pMode)
{
	mOid = *pOid;
	mMetadataStream = NULL;
	mTree = NULL;
	if(0 != git_tree_lookup(&mTree, mRepository, &mOid)) {
		return;
	}
	const git_tree_entry *lTreeEntry = git_tree_entry_byname(mTree, ".bupm");
	if(lTreeEntry != NULL && 0 == git_blob_lookup(&mMetadataBlob, mRepository, git_tree_entry_id(lTreeEntry))) {
		mMetadataStream = new VintStream(git_blob_rawcontent(mMetadataBlob), git_blob_rawsize(mMetadataBlob), this);
		readMetadata(*mMetadataStream); // the first entry is metadata for the directory itself
	}
}

void ArchivedDirectory::generateSubNodes() {
	if(mTree == NULL) {
		return;
	}
	uint lEntryCount = git_tree_entrycount(mTree);
	for(uint i = 0; i < lEntryCount; ++i) {
		uint lMode;
		const git_oid *lOid;
		QString lName;
		bool lChunked;
		const git_tree_entry *lTreeEntry = git_tree_entry_byindex(mTree, i);
		getEntryAttributes(lTreeEntry, lMode, lChunked, lOid, lName);
		if(lName == QLatin1String(".bupm")) {
			continue;
		}

		Node *lSubNode = NULL;
		if(S_ISDIR(lMode)) {
			lSubNode = new ArchivedDirectory(this, lOid, lName, lMode);
		} else if(S_ISLNK(lMode)) {
			lSubNode = new Symlink(this, lOid, lName, lMode);
		} else if(lChunked) {
			lSubNode = new ChunkFile(this, lOid, lName, lMode);
		} else {
			lSubNode = new BlobFile(this, lOid, lName, lMode);
		}
		mSubNodes->insert(lName, lSubNode);
		if(!S_ISDIR(lMode) && mMetadataStream != NULL) {
			lSubNode->readMetadata(*mMetadataStream);
		}
	}
	if(mMetadataStream != NULL) {
		delete mMetadataStream;
		mMetadataStream = NULL;
		git_blob_free(mMetadataBlob);
		mMetadataBlob = NULL;
	}
	git_tree_free(mTree);
	mTree = NULL;
}

Branch::Branch(Node *pParent, const char *pName)
   : Directory(pParent, QString::fromLocal8Bit(pName).remove(0, 11), DEFAULT_MODE_DIRECTORY)
{
	mRefName = QByteArray(pName);
	QByteArray lPath = parent()->objectName().toLocal8Bit();
	lPath.append(mRefName);
	struct stat lStat;
	if(0 == stat(lPath, &lStat)) {
		mAtime = lStat.st_atime;
		mMtime = lStat.st_mtime;
	}
}

void Branch::reload() {
	if(mSubNodes == NULL) {
		mSubNodes = new NodeMap();
	}
	// potentially changed content in a branch, generateSubNodes is written so
	// that it can be called repetedly.
	generateSubNodes();
}

void Branch::generateSubNodes() {
	if(0 != git_revwalk_push_ref(mRevisionWalker, mRefName)) {
		return;
	}
	git_oid lOid;
	while(0 == git_revwalk_next(&lOid, mRevisionWalker)) {
		git_commit *lCommit;
		if(0 != git_commit_lookup(&lCommit, mRepository, &lOid)) {
			continue;
		}
		QString lCommitTimeLocal = vfsTimeToString(git_commit_time(lCommit));
		if(!mSubNodes->contains(lCommitTimeLocal)) {
			Directory * lDirectory = new ArchivedDirectory(this, git_commit_tree_id(lCommit),
			                                               lCommitTimeLocal, DEFAULT_MODE_DIRECTORY);
			lDirectory->mMtime = git_commit_time(lCommit);
			mSubNodes->insert(lCommitTimeLocal, lDirectory);
		}
		git_commit_free(lCommit);
	}
}

Repository::Repository(QObject *pParent, const QString &pRepositoryPath)
   : Directory(pParent, pRepositoryPath, DEFAULT_MODE_DIRECTORY)
{
	if(!objectName().endsWith(QLatin1Char('/'))) {
		setObjectName(objectName() + QLatin1Char('/'));
	}
	if(0 != git_repository_open(&mRepository, pRepositoryPath.toLocal8Bit())) {
		qWarning() << "could not open repository " << pRepositoryPath;
		mRepository = NULL;
		return;
	}
	git_strarray lBranchNames;
	git_reference_list(&lBranchNames, mRepository);
	for(uint i = 0; i < lBranchNames.count; ++i) {
		QString lRefName = QString::fromLocal8Bit(lBranchNames.strings[i]);
		if(lRefName.startsWith(QLatin1String("refs/heads/"))) {
			QString lPath = objectName();
			lPath.append(lRefName);
			struct stat lStat;
			stat(lPath.toLocal8Bit(), &lStat);
			if(lStat.st_atime > mAtime) {
				mAtime = lStat.st_atime;
			}
			if(lStat.st_mtime > mMtime) {
				mMtime = lStat.st_mtime;
			}
		}
	}
	git_strarray_free(&lBranchNames);

	if(0 != git_revwalk_new(&mRevisionWalker, mRepository)) {
		qWarning() << "could not create a revision walker in repository " << pRepositoryPath;
		mRevisionWalker = NULL;
		return;
	}
}

Repository::~Repository() {
	if(mRepository != NULL) {
		git_repository_free(mRepository);
	}
	if(mRevisionWalker != NULL) {
		git_revwalk_free(mRevisionWalker);
	}
}

void Repository::generateSubNodes() {
	git_strarray lBranchNames;
	git_reference_list(&lBranchNames, mRepository);
	for(uint i = 0; i < lBranchNames.count; ++i) {
		QString lRefName = QString::fromLocal8Bit(lBranchNames.strings[i]);
		if(lRefName.startsWith(QLatin1String("refs/heads/"))) {
			Branch *lBranch = new Branch(this, lBranchNames.strings[i]);
			mSubNodes->insert(lBranch->objectName(), lBranch);
		}
	}
	git_strarray_free(&lBranchNames);
}



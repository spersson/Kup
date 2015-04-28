#include "bupvfs.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QVarLengthArray>

#include <KIO/SlaveBase>
using namespace KIO;
#include <KLocalizedString>
#include <KProcess>

#include <grp.h>
#include <pwd.h>

class BupSlave : public SlaveBase
{
public:
	BupSlave(const QByteArray &pPoolSocket, const QByteArray &pAppSocket);
	virtual ~BupSlave();
	virtual void close();
	virtual void get(const QUrl &pUrl);
	virtual void listDir(const QUrl &pUrl) ;
	virtual void open(const QUrl &pUrl, QIODevice::OpenMode pMode);
	virtual void read(filesize_t pSize);
	virtual void seek(filesize_t pOffset);
	virtual void stat(const QUrl &pUrl);
	virtual void mimetype(const QUrl &pUrl);

private:
	bool checkCorrectRepository(const QUrl &pUrl, QStringList &pPathInRepository);
	QString getUserName(uid_t pUid);
	QString getGroupName(gid_t pGid);
	void createUDSEntry(Node *pNode, KIO::UDSEntry & pUDSEntry, int pDetails);

	QHash<uid_t, QString> mUsercache;
	QHash<gid_t, QString> mGroupcache;
	Repository *mRepository;
	File *mOpenFile;
};

BupSlave::BupSlave(const QByteArray &pPoolSocket, const QByteArray &pAppSocket)
   : SlaveBase("bup", pPoolSocket, pAppSocket)
{
	mRepository = NULL;
	mOpenFile = NULL;
	git_threads_init();
}

BupSlave::~BupSlave() {
	if(mRepository != NULL) {
		delete mRepository;
	}
	git_threads_shutdown();
}

void BupSlave::close() {
	mOpenFile = NULL;
	emit finished();
}

void BupSlave::get(const QUrl &pUrl) {
	QStringList lPathInRepo;
	if(!checkCorrectRepository(pUrl, lPathInRepo)) {
		emit error(KIO::ERR_SLAVE_DEFINED, i18n("No bup repository found.\n%1", pUrl.toDisplayString()));
		return;
	}

	// Assume that a symlink should be followed.
	// Kio will never call get() on a symlink if it actually wants to copy a
	// symlink, it would just create a symlink on the destination kioslave using the
	// target it already got from calling stat() on this one.
	Node *lNode = mRepository->resolve(lPathInRepo, true);
	if(lNode == NULL) {
		emit error(KIO::ERR_DOES_NOT_EXIST, lPathInRepo.join(QStringLiteral("/")));
		return;
	}
	File *lFile = qobject_cast<File *>(lNode);
	if(lFile == NULL) {
		emit error(KIO::ERR_IS_DIRECTORY, lPathInRepo.join(QStringLiteral("/")));
		return;
	}

	emit mimeType(lFile->mMimeType);
	// Emit total size AFTER mimetype
	emit totalSize(lFile->size());

	//make sure file is at the beginning
	lFile->seek(0);
	KIO::filesize_t lProcessedSize = 0;
	const QString lResumeOffset = metaData(QStringLiteral("resume"));
	if(!lResumeOffset.isEmpty()) {
		bool ok;
		KIO::fileoffset_t lOffset = lResumeOffset.toLongLong(&ok);
		if (ok && (lOffset > 0) && ((quint64)lOffset < lFile->size())) {
			if(0 == lFile->seek(lOffset)) {
				emit canResume();
				lProcessedSize = lOffset;
			}
		}
	}

	QByteArray lResultArray;
	int lRetVal;
	while(0 == (lRetVal = lFile->read(lResultArray))) {
		emit data(lResultArray);
		lProcessedSize += lResultArray.length();
		emit processedSize(lProcessedSize);
	}
	if(lRetVal == KIO::ERR_NO_CONTENT) {
		emit data(QByteArray());
		emit processedSize(lProcessedSize);
		emit finished();
	} else {
		emit error(lRetVal, lPathInRepo.join(QStringLiteral("/")));
	}
}

void BupSlave::listDir(const QUrl &pUrl) {
	QStringList lPathInRepo;
	if(!checkCorrectRepository(pUrl, lPathInRepo)) {
		emit error(KIO::ERR_SLAVE_DEFINED, i18n("No bup repository found.\n%1", pUrl.toDisplayString()));
		return;
	}
	Node *lNode = mRepository->resolve(lPathInRepo, true);
	if(lNode == NULL) {
		emit error(KIO::ERR_DOES_NOT_EXIST, lPathInRepo.join(QStringLiteral("/")));
		return;
	}
	Directory *lDir = qobject_cast<Directory *>(lNode);
	if(lDir == NULL) {
		emit error(KIO::ERR_IS_FILE, lPathInRepo.join(QStringLiteral("/")));
		return;
	}

	// give the directory a chance to reload if necessary.
	lDir->reload();

	const QString sDetails = metaData(QStringLiteral("details"));
	const int lDetails = sDetails.isEmpty() ? 2 : sDetails.toInt();

	NodeMapIterator i(lDir->subNodes());
	UDSEntry lEntry;
	while(i.hasNext()) {
		createUDSEntry(i.next().value(), lEntry, lDetails);
		emit listEntry(lEntry);
	}
	emit finished();
}

void BupSlave::open(const QUrl &pUrl, QIODevice::OpenMode pMode) {
	if(pMode & QIODevice::WriteOnly) {
		emit error(KIO::ERR_CANNOT_OPEN_FOR_WRITING, pUrl.toDisplayString());
		return;
	}

	QStringList lPathInRepo;
	if(!checkCorrectRepository(pUrl, lPathInRepo)) {
		emit error(KIO::ERR_SLAVE_DEFINED, i18n("No bup repository found.\n%1", pUrl.toDisplayString()));
		return;
	}

	Node *lNode = mRepository->resolve(lPathInRepo, true);
	if(lNode == NULL) {
		emit error(KIO::ERR_DOES_NOT_EXIST, lPathInRepo.join(QStringLiteral("/")));
		return;
	}

	File *lFile = qobject_cast<File *>(lNode);
	if(lFile == NULL) {
		emit error(KIO::ERR_IS_DIRECTORY, lPathInRepo.join(QStringLiteral("/")));
		return;
	}

	if(0 != lFile->seek(0)) {
		emit error(KIO::ERR_CANNOT_OPEN_FOR_READING, pUrl.toDisplayString());
		return;
	}

	mOpenFile = lFile;
	emit mimeType(lFile->mMimeType);
	emit totalSize(lFile->size());
	emit position(0);
	emit opened();
}

void BupSlave::read(filesize_t pSize) {
	if(mOpenFile == NULL) {
		emit error(KIO::ERR_COULD_NOT_READ, QString());
		return;
	}
	QByteArray lResultArray;
	int lRetVal = 0;
	while(pSize > 0 && 0 == (lRetVal = mOpenFile->read(lResultArray, pSize))) {
		pSize -= lResultArray.size();
		emit data(lResultArray);
	}
	if(lRetVal == 0) {
		emit data(QByteArray());
		emit finished();
	} else {
		emit error(lRetVal, mOpenFile->completePath());
	}
}

void BupSlave::seek(filesize_t pOffset) {
	if(mOpenFile == NULL) {
		emit error(KIO::ERR_COULD_NOT_SEEK, QString());
		return;
	}

	if(0 != mOpenFile->seek(pOffset)) {
		emit error(KIO::ERR_COULD_NOT_SEEK, mOpenFile->completePath());
		return;
	}
	emit position(pOffset);
}

void BupSlave::stat(const QUrl &pUrl) {
	QStringList lPathInRepo;
	if(!checkCorrectRepository(pUrl, lPathInRepo)) {
		emit error(KIO::ERR_SLAVE_DEFINED, i18n("No bup repository found.\n%1", pUrl.toDisplayString()));
		return;
	}

	Node *lNode = mRepository->resolve(lPathInRepo);
	if(lNode == NULL) {
		emit error(KIO::ERR_DOES_NOT_EXIST, lPathInRepo.join(QStringLiteral("/")));
		return;
	}

	const QString sDetails = metaData(QStringLiteral("details"));
	const int lDetails = sDetails.isEmpty() ? 2 : sDetails.toInt();

	UDSEntry lUDSEntry;
	createUDSEntry(lNode, lUDSEntry, lDetails);
	emit statEntry(lUDSEntry);
	emit finished();
}

void BupSlave::mimetype(const QUrl &pUrl) {
	QStringList lPathInRepo;
	if(!checkCorrectRepository(pUrl, lPathInRepo)) {
		emit error(KIO::ERR_SLAVE_DEFINED, i18n("No bup repository found.\n%1", pUrl.toDisplayString()));
		return;
	}

	Node *lNode = mRepository->resolve(lPathInRepo);
	if(lNode == NULL) {
		emit error(KIO::ERR_DOES_NOT_EXIST, lPathInRepo.join(QStringLiteral("/")));
		return;
	}

	emit mimeType(lNode->mMimeType);
	emit finished();
}

bool BupSlave::checkCorrectRepository(const QUrl &pUrl, QStringList &pPathInRepository) {
	// make this slave accept most URLs.. even incorrect ones. (no slash (wrong),
	// one slash (correct), two slashes (wrong), three slashes (correct))
	QString lPath;
	if(!pUrl.host().isEmpty()) {
		lPath = QStringLiteral("/") + pUrl.host() + pUrl.adjusted(QUrl::StripTrailingSlash).path() + '/';
	} else {
		lPath = pUrl.adjusted(QUrl::StripTrailingSlash).path() + '/';
		if(!lPath.startsWith(QLatin1Char('/'))) {
			lPath.prepend(QLatin1Char('/'));
		}
	}

	if(mRepository && mRepository->isValid()) {
		if(lPath.startsWith(mRepository->objectName())) {
			lPath.remove(0, mRepository->objectName().length());
			pPathInRepository = lPath.split(QLatin1Char('/'), QString::SkipEmptyParts);
			return true;
		}
		else {
			delete mRepository;
			mRepository = NULL;
		}
	}

	pPathInRepository = lPath.split(QLatin1Char('/'), QString::SkipEmptyParts);
	QString lRepoPath = QString::fromLatin1("/");
	while(!pPathInRepository.isEmpty()) {
		// make sure the repo path will end with a slash
		lRepoPath += pPathInRepository.takeFirst();
		lRepoPath += QStringLiteral("/");
		if((QFile::exists(lRepoPath + QStringLiteral("objects")) &&
		    QFile::exists(lRepoPath + QStringLiteral("refs"))) ||
		      (QFile::exists(lRepoPath + QStringLiteral(".git/objects")) &&
		       QFile::exists(lRepoPath + QStringLiteral(".git/refs")))) {
			mRepository = new Repository(NULL, lRepoPath);
			return mRepository->isValid();
		}
	}
	return false;
}

QString BupSlave::getUserName(uid_t pUid) {
	if(!mUsercache.contains(pUid)) {
		struct passwd *lUserInfo = getpwuid(pUid);
		if(lUserInfo) {
			mUsercache.insert(pUid, QString::fromLocal8Bit(lUserInfo->pw_name));
		}
		else {
			return QString::number(pUid);
		}
	}
	return mUsercache.value(pUid);
}

QString BupSlave::getGroupName(gid_t pGid) {
	if(!mGroupcache.contains(pGid)) {
		struct group *lGroupInfo = getgrgid(pGid);
		if(lGroupInfo) {
			mGroupcache.insert(pGid, QString::fromLocal8Bit(lGroupInfo->gr_name));
		}
		else {
			return QString::number( pGid );
		}
	}
	return mGroupcache.value(pGid);
}

void BupSlave::createUDSEntry(Node *pNode, UDSEntry &pUDSEntry, int pDetails) {
	pUDSEntry.clear();
	pUDSEntry.insert(KIO::UDSEntry::UDS_NAME, pNode->objectName());
	if(!pNode->mSymlinkTarget.isEmpty()) {
		pUDSEntry.insert(KIO::UDSEntry::UDS_LINK_DEST, pNode->mSymlinkTarget);
		if(pDetails > 1) {
			Node *lNode = qobject_cast<Node *>(pNode->parent())->resolve(pNode->mSymlinkTarget, true);
			if(lNode != NULL) { // follow symlink only if details > 1 and it leads to something
				pNode = lNode;
			}
		}
	}
	pUDSEntry.insert(KIO::UDSEntry::UDS_FILE_TYPE, pNode->mMode & S_IFMT);
	pUDSEntry.insert(KIO::UDSEntry::UDS_ACCESS, pNode->mMode & 07777);
	if(pDetails > 0) {
		qint64 lSize = 0;
		File *lFile = qobject_cast<File *>(pNode);
		if(lFile != NULL) {
			lSize = lFile->size();
		}
		pUDSEntry.insert(KIO::UDSEntry::UDS_SIZE, lSize);
		pUDSEntry.insert(KIO::UDSEntry::UDS_MIME_TYPE, pNode->mMimeType);
		pUDSEntry.insert(KIO::UDSEntry::UDS_ACCESS_TIME, pNode->mAtime);
		pUDSEntry.insert(KIO::UDSEntry::UDS_MODIFICATION_TIME, pNode->mMtime);
		pUDSEntry.insert(KIO::UDSEntry::UDS_USER, getUserName(pNode->mUid));
		pUDSEntry.insert(KIO::UDSEntry::UDS_GROUP, getGroupName(pNode->mGid));
	}
}

extern "C" int Q_DECL_EXPORT kdemain(int pArgc, char **pArgv) {
	QCoreApplication lApp(pArgc, pArgv);
	lApp.setApplicationName(QStringLiteral("kio_bup"));
	if(pArgc != 4) {
		fprintf(stderr, "Usage: kio_bup protocol domain-socket1 domain-socket2\n");
		exit(-1);
	}

	BupSlave lSlave(pArgv[2], pArgv[3]);
	lSlave.dispatchLoop();

	return 0;
}


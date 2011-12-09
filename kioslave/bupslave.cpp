
#include "bupslave.h"

#include <QtCore/QCoreApplication>
//#include <QtCore/QBuffer>
//#include <QtCore/QByteArray>
//#include <QtCore/QDir>
//#include <QtCore/QFile>
//#include <QtCore/QObject>
//#include <QtCore/QString>
//#include <QtCore/QVarLengthArray>

//#include <stdlib.h>
//#include <unistd.h>
//#include <errno.h>
//#include <time.h>
//#include <string.h>

//#include <sys/time.h>
//#include <sys/stat.h>
//#include <sys/types.h>

//#include <kapplication.h>
//#include <kuser.h>
#include <kdebug.h>
//#include <kmessagebox.h>
#include <KComponentData>
//#include <kglobal.h>
//#include <kstandarddirs.h>
//#include <klocale.h>
//#include <kurl.h>
//#include <kio/ioslave_defaults.h>
//#include <kmimetype.h>
//#include <kde_file.h>
//#include <kconfiggroup.h>


#include <dirent.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>


#include <kde_file.h>
#include <KMimeType>
#include <KProcess>
#include <KTempDir>

//#include <kio/global.h>
#include <QDebug>
#include <QFile>

#define MAX_IPC_SIZE (1024*32)

using namespace KIO;
extern "C"
{
int KDE_EXPORT kdemain(int pArgc, char **pArgv ) {
	QCoreApplication app(pArgc, pArgv);
	KComponentData componentData("kio_bup");
	(void) KGlobal::locale();

	if(pArgc != 4) {
		fprintf(stderr, "Usage: kio_bup protocol domain-socket1 domain-socket2\n");
		exit(-1);
	}

	BupSlave lSlave(pArgv[2], pArgv[3]);
	lSlave.dispatchLoop();

	return 0;
}
}


FuseRunner::FuseRunner(const QString &pRepoPath) {
	mRepoPath = pRepoPath;
	mTempDir = new KTempDir();
	mMountPath = mTempDir->name();
	mProcess << QLatin1String("bup") << QLatin1String("-d") << mRepoPath;
	mProcess << QLatin1String("fuse") << QLatin1String("-f") << mMountPath;

	connect(&mProcess, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(fuseFinished(int,QProcess::ExitStatus)));
	mProcess.start();
	mProcess.waitForStarted();
	mProcess.waitForReadyRead(500);
	mRunning = true;
}

FuseRunner::~FuseRunner() {
	if(mRunning) {
		KProcess lUnmount;
		lUnmount << QLatin1String("fusermount") << QLatin1String("-u") << mMountPath;
		lUnmount.execute();

		if(!mProcess.waitForFinished(500)) {
			mProcess.terminate();
			if(!mProcess.waitForFinished(500)) {
				mProcess.kill();
				mProcess.waitForFinished();
			}
		}
	}
	delete mTempDir;
}

void FuseRunner::fuseFinished(int pExitCode, QProcess::ExitStatus pExitStatus) {
	Q_UNUSED(pExitCode)
	Q_UNUSED(pExitStatus)
	mRunning = false;
}

BupSlave::BupSlave(const QByteArray &pPoolSocket, const QByteArray &pAppSocket)
   : SlaveBase("bup", pPoolSocket, pAppSocket)
{
	mFuseRunner = NULL;
}

BupSlave::~BupSlave() {
	if(mFuseRunner)
		delete mFuseRunner;
}

void BupSlave::get(const KUrl& pUrl) {
	KProcess lP;
	lP <<"logger" <<"get: " <<pUrl.url();
	lP.execute();

	if(!checkRunnningFuse(pUrl) || !pUrl.path(KUrl::AddTrailingSlash).startsWith(mFuseRunner->mRepoPath)) {
		error(KIO::ERR_SLAVE_DEFINED, i18n("No bup repository found.\n%1", pUrl.prettyUrl()));
		return;
	}

	QString lPathInRepo = pUrl.path();
	lPathInRepo.remove(0, mFuseRunner->mRepoPath.length());
	fileGet(KUrl::fromPath(mFuseRunner->mMountPath + lPathInRepo));
}


void BupSlave::stat(const KUrl& pUrl) {
	KProcess lP;
	lP <<"logger" <<"stat: " <<pUrl.url();
	lP.execute();

	if(!checkRunnningFuse(pUrl)|| !pUrl.path(KUrl::AddTrailingSlash).startsWith(mFuseRunner->mRepoPath)) {
		error(KIO::ERR_SLAVE_DEFINED, i18n("No bup repository found.\n%1", pUrl.prettyUrl()));
		return;
	}

	QString lPathInRepo = pUrl.path();
	lPathInRepo.remove(0, mFuseRunner->mRepoPath.length());
	fileStat(KUrl::fromPath(mFuseRunner->mMountPath + lPathInRepo));
}

void BupSlave::listDir(const KUrl& pUrl) {
	KProcess lP;
	lP <<"logger" <<"listdir: " <<pUrl.url();
	lP.execute();

	if(!checkRunnningFuse(pUrl) || !pUrl.path(KUrl::AddTrailingSlash).startsWith(mFuseRunner->mRepoPath)) {
		error(KIO::ERR_SLAVE_DEFINED, i18n("No bup repository found.\n%1", pUrl.prettyUrl()));
		return;
	}

	QString lPathInRepo = pUrl.path();
	lPathInRepo.remove(0, mFuseRunner->mRepoPath.length());
	fileListDir(KUrl::fromPath(mFuseRunner->mMountPath + lPathInRepo));


//	const QByteArray _path(QFile::encodeName("/tmp/lala"));
//	KProcess p;
//	p <<"logger" <<"filelistdir listing: " <<_path;
//	p.execute();

//	DIR* dp = opendir(_path.data());
//	struct dirent *ep;
//	while ( ( ep = readdir( dp ) ) != 0 ) {
//		KProcess lP;
//		lP <<"logger" <<"filelistdir listed: " <<ep->d_name;
//		lP.execute();
//	}

//	closedir( dp );

}

bool BupSlave::checkRunnningFuse(const KUrl &pUrl) {

	if(mFuseRunner) {
		KProcess lP;
		lP <<"logger" <<"checking: " <<pUrl.path(KUrl::RemoveTrailingSlash) <<mFuseRunner->mRepoPath <<(mFuseRunner->mRunning ? "true": "false");
		lP.execute();

		if(mFuseRunner->mRunning)// && pUrl.path(KUrl::AddTrailingSlash).startsWith(mFuseRunner->mRepoPath))
			return true;
		else
			delete mFuseRunner;
	}

	QString lPath = pUrl.path(KUrl::RemoveTrailingSlash);
	QStringList lPathElements = lPath.split('/');
	QString lRepoPath;
	bool lFound = false;
//	error(KIO::ERR_SLAVE_DEFINED, QString("%1 elements.").arg(lPathElements.count()));
	foreach(QString lPathElement, lPathElements) {
		lRepoPath += lPathElement + "/";
		qDebug() <<"trying repo path: " << lRepoPath;
		if(QFile::exists(lRepoPath + "objects/pack/bup.bloom")) {
			lFound = true;
			break;
		}
	}
	if(!lFound)
		return false;


	mFuseRunner = new FuseRunner(lRepoPath);
	return true;
}

void BupSlave::fileListDir(const KUrl& url)
{
	const QString path(url.toLocalFile());
	const QByteArray _path(QFile::encodeName(path));

	KProcess p;
	p <<"logger" << "filelistdir..." << _path.data();
	p.execute();

	DIR* dp = opendir(_path.data());
	if ( dp == 0 ) {
		switch (errno) {
		case ENOENT:
			error(KIO::ERR_DOES_NOT_EXIST, path);
			return;
		case ENOTDIR:
			error(KIO::ERR_IS_FILE, path);
			break;
#ifdef ENOMEDIUM
		case ENOMEDIUM:
			error(ERR_SLAVE_DEFINED,
			      i18n("No media in device for %1", path));
			break;
#endif
		default:
			error(KIO::ERR_CANNOT_ENTER_DIRECTORY, path);
			break;
		}
		return;
	}


	const QString sDetails = metaData(QLatin1String("details"));
	const int details = sDetails.isEmpty() ? 2 : sDetails.toInt();
	kDebug(7101) << "========= LIST " << url << "details=" << details << " =========";
	UDSEntry entry;

#ifndef HAVE_DIRENT_D_TYPE
	KDE_struct_stat st;
#endif
	// Don't make this a QStringList. The locale file name we get here
	// should be passed intact to createUDSEntry to avoid problems with
	// files where QFile::encodeName(QFile::decodeName(a)) != a.
	QList<QByteArray> entryNames;
	KDE_struct_dirent *ep;
	if (details == 0) {
		KProcess lP;
		lP <<"logger" <<"filelistdir (details = 0): " <<_path.data();
		lP.execute();

		// Fast path (for recursive deletion, mostly)
		// Simply emit the name and file type, nothing else.
		while ( ( ep = KDE_readdir( dp ) ) != 0 ) {
			entry.clear();
			entry.insert(KIO::UDSEntry::UDS_NAME, QFile::decodeName(ep->d_name));
#ifdef HAVE_DIRENT_D_TYPE
			entry.insert(KIO::UDSEntry::UDS_FILE_TYPE,
			             (ep->d_type & DT_DIR) ? S_IFDIR : S_IFREG );
			const bool isSymLink = (ep->d_type & DT_LNK);
#else
			// oops, no fast way, we need to stat (e.g. on Solaris)
			if (KDE_lstat(ep->d_name, &st) == -1) {
				continue; // how can stat fail?
			}
			entry.insert(KIO::UDSEntry::UDS_FILE_TYPE,
			             (S_ISDIR(st.st_mode)) ? S_IFDIR : S_IFREG );
			const bool isSymLink = S_ISLNK(st.st_mode);
#endif
			if (isSymLink) {
				// for symlinks obey the UDSEntry contract and provide UDS_LINK_DEST
				// even if we don't know the link dest (and DeleteJob doesn't care...)
				entry.insert(KIO::UDSEntry::UDS_LINK_DEST, QLatin1String("Dummy Link Target"));
			}
			listEntry(entry, false);
		}
		closedir( dp );
		listEntry( entry, true ); // ready
	} else {
		while ( ( ep = KDE_readdir( dp ) ) != 0 ) {
			entryNames.append( ep->d_name );
		}

		closedir( dp );
		totalSize( entryNames.count() );


		/* set the current dir to the path to speed up
           in not having to pass an absolute path.
           We restore the path later to get out of the
           path - the kernel wouldn't unmount or delete
           directories we keep as active directory. And
           as the slave runs in the background, it's hard
           to see for the user what the problem would be */
#if !defined(PATH_MAX) && defined(__GLIBC__)
		char *path_buffer = ::get_current_dir_name();
		const KDEPrivate::CharArrayDeleter path_buffer_deleter(path_buffer);
#else
		char path_buffer[PATH_MAX];
		path_buffer[0] = '\0';
		(void) getcwd(path_buffer, PATH_MAX - 1);
#endif
		if ( chdir( _path.data() ) )  {
			if (errno == EACCES)
				error(ERR_ACCESS_DENIED, path);
			else
				error(ERR_CANNOT_ENTER_DIRECTORY, path);
			finished();
		}

		QList<QByteArray>::ConstIterator it = entryNames.constBegin();
		QList<QByteArray>::ConstIterator end = entryNames.constEnd();
		for (; it != end; ++it) {
			entry.clear();
			KProcess p;
			p <<"logger" << QFile::decodeName(*it);
			p.execute();
			if ( createUDSEntry( QFile::decodeName(*it),
			                     *it /* we can use the filename as relative path*/,
			                     entry, details, true ) ) {
				listEntry( entry, false);
			}
		}

		listEntry( entry, true ); // ready

        kDebug(7101) << "============= COMPLETED LIST ============";

#if !defined(PATH_MAX) && defined(__GLIBC__)
		if (path_buffer)
#else
		if (*path_buffer)
#endif
		{
			chdir(path_buffer);
		}
	}
	finished();
}

QString BupSlave::getUserName( uid_t uid ) const
{
	if ( !mUsercache.contains( uid ) ) {
		struct passwd *user = getpwuid( uid );
		if ( user ) {
			mUsercache.insert( uid, QString::fromLatin1(user->pw_name) );
		}
		else
			return QString::number( uid );
	}
	return mUsercache[uid];
}

QString BupSlave::getGroupName( gid_t gid ) const
{
	if ( !mGroupcache.contains( gid ) ) {
		struct group *grp = getgrgid( gid );
		if ( grp ) {
			mGroupcache.insert( gid, QString::fromLatin1(grp->gr_name) );
		}
		else
			return QString::number( gid );
	}
	return mGroupcache[gid];
}


bool BupSlave::createUDSEntry(const QString & filename, const QByteArray & path, UDSEntry & entry,
                              short int details, bool withACL )
{
#ifndef HAVE_POSIX_ACL
	Q_UNUSED(withACL);
#endif

	entry.insert( KIO::UDSEntry::UDS_NAME, filename );

	mode_t type;
	mode_t access;
	KDE_struct_stat buff;

	if ( KDE_lstat( path.data(), &buff ) == 0 )  {

		if (S_ISLNK(buff.st_mode)) {

			char buffer2[ 1000 ];
			int n = readlink( path.data(), buffer2, 999 );
			if ( n != -1 ) {
				buffer2[ n ] = 0;
			}

			entry.insert( KIO::UDSEntry::UDS_LINK_DEST, QFile::decodeName( buffer2 ) );

			// A symlink -> follow it only if details>1
			if ( details > 1 && KDE_stat( path.data(), &buff ) == -1 ) {
				// It is a link pointing to nowhere
				type = S_IFMT - 1;
				access = S_IRWXU | S_IRWXG | S_IRWXO;

				entry.insert( KIO::UDSEntry::UDS_FILE_TYPE, type );
				entry.insert( KIO::UDSEntry::UDS_ACCESS, access );
				entry.insert( KIO::UDSEntry::UDS_SIZE, 0LL );
				goto notype;

			}
		}
	} else {
		// kWarning() << "lstat didn't work on " << path.data();
		return false;
	}

	type = buff.st_mode & S_IFMT; // extract file type
	access = buff.st_mode & 07777; // extract permissions

	entry.insert( KIO::UDSEntry::UDS_FILE_TYPE, type );
	entry.insert( KIO::UDSEntry::UDS_ACCESS, access );

	entry.insert( KIO::UDSEntry::UDS_SIZE, buff.st_size );

#ifdef HAVE_POSIX_ACL
	if (details > 0) {
		/* Append an atom indicating whether the file has extended acl information
         * and if withACL is specified also one with the acl itself. If it's a directory
         * and it has a default ACL, also append that. */
		appendACLAtoms( path, entry, type, withACL );
	}
#endif

notype:
	if (details > 0) {
		entry.insert( KIO::UDSEntry::UDS_MODIFICATION_TIME, buff.st_mtime );
		entry.insert( KIO::UDSEntry::UDS_USER, getUserName( buff.st_uid ) );
		entry.insert( KIO::UDSEntry::UDS_GROUP, getGroupName( buff.st_gid ) );
		entry.insert( KIO::UDSEntry::UDS_ACCESS_TIME, buff.st_atime );
	}

	// Note: buff.st_ctime isn't the creation time !
	// We made that mistake for KDE 2.0, but it's in fact the
	// "file status" change time, which we don't care about.

	return true;
}

void BupSlave::fileGet(const KUrl& url) {
	KProcess lP;
	lP <<"logger" <<"fileget: " <<url.url();
	lP.execute();
	const QString path(url.toLocalFile());
	KDE_struct_stat buff;
	if ( KDE::stat( path, &buff ) == -1 ) {
		if ( errno == EACCES )
			error(KIO::ERR_ACCESS_DENIED, path);
		else
			error(KIO::ERR_DOES_NOT_EXIST, path);
		return;
	}

	if ( S_ISDIR( buff.st_mode ) ) {
		error(KIO::ERR_IS_DIRECTORY, path);
		return;
	}
	if ( !S_ISREG( buff.st_mode ) ) {
		error(KIO::ERR_CANNOT_OPEN_FOR_READING, path);
		return;
	}

	int fd = KDE::open( path, O_RDONLY);
	if ( fd < 0 ) {
		error(KIO::ERR_CANNOT_OPEN_FOR_READING, path);
		return;
	}

	// Determine the mimetype of the file to be retrieved, and emit it.
	// This is mandatory in all slaves (for KRun/BrowserRun to work)
	// In real "remote" slaves, this is usually done using findByNameAndContent
	// after receiving some data. But we don't know how much data the mimemagic rules
	// need, so for local files, better use findByUrl with localUrl=true.
	KMimeType::Ptr mt = KMimeType::findByUrl( url, buff.st_mode, true /* local URL */ );
	emit mimeType( mt->name() );
	// Emit total size AFTER mimetype
	totalSize( buff.st_size );

	KIO::filesize_t processed_size = 0;

	const QString resumeOffset = metaData(QLatin1String("resume"));
	if ( !resumeOffset.isEmpty() )
	{
		bool ok;
		KIO::fileoffset_t offset = resumeOffset.toLongLong(&ok);
		if (ok && (offset > 0) && (offset < buff.st_size))
		{
			if (KDE_lseek(fd, offset, SEEK_SET) == offset)
			{
				canResume ();
				processed_size = offset;
			}
		}
	}

	char buffer[MAX_IPC_SIZE];
	QByteArray array;

	while( 1 )
	{
		int n = ::read( fd, buffer, MAX_IPC_SIZE );
		if (n == -1)
		{
			if (errno == EINTR)
				continue;
			error(KIO::ERR_COULD_NOT_READ, path);
			::close(fd);
			return;
		}
		if (n == 0)
			break; // Finished

		array = QByteArray::fromRawData(buffer, n);
		data( array );
		array.clear();

		processed_size += n;
		processedSize( processed_size );

		//kDebug(7101) << "Processed: " << KIO::number (processed_size);
	}

	data( QByteArray() );

	::close( fd );

	processedSize( buff.st_size );
	finished();
}

void BupSlave::fileStat(const KUrl & url) {
	KProcess lP;
	lP <<"logger" <<"filestat: " <<url.url();
	lP.execute();
	/* directories may not have a slash at the end if
     * we want to stat() them; it requires that we
     * change into it .. which may not be allowed
     * stat("/is/unaccessible")  -> rwx------
     * stat("/is/unaccessible/") -> EPERM            H.Z.
     * This is the reason for the -1
     */
	const QString path(url.path(KUrl::RemoveTrailingSlash));
	const QByteArray _path( QFile::encodeName(path));
	const QString sDetails = metaData(QLatin1String("details"));
	const int details = sDetails.isEmpty() ? 2 : sDetails.toInt();

	UDSEntry entry;
	if ( !createUDSEntry( url.fileName(), _path, entry, details, true /*with acls*/ ) )
	{
		error(KIO::ERR_DOES_NOT_EXIST, path);
		return;
	}
	statEntry(entry);

	finished();
}


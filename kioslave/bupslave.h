
#ifndef BUPSLAVE_H
#define BUPSLAVE_H

#include <KProcess>
#include <KIO/SlaveBase>

#include <QHash>

class KTempDir;

class FuseRunner : public QObject
{
	Q_OBJECT
public:
	FuseRunner(const QString &pRepoPath);
	~FuseRunner();
	KTempDir *mTempDir;
	KProcess mProcess;
	QString mRepoPath;
	QString mMountPath;
	bool mRunning;
public slots:
	void fuseFinished(int pExitCode,QProcess::ExitStatus pExitStatus);
};

class BupSlave : public KIO::SlaveBase
{
public:
	BupSlave(const QByteArray &pPoolSocket, const QByteArray &pAppSocket);
	virtual ~BupSlave();
	virtual void get(const KUrl &pUrl);
	virtual void listDir(const KUrl &pUrl) ;
	virtual void stat(const KUrl &pUrl);

private:
	void fileListDir(const KUrl& url);
	void fileGet(const KUrl& url);
	void fileStat(const KUrl & url);
	bool checkRunnningFuse(const KUrl &pUrl);
	FuseRunner *mFuseRunner;

	QString getUserName( uid_t uid ) const;
	QString getGroupName( gid_t gid ) const;
	bool createUDSEntry( const QString & filename, const QByteArray & path, KIO::UDSEntry & entry,
	                     short int details, bool withACL );

	mutable QHash<uid_t, QString> mUsercache;
	mutable QHash<gid_t, QString> mGroupcache;

};

#endif // BUPSLAVE_H

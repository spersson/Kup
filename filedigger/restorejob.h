#ifndef RESTOREJOB_H
#define RESTOREJOB_H

#include "versionlistmodel.h"

#include <KJob>
#include <KProcess>

class RestoreJob : public KJob
{
	Q_OBJECT
public:
	explicit RestoreJob(const QString &pRepositoryPath, const QString &pSourcePath, const QString &pRestorationPath,
	                    int pTotalDirCount, quint64 pTotalFileSize, const QHash<QString, quint64> &pFileSizes);
	virtual void start();

protected slots:
	void slotRestoringStarted();
	void slotRestoringDone(int pExitCode, QProcess::ExitStatus pExitStatus);

protected:
	virtual void timerEvent(QTimerEvent *pTimerEvent);
	void makeNice(int pPid);
	void moveFolder();

	KProcess mRestoreProcess;
	QString mRepositoryPath;
	QString mSourcePath;
	QString mRestorationPath;
	QString mSourceFileName;
	int mTotalDirCount;
	quint64 mTotalFileSize;
	const QHash<QString, quint64> &mFileSizes;
	int mTimerId;
};

#endif // RESTOREJOB_H

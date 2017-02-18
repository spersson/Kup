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

#include "backupjob.h"
#include "bupjob.h"
#include "kupdaemon.h"
#include "rsyncjob.h"

#include <unistd.h>
#include <sys/resource.h>
#ifdef Q_OS_LINUX
#include <sys/syscall.h>
#endif

#include <QTimer>

BackupJob::BackupJob(const BackupPlan &pBackupPlan, const QString &pDestinationPath, const QString &pLogFilePath, KupDaemon *pKupDaemon)
   :KJob(), mBackupPlan(pBackupPlan), mDestinationPath(pDestinationPath), mLogFilePath(pLogFilePath), mKupDaemon(pKupDaemon)
{
	mLogFile.setFileName(mLogFilePath);
	mLogFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
	mLogStream.setDevice(&mLogFile);
}

void BackupJob::start() {
	mKupDaemon->registerJob(this);
	QTimer::singleShot(0, this, &BackupJob::performJob);
}

void BackupJob::makeNice(int pPid) {
#ifdef Q_OS_LINUX
	// See linux documentation Documentation/block/ioprio.txt for details of the syscall
	syscall(SYS_ioprio_set, 1, pPid, 3 << 13 | 7);
#endif
	setpriority(PRIO_PROCESS, pPid, 19);
}

QString BackupJob::quoteArgs(const QStringList &pCommand) {
	QString lResult;
	bool lFirst = true;
	foreach(const QString &lArg, pCommand) {
		if(lFirst) {
			lResult.append(lArg);
			lFirst = false;
		} else {
			lResult.append(QStringLiteral(" \""));
			lResult.append(lArg);
			lResult.append(QStringLiteral("\""));
		}
	}
	return lResult;
}

void BackupJob::jobFinishedSuccess() {
	// unregistring a job will normally show a UI notification that it the job was completed
	// setting the error code to indicate that the user canceled the job makes the UI not show
	// any notification. We want that since we want to trigger our own notification which has
	// more buttons and stuff.
	setError(KilledJobError);
	mKupDaemon->unregisterJob(this);

	// The error code is still used by our internal logic, for triggering our own notification.
	// So make sure to set it correctly.
	setError(NoError);
	emitResult();
}

void BackupJob::jobFinishedError(BackupJob::ErrorCodes pErrorCode, QString pErrorText) {
	// if job has already set the error that it was killed by the user then ignore any fault
	// we get here as that fault is surely about the process exit code was not zero.
	// And we don't want to report about that (with our notification) in this case.
	bool lWasKilled = (error() == KilledJobError);

	setError(KilledJobError);
	mKupDaemon->unregisterJob(this);
	if(!lWasKilled) {
		setError(pErrorCode);
		setErrorText(pErrorText);
	}
	emitResult();
}


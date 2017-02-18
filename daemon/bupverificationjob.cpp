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

#include "bupverificationjob.h"

#include <KLocalizedString>

BupVerificationJob::BupVerificationJob(const BackupPlan &pBackupPlan, const QString &pDestinationPath,
                                       const QString &pLogFilePath, KupDaemon *pKupDaemon)
   : BackupJob(pBackupPlan, pDestinationPath, pLogFilePath, pKupDaemon){
	mFsckProcess.setOutputChannelMode(KProcess::SeparateChannels);
}

void BupVerificationJob::performJob() {
	KProcess lVersionProcess;
	lVersionProcess.setOutputChannelMode(KProcess::SeparateChannels);
	lVersionProcess << QStringLiteral("bup") << QStringLiteral("version");
	if(lVersionProcess.execute() < 0) {
		jobFinishedError(ErrorWithoutLog, xi18nc("@info notification",
		                                         "The <application>bup</application> program is needed but could not be found, "
		                                         "maybe it is not installed?"));
		return;
	}

	mLogStream << QStringLiteral("Kup is starting bup verification job at ")
	           << QLocale().toString(QDateTime::currentDateTime())
	           << endl << endl;

	mFsckProcess << QStringLiteral("bup");
	mFsckProcess << QStringLiteral("-d") << mDestinationPath;
	mFsckProcess << QStringLiteral("fsck") << QStringLiteral("--quick");

	connect(&mFsckProcess, SIGNAL(finished(int,QProcess::ExitStatus)), SLOT(slotCheckingDone(int,QProcess::ExitStatus)));
	connect(&mFsckProcess, SIGNAL(started()), SLOT(slotCheckingStarted()));
	mLogStream << mFsckProcess.program().join(QStringLiteral(" ")) << endl;
	mFsckProcess.start();
}

void BupVerificationJob::slotCheckingStarted() {
	makeNice(mFsckProcess.pid());
}

void BupVerificationJob::slotCheckingDone(int pExitCode, QProcess::ExitStatus pExitStatus) {
	mLogStream << QString::fromUtf8(mFsckProcess.readAllStandardError());
	if(pExitStatus != QProcess::NormalExit) {
		mLogStream << endl << QStringLiteral("Integrity check failed (the process crashed). Your backups could be "
		                                    "corrupted! See above for details.") << endl;
		if(mBackupPlan.mGenerateRecoveryInfo) {
			jobFinishedError(ErrorSuggestRepair, xi18nc("@info notification",
			                                    "Failed backup integrity check. Your backups could be corrupted! "
			                                    "See log file for more details. Do you want to try repairing the backup files?"));
		} else {
			jobFinishedError(ErrorWithLog, xi18nc("@info notification", "Failed backup integrity check. Your backups are corrupted! "
			                                                            "See log file for more details."));
		}
	} else if(pExitCode == 0) {
		mLogStream << endl << QStringLiteral("Backup integrity test was successful. "
		                                     "Your backups are fine. See above for details.") << endl;
		jobFinishedError(ErrorWithLog, xi18nc("@info notification", "Backup integrity test was successful, "
		                                                            "Your backups are fine."));
	} else {
		mLogStream << endl << QStringLiteral("Integrity check failed. Your backups are "
		                                     "corrupted! See above for details.") << endl;
		if(mBackupPlan.mGenerateRecoveryInfo) {
			jobFinishedError(ErrorSuggestRepair, xi18nc("@info notification",
			                                            "Failed backup integrity check. Your backups are corrupted! "
			                                            "See log file for more details. Do you want to try repairing the backup files?"));

		} else {
			jobFinishedError(ErrorWithLog, xi18nc("@info notification", "Failed backup integrity check. Your backups are corrupted! "
			                                                            "See log file for more details."));
		}
	}
}

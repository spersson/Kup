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

#include <KGlobal>
#include <KLocalizedString>
#include <QTimer>

BupVerificationJob::BupVerificationJob(const BackupPlan &pBackupPlan, const QString &pDestinationPath,
                                       const QString &pLogFilePath)
   : BackupJob(pBackupPlan, pDestinationPath, pLogFilePath){
	mFsckProcess.setOutputChannelMode(KProcess::SeparateChannels);
}

void BupVerificationJob::start() {
	QTimer::singleShot(0, this, SLOT(startJob()));
}

void BupVerificationJob::startJob() {
	KProcess lVersionProcess;
	lVersionProcess.setOutputChannelMode(KProcess::SeparateChannels);
	lVersionProcess << QLatin1String("bup") << QLatin1String("version");
	if(lVersionProcess.execute() < 0) {
		setError(ErrorWithoutLog);
		setErrorText(i18nc("notification", "The \"bup\" program is needed but could not be found, "
		                   "maybe it is not installed?"));
		emitResult();
		return;
	}

	mLogStream << QLatin1String("Kup is starting bup verification job at ")
	           << KGlobal::locale()->formatDateTime(QDateTime::currentDateTime(),
	                                                KLocale::LongDate, true)
	           << endl << endl;

	mFsckProcess << QLatin1String("bup");
	mFsckProcess << QLatin1String("-d") << mDestinationPath;
	mFsckProcess << QLatin1String("fsck") << QLatin1String("--quick");

	connect(&mFsckProcess, SIGNAL(finished(int,QProcess::ExitStatus)), SLOT(slotCheckingDone(int,QProcess::ExitStatus)));
	connect(&mFsckProcess, SIGNAL(started()), SLOT(slotCheckingStarted()));
	mLogStream << mFsckProcess.program().join(QLatin1String(" ")) << endl;
	mFsckProcess.start();
}

void BupVerificationJob::slotCheckingStarted() {
	makeNice(mFsckProcess.pid());
}

void BupVerificationJob::slotCheckingDone(int pExitCode, QProcess::ExitStatus pExitStatus) {
	mLogStream << QString::fromUtf8(mFsckProcess.readAllStandardError());
	setError(ErrorWithLog);
	if(pExitStatus != QProcess::NormalExit) {
		mLogStream << endl << QLatin1String("Integrity check failed (the process crashed). Your backups could be "
		                                    "corrupted! See above for details.") << endl;
		if(mBackupPlan.mGenerateRecoveryInfo) {
			setErrorText(i18nc("notification", "Failed backup integrity check. Your backups could be corrupted! "
			                                   "See log file for more details. Do you want to try repairing the backup files?"));
			setError(ErrorSuggestRepair);
		} else {
			setErrorText(i18nc("notification", "Failed backup integrity check. Your backups are corrupted! "
			                                   "See log file for more details."));
		}
	} else if(pExitCode == 0) {
		mLogStream << endl << QLatin1String("Backup integrity test was successful. "
		                                    "Your backups are fine. See above for details.") << endl;
		setErrorText(i18nc("notification", "Backup integrity test was successful, "
		                                   "Your backups are fine."));
	} else {
		mLogStream << endl << QLatin1String("Integrity check failed. Your backups are "
		                                    "corrupted! See above for details.") << endl;
		if(mBackupPlan.mGenerateRecoveryInfo) {
			setErrorText(i18nc("notification", "Failed backup integrity check. Your backups are corrupted! "
			                                   "See log file for more details. Do you want to try repairing the backup files?"));
			setError(ErrorSuggestRepair);
		} else {
			setErrorText(i18nc("notification", "Failed backup integrity check. Your backups are corrupted! "
			                                   "See log file for more details."));
		}
	}

	emitResult();
	return;
}

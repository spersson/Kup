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

#include "buprepairjob.h"

#include <KGlobal>
#include <KLocale>
#include <QTimer>

BupRepairJob::BupRepairJob(const BackupPlan &pBackupPlan, const QString &pDestinationPath,
                                       const QString &pLogFilePath)
   : BackupJob(pBackupPlan, pDestinationPath, pLogFilePath){
	mFsckProcess.setOutputChannelMode(KProcess::SeparateChannels);
}

void BupRepairJob::start() {
	QTimer::singleShot(0, this, SLOT(startJob()));
}

void BupRepairJob::startJob() {
	KProcess lPar2Process;
	lPar2Process.setOutputChannelMode(KProcess::SeparateChannels);
	lPar2Process << QLatin1String("bup") << QLatin1String("fsck") << QLatin1String("--par2-ok");
	int lExitCode = lPar2Process.execute();
	if(lExitCode < 0) {
		setError(ErrorWithoutLog);
		setErrorText(i18nc("notification", "The \"bup\" program is needed but could not be found, "
		                   "maybe it is not installed?"));
		emitResult();
		return;
	} else if(mBackupPlan.mGenerateRecoveryInfo && lExitCode != 0) {
		setError(ErrorWithoutLog);
		setErrorText(i18nc("notification", "The \"par2\" program is needed but could not be found, "
		                   "maybe it is not installed?"));
		emitResult();
		return;
	}

	mLogStream << QLatin1String("Kup is starting bup repair job at ")
	           << KGlobal::locale()->formatDateTime(QDateTime::currentDateTime(),
	                                                KLocale::LongDate, true)
	           << endl << endl;

	mFsckProcess << QLatin1String("bup");
	mFsckProcess << QLatin1String("-d") << mDestinationPath;
	mFsckProcess << QLatin1String("fsck") << QLatin1String("-r");

	connect(&mFsckProcess, SIGNAL(finished(int,QProcess::ExitStatus)), SLOT(slotRepairDone(int,QProcess::ExitStatus)));
	connect(&mFsckProcess, SIGNAL(started()), SLOT(slotRepairStarted()));
	mLogStream << mFsckProcess.program().join(QLatin1String(" ")) << endl;
	mFsckProcess.start();
}

void BupRepairJob::slotRepairStarted() {
	makeNice(mFsckProcess.pid());
}

void BupRepairJob::slotRepairDone(int pExitCode, QProcess::ExitStatus pExitStatus) {
	mLogStream << QString::fromUtf8(mFsckProcess.readAllStandardError());
	setError(ErrorWithLog);
	if(pExitStatus != QProcess::NormalExit) {
		mLogStream << endl << QLatin1String("Repair failed (the repair process crashed). Your backups could be "
		                                    "corrupted! See above for details.") << endl;
		setErrorText(i18nc("notification", "Backup repair failed. Your backups could be corrupted! "
		                                   "See log file for more details."));
	} else if(pExitCode == 100) {
		mLogStream << endl << QLatin1String("Repair succeded. See above for details.") << endl;
		setErrorText(i18nc("notification", "Success! Backup repair worked. See log file for more details."));
	} else if(pExitCode == 0) {
		mLogStream << endl << QLatin1String("Repair was not necessary. Your backups are fine. See above for details.") << endl;
		setErrorText(i18nc("notification", "Backup repair was not necessary. Your backups are not corrupted."
		                                   "See log file for more details."));
	} else {
		mLogStream << endl << QLatin1String("Repair failed. Your backups could still be "
		                                    "corrupted! See above for details.") << endl;
		setErrorText(i18nc("notification", "Backup repair failed. Your backups could still be corrupted! "
		                                   "See log file for more details."));
	}

	emitResult();
	return;
}

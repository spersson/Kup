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

#include <QTimer>

#include <KLocalizedString>

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
	lPar2Process << QStringLiteral("bup") << QStringLiteral("fsck") << QStringLiteral("--par2-ok");
	int lExitCode = lPar2Process.execute();
	if(lExitCode < 0) {
		setError(ErrorWithoutLog);
		setErrorText(xi18nc("@info notification",
		                    "The <application>bup</application> program is needed but could not be found, "
		                    "maybe it is not installed?"));
		emitResult();
		return;
	} else if(mBackupPlan.mGenerateRecoveryInfo && lExitCode != 0) {
		setError(ErrorWithoutLog);
		setErrorText(xi18nc("@info notification",
		                    "The <application>par2</application> program is needed but could not be found, "
		                    "maybe it is not installed?"));
		emitResult();
		return;
	}

	mLogStream << QStringLiteral("Kup is starting bup repair job at ")
	           << QLocale().toString(QDateTime::currentDateTime())
	           << endl << endl;

	mFsckProcess << QStringLiteral("bup");
	mFsckProcess << QStringLiteral("-d") << mDestinationPath;
	mFsckProcess << QStringLiteral("fsck") << QStringLiteral("-r");

	connect(&mFsckProcess, SIGNAL(finished(int,QProcess::ExitStatus)), SLOT(slotRepairDone(int,QProcess::ExitStatus)));
	connect(&mFsckProcess, SIGNAL(started()), SLOT(slotRepairStarted()));
	mLogStream << mFsckProcess.program().join(QStringLiteral(" ")) << endl;
	mFsckProcess.start();
}

void BupRepairJob::slotRepairStarted() {
	makeNice(mFsckProcess.pid());
}

void BupRepairJob::slotRepairDone(int pExitCode, QProcess::ExitStatus pExitStatus) {
	mLogStream << QString::fromUtf8(mFsckProcess.readAllStandardError());
	setError(ErrorWithLog);
	if(pExitStatus != QProcess::NormalExit) {
		mLogStream << endl << QStringLiteral("Repair failed (the repair process crashed). Your backups could be "
		                                     "corrupted! See above for details.") << endl;
		setErrorText(xi18nc("@info notification", "Backup repair failed. Your backups could be corrupted! "
		                                          "See log file for more details."));
	} else if(pExitCode == 100) {
		mLogStream << endl << QStringLiteral("Repair succeded. See above for details.") << endl;
		setErrorText(xi18nc("@info notification", "Success! Backup repair worked. See log file for more details."));
	} else if(pExitCode == 0) {
		mLogStream << endl << QStringLiteral("Repair was not necessary. Your backups are fine. See "
		                                     "above for details.") << endl;
		setErrorText(xi18nc("@info notification", "Backup repair was not necessary. Your backups are not corrupted."
		                                          "See log file for more details."));
	} else {
		mLogStream << endl << QStringLiteral("Repair failed. Your backups could still be "
		                                    "corrupted! See above for details.") << endl;
		setErrorText(xi18nc("@info notification", "Backup repair failed. Your backups could still be corrupted! "
		                                   "See log file for more details."));
	}

	emitResult();
	return;
}

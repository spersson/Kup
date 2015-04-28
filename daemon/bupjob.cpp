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

#include "bupjob.h"

#include <QDir>
#include <QTimer>

#include <KLocalizedString>

BupJob::BupJob(const BackupPlan &pBackupPlan, const QString &pDestinationPath, const QString &pLogFilePath)
   :BackupJob(pBackupPlan, pDestinationPath, pLogFilePath)
{
	mFsckProcess.setOutputChannelMode(KProcess::SeparateChannels);
	mIndexProcess.setOutputChannelMode(KProcess::SeparateChannels);
	mSaveProcess.setOutputChannelMode(KProcess::SeparateChannels);
	mPar2Process.setOutputChannelMode(KProcess::SeparateChannels);
}

void BupJob::start() {
	QTimer::singleShot(0, this, SLOT(startJob()));
}

void BupJob::startJob() {
	KProcess lPar2Process;
	lPar2Process.setOutputChannelMode(KProcess::SeparateChannels);
	lPar2Process << QStringLiteral("bup") << QStringLiteral("fsck") << QStringLiteral("--par2-ok");
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

	mLogStream << QStringLiteral("Kup is starting bup backup job at ")
	           << QLocale().toString(QDateTime::currentDateTime())
	           << endl << endl;

	KProcess lInitProcess;
	lInitProcess.setOutputChannelMode(KProcess::SeparateChannels);
	lInitProcess << QStringLiteral("bup");
	lInitProcess << QStringLiteral("-d") << mDestinationPath;
	lInitProcess << QStringLiteral("init");
	mLogStream << lInitProcess.program().join(QStringLiteral(" ")) << endl;
	if(lInitProcess.execute() != 0) {
		mLogStream << QString::fromUtf8(lInitProcess.readAllStandardError()) << endl;
		mLogStream << endl << QStringLiteral("Kup did not successfully complete the bup backup job: "
		                                    "failed to initialize backup destination.") << endl;
		setError(ErrorWithLog);
		setErrorText(i18nc("notification", "Backup destination could not be initialised. "
		                   "See log file for more details."));
		emitResult();
		return;
	}

	if(mBackupPlan.mCheckBackups) {
		mFsckProcess << QStringLiteral("bup");
		mFsckProcess << QStringLiteral("-d") << mDestinationPath;
		mFsckProcess << QStringLiteral("fsck") << QStringLiteral("--quick");

		connect(&mFsckProcess, SIGNAL(finished(int,QProcess::ExitStatus)), SLOT(slotCheckingDone(int,QProcess::ExitStatus)));
		connect(&mFsckProcess, SIGNAL(started()), SLOT(slotCheckingStarted()));
		mLogStream << mFsckProcess.program().join(QStringLiteral(" ")) << endl;
		mFsckProcess.start();
	} else {
		slotCheckingDone(0, QProcess::NormalExit);
	}
}

void BupJob::slotCheckingStarted() {
	makeNice(mFsckProcess.pid());
}

void BupJob::slotCheckingDone(int pExitCode, QProcess::ExitStatus pExitStatus) {
	mLogStream << QString::fromUtf8(mFsckProcess.readAllStandardError());
	if(pExitStatus != QProcess::NormalExit || pExitCode != 0) {
		mLogStream << endl << QStringLiteral("Kup did not successfully complete the bup backup job: "
		                            "failed integrity check. Your backups could be "
		                            "corrupted! See above for details.") << endl;
		if(mBackupPlan.mGenerateRecoveryInfo) {
			setErrorText(i18nc("notification", "Failed backup integrity check. Your backups could be corrupted! "
			                                   "See log file for more details. Do you want to try repairing the backup files?"));
			setError(ErrorSuggestRepair);
		} else {
			setErrorText(i18nc("notification", "Failed backup integrity check. Your backups could be corrupted! "
			                                   "See log file for more details."));
			setError(ErrorWithLog);
		}
		emitResult();
		return;
	}
	mIndexProcess << QStringLiteral("bup");
	mIndexProcess << QStringLiteral("-d") << mDestinationPath;
	mIndexProcess << QStringLiteral("index") << QStringLiteral("-u");

	foreach(QString lExclude, mBackupPlan.mPathsExcluded) {
		mIndexProcess << QStringLiteral("--exclude");
		mIndexProcess << lExclude;
	}
	mIndexProcess << mBackupPlan.mPathsIncluded;

	connect(&mIndexProcess, SIGNAL(finished(int,QProcess::ExitStatus)), SLOT(slotIndexingDone(int,QProcess::ExitStatus)));
	connect(&mIndexProcess, SIGNAL(started()), SLOT(slotIndexingStarted()));
	mLogStream << mIndexProcess.program().join(QStringLiteral(" ")) << endl;
	mIndexProcess.start();
}

void BupJob::slotIndexingStarted() {
	makeNice(mIndexProcess.pid());
}

void BupJob::slotIndexingDone(int pExitCode, QProcess::ExitStatus pExitStatus) {
	mLogStream << QString::fromUtf8(mIndexProcess.readAllStandardError());
	if(pExitStatus != QProcess::NormalExit || pExitCode != 0) {
		mLogStream << endl << QStringLiteral("Kup did not successfully complete the bup backup job: failed to index everything.") << endl;
		setErrorText(i18nc("notification", "Failed to index the file system. "
		                   "See log file for more details."));
		setError(ErrorWithLog);
		emitResult();
		return;
	}

	mSaveProcess << QStringLiteral("bup");
	mSaveProcess << QStringLiteral("-d") << mDestinationPath;
	mSaveProcess << QStringLiteral("save");
	mSaveProcess << QStringLiteral("-n") << QStringLiteral("kup");
	mSaveProcess << mBackupPlan.mPathsIncluded;

	connect(&mSaveProcess, SIGNAL(finished(int,QProcess::ExitStatus)), SLOT(slotSavingDone(int,QProcess::ExitStatus)));
	connect(&mSaveProcess, SIGNAL(started()), SLOT(slotSavingStarted()));
	mLogStream << mSaveProcess.program().join(QStringLiteral(" ")) << endl;
	mSaveProcess.start();
}

void BupJob::slotSavingStarted() {
	makeNice(mSaveProcess.pid());
}

void BupJob::slotSavingDone(int pExitCode, QProcess::ExitStatus pExitStatus) {
	mLogStream << QString::fromUtf8(mSaveProcess.readAllStandardError());
	if(pExitStatus != QProcess::NormalExit || pExitCode != 0) {
		mLogStream << endl << QStringLiteral("Kup did not successfully complete the bup backup job: failed to save everything.") << endl;
		setErrorText(i18nc("notification", "Failed to save the complete backup. "
		                   "See log file for more details."));
		setError(ErrorWithLog);
		emitResult();
		return;
	}
	if(mBackupPlan.mGenerateRecoveryInfo) {
		mPar2Process << QStringLiteral("bup");
		mPar2Process << QStringLiteral("-d") << mDestinationPath;
		mPar2Process << QStringLiteral("fsck") << QStringLiteral("-g");

		connect(&mPar2Process, SIGNAL(finished(int,QProcess::ExitStatus)), SLOT(slotRecoveryInfoDone(int,QProcess::ExitStatus)));
		connect(&mPar2Process, SIGNAL(started()), SLOT(slotRecoveryInfoStarted()));
		mLogStream << mPar2Process.program().join(QStringLiteral(" ")) << endl;
		mPar2Process.start();
	} else {
		mLogStream << endl << QStringLiteral("Kup successfully completed the bup backup job.") << endl;
		emitResult();
	}
}

void BupJob::slotRecoveryInfoStarted() {
	makeNice(mPar2Process.pid());
}

void BupJob::slotRecoveryInfoDone(int pExitCode, QProcess::ExitStatus pExitStatus) {
	mLogStream << QString::fromUtf8(mPar2Process.readAllStandardError());
	if(pExitStatus != QProcess::NormalExit || pExitCode != 0) {
		mLogStream << endl << QStringLiteral("Kup did not successfully complete the bup backup job: "
		                                    "failed to generate recovery info.") << endl;
		setErrorText(i18nc("notification", "Failed to generate recovery info for the backup. "
		                   "See log file for more details."));
		setError(ErrorWithLog);
	} else {
		mLogStream << endl << QStringLiteral("Kup successfully completed the bup backup job.") << endl;
	}
	emitResult();
}

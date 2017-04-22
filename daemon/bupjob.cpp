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

#include <signal.h>

#include <QRegularExpression>
#include <QTextStream>

#include <KLocalizedString>

BupJob::BupJob(const BackupPlan &pBackupPlan, const QString &pDestinationPath, const QString &pLogFilePath, KupDaemon *pKupDaemon)
   :BackupJob(pBackupPlan, pDestinationPath, pLogFilePath, pKupDaemon)
{
	mFsckProcess.setOutputChannelMode(KProcess::SeparateChannels);
	mIndexProcess.setOutputChannelMode(KProcess::SeparateChannels);
	mSaveProcess.setOutputChannelMode(KProcess::SeparateChannels);
	mPar2Process.setOutputChannelMode(KProcess::SeparateChannels);
	setCapabilities(KJob::Suspendable);
}

void BupJob::performJob() {
	KProcess lPar2Process;
	lPar2Process.setOutputChannelMode(KProcess::SeparateChannels);
	lPar2Process << QStringLiteral("bup") << QStringLiteral("fsck") << QStringLiteral("--par2-ok");
	int lExitCode = lPar2Process.execute();
	if(lExitCode < 0) {
		jobFinishedError(ErrorWithoutLog, xi18nc("@info notification",
		                                         "The <application>bup</application> program is "
		                                         "needed but could not be found, maybe it is not installed?"));
		return;
	} else if(mBackupPlan.mGenerateRecoveryInfo && lExitCode != 0) {
		jobFinishedError(ErrorWithoutLog, xi18nc("@info notification",
		                                         "The <application>par2</application> program is "
		                                         "needed but could not be found, maybe it is not installed?"));
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
	mLogStream << quoteArgs(lInitProcess.program()) << endl;
	if(lInitProcess.execute() != 0) {
		mLogStream << QString::fromUtf8(lInitProcess.readAllStandardError()) << endl;
		mLogStream << endl << QStringLiteral("Kup did not successfully complete the bup backup job: "
		                                     "failed to initialize backup destination.") << endl;
		jobFinishedError(ErrorWithLog, xi18nc("@info notification", "Backup destination could not be initialised. "
		                                                            "See log file for more details."));
		return;
	}

	if(mBackupPlan.mCheckBackups) {
		mFsckProcess << QStringLiteral("bup");
		mFsckProcess << QStringLiteral("-d") << mDestinationPath;
		mFsckProcess << QStringLiteral("fsck") << QStringLiteral("--quick");

		connect(&mFsckProcess, SIGNAL(finished(int,QProcess::ExitStatus)), SLOT(slotCheckingDone(int,QProcess::ExitStatus)));
		connect(&mFsckProcess, SIGNAL(started()), SLOT(slotCheckingStarted()));
		mLogStream << quoteArgs(mFsckProcess.program()) << endl;
		mFsckProcess.start();
	} else {
		slotCheckingDone(0, QProcess::NormalExit);
	}
}

void BupJob::slotCheckingStarted() {
	makeNice(mFsckProcess.pid());
	emit description(this, i18n("Checking backup integrity"));
}

void BupJob::slotCheckingDone(int pExitCode, QProcess::ExitStatus pExitStatus) {
	mLogStream << QString::fromUtf8(mFsckProcess.readAllStandardError());
	if(pExitStatus != QProcess::NormalExit || pExitCode != 0) {
		mLogStream << endl << QStringLiteral("Kup did not successfully complete the bup backup job: "
		                                     "failed integrity check. Your backups could be "
		                                     "corrupted! See above for details.") << endl;
		if(mBackupPlan.mGenerateRecoveryInfo) {
			jobFinishedError(ErrorSuggestRepair, xi18nc("@info notification",
			                                            "Failed backup integrity check. Your backups could be corrupted! "
			                                            "See log file for more details. Do you want to try repairing the backup files?"));
		} else {
			jobFinishedError(ErrorWithLog, xi18nc("@info notification",
			                                      "Failed backup integrity check. Your backups could be corrupted! "
			                                      "See log file for more details."));
		}
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
	mLogStream << quoteArgs(mIndexProcess.program()) << endl;
	mIndexProcess.start();
}

void BupJob::slotIndexingStarted() {
	makeNice(mIndexProcess.pid());
	emit description(this, i18n("Checking what to copy"));
}

void BupJob::slotIndexingDone(int pExitCode, QProcess::ExitStatus pExitStatus) {
	mLogStream << QString::fromUtf8(mIndexProcess.readAllStandardError());
	if(pExitStatus != QProcess::NormalExit || pExitCode != 0) {
		mLogStream << endl << QStringLiteral("Kup did not successfully complete the bup backup job: failed to index everything.") << endl;
		jobFinishedError(ErrorWithLog, xi18nc("@info notification", "Failed to analyze files. "
		                                                            "See log file for more details."));
		return;
	}
	mSaveProcess << QStringLiteral("bup");
	mSaveProcess << QStringLiteral("-d") << mDestinationPath;
	mSaveProcess << QStringLiteral("save");
	mSaveProcess << QStringLiteral("-n") << QStringLiteral("kup") << QStringLiteral("-vv");
	mSaveProcess << mBackupPlan.mPathsIncluded;
	mLogStream << quoteArgs(mSaveProcess.program()) << endl;

	connect(&mSaveProcess, SIGNAL(finished(int,QProcess::ExitStatus)), SLOT(slotSavingDone(int,QProcess::ExitStatus)));
	connect(&mSaveProcess, SIGNAL(started()), SLOT(slotSavingStarted()));
	connect(&mSaveProcess, &KProcess::readyReadStandardError, this, &BupJob::slotReadBupErrors);

	mSaveProcess.setEnv(QStringLiteral("BUP_FORCE_TTY"), QStringLiteral("2"));
	mSaveProcess.start();
}

void BupJob::slotSavingStarted() {
	makeNice(mSaveProcess.pid());
	emit description(this, i18n("Saving backup"));
}

void BupJob::slotSavingDone(int pExitCode, QProcess::ExitStatus pExitStatus) {
	mLogStream << QString::fromUtf8(mSaveProcess.readAllStandardError());
	if(pExitStatus != QProcess::NormalExit || pExitCode != 0) {
		mLogStream << endl << QStringLiteral("Kup did not successfully complete the bup backup job: "
		                                     "failed to save everything.") << endl;
		jobFinishedError(ErrorWithLog, xi18nc("@info notification", "Failed to save backup. "
		                                                            "See log file for more details."));
		return;
	}
	if(mBackupPlan.mGenerateRecoveryInfo) {
		mPar2Process << QStringLiteral("bup");
		mPar2Process << QStringLiteral("-d") << mDestinationPath;
		mPar2Process << QStringLiteral("fsck") << QStringLiteral("-g");

		connect(&mPar2Process, SIGNAL(finished(int,QProcess::ExitStatus)), SLOT(slotRecoveryInfoDone(int,QProcess::ExitStatus)));
		connect(&mPar2Process, SIGNAL(started()), SLOT(slotRecoveryInfoStarted()));
		mLogStream << quoteArgs(mPar2Process.program()) << endl;
		mPar2Process.start();
	} else {
		mLogStream << endl << QStringLiteral("Kup successfully completed the bup backup job at ")
		           << QLocale().toString(QDateTime::currentDateTime()) << endl;
		jobFinishedSuccess();
	}
}

void BupJob::slotRecoveryInfoStarted() {
	makeNice(mPar2Process.pid());
	emit description(this, i18n("Generating recovery information"));
}

void BupJob::slotRecoveryInfoDone(int pExitCode, QProcess::ExitStatus pExitStatus) {
	mLogStream << QString::fromUtf8(mPar2Process.readAllStandardError());
	if(pExitStatus != QProcess::NormalExit || pExitCode != 0) {
		mLogStream << endl << QStringLiteral("Kup did not successfully complete the bup backup job: "
		                                     "failed to generate recovery info.") << endl;
		jobFinishedError(ErrorWithLog, xi18nc("@info notification", "Failed to generate recovery info for the backup. "
		                                                            "See log file for more details."));
	} else {
		mLogStream << endl << QStringLiteral("Kup successfully completed the bup backup job.") << endl;
		jobFinishedSuccess();
	}
}

void BupJob::slotReadBupErrors() {
	bool lValidInfo = false, lValidFileName = false;
	qulonglong lCopiedKBytes, lTotalKBytes, lCopiedFiles, lTotalFiles;
	ulong lSpeedKBps, lPercent = 0;
	QString lFileName;

	QTextStream lStream(mSaveProcess.readAllStandardError());
	QString lLine;
	while(lStream.readLineInto(&lLine, 500)) {
//		mLogStream << "read a line: " << lLine << endl;
		if(lLine.startsWith(QStringLiteral("Reading index:"))) {
			continue;
		} else if(lLine.startsWith(QStringLiteral("bloom:"))) {
			continue;
		} else if(lLine.startsWith(QStringLiteral("midx:"))) {
			continue;
		} else if(lLine.startsWith(QStringLiteral("Saving:"))) {
			QRegularExpression lRegExp(QStringLiteral("(\\d+)/(\\d+)k, (\\d+)/(\\d+) files\\) \\S* (?:(\\d+)k/s|)"));
			QRegularExpressionMatch lMatch = lRegExp.match(lLine);
			if(lMatch.hasMatch()) {
				lCopiedKBytes = lMatch.captured(1).toULongLong();
				lTotalKBytes = lMatch.captured(2).toULongLong();
				lCopiedFiles = lMatch.captured(3).toULongLong();
				lTotalFiles = lMatch.captured(4).toULongLong();
				lSpeedKBps = lMatch.captured(5).toULong();
				if(lTotalKBytes != 0) {
					lPercent = qMax(100*lCopiedKBytes/lTotalKBytes, (qulonglong)1);
				}
				lValidInfo = true;
			}
		} else if((lLine.at(0) == ' ' || lLine.at(0) == 'A' || lLine.at(0) == 'M') && lLine.at(1) == ' ' && lLine.at(2) == '/') {
			lLine.remove(0, 2);
			lFileName = lLine;
			lValidFileName = true;
		} else {
			mLogStream << lLine << endl;
		}
	}
	if(lValidInfo) {
		setPercent(lPercent);
		setTotalAmount(KJob::Bytes, lTotalKBytes*1024);
		setTotalAmount(KJob::Files, lTotalFiles);
		setProcessedAmount(KJob::Bytes, lCopiedKBytes*1024);
		setProcessedAmount(KJob::Files, lCopiedFiles);
		emitSpeed(lSpeedKBps * 1024);
	}
	if(lValidFileName) {
		emit description(this, i18n("Saving backup"),
		                 qMakePair(i18nc("Label for file currently being copied", "File"), lFileName));
	}
}

bool BupJob::doSuspend() {
	if(mFsckProcess.state() == KProcess::Running) {
		return 0 == ::kill(mFsckProcess.pid(), SIGSTOP);
	}
	if(mIndexProcess.state() == KProcess::Running) {
		return 0 == ::kill(mIndexProcess.pid(), SIGSTOP);
	}
	if(mSaveProcess.state() == KProcess::Running) {
		return 0 == ::kill(mSaveProcess.pid(), SIGSTOP);
	}
	if(mPar2Process.state() == KProcess::Running) {
		return 0 == ::kill(mPar2Process.pid(), SIGSTOP);
	}
	return false;
}

bool BupJob::doResume() {
	if(mFsckProcess.state() == KProcess::Running) {
		return 0 == ::kill(mFsckProcess.pid(), SIGCONT);
	}
	if(mIndexProcess.state() == KProcess::Running) {
		return 0 == ::kill(mIndexProcess.pid(), SIGCONT);
	}
	if(mSaveProcess.state() == KProcess::Running) {
		return 0 == ::kill(mSaveProcess.pid(), SIGCONT);
	}
	if(mPar2Process.state() == KProcess::Running) {
		return 0 == ::kill(mPar2Process.pid(), SIGCONT);
	}
	return false;
}

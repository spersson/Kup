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

#include <KGlobal>
#include <KLocale>
#include <QDir>
#include <QTimer>

BupJob::BupJob(const BackupPlan &pBackupPlan, const QString &pDestinationPath, const QString &pLogFilePath)
   :BackupJob(pBackupPlan, pDestinationPath, pLogFilePath)
{
	mIndexProcess.setOutputChannelMode(KProcess::SeparateChannels);
	mSaveProcess.setOutputChannelMode(KProcess::SeparateChannels);
	mFsckProcess.setOutputChannelMode(KProcess::SeparateChannels);
}

void BupJob::start() {
	QTimer::singleShot(0, this, SLOT(startIndexing()));
}

void BupJob::startIndexing() {
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
	mBupVersion = QString::fromUtf8(lVersionProcess.readAllStandardOutput());

	mLogStream << QLatin1String("Kup is starting bup backup job at ")
	           << KGlobal::locale()->formatDateTime(QDateTime::currentDateTime(),
	                                                KLocale::LongDate, true)
	           << endl;

	KProcess lInitProcess;
	lInitProcess.setOutputChannelMode(KProcess::SeparateChannels);
	lInitProcess << QLatin1String("bup");
	lInitProcess << QLatin1String("-d") << mDestinationPath;
	lInitProcess << QLatin1String("init");
	mLogStream << lInitProcess.program().join(QLatin1String(" ")) << endl;
	if(lInitProcess.execute() != 0) {
		mLogStream << QString::fromUtf8(lInitProcess.readAllStandardError()) << endl;
		mLogStream << QLatin1String("Kup is aborting the bup backup job.") << endl;
		setError(ErrorWithLog);
		setErrorText(i18nc("notification", "Backup destination could not be initialised. "
		                   "See log file for more details."));
		emitResult();
		return;
	}

	mIndexProcess << QLatin1String("bup");
	mIndexProcess << QLatin1String("-d") << mDestinationPath;
	mIndexProcess << QLatin1String("index") << QLatin1String("-u");

	foreach(QString lExclude, mBackupPlan.mPathsExcluded) {
		mIndexProcess << QLatin1String("--exclude");
		mIndexProcess << lExclude;
	}
	mIndexProcess << mBackupPlan.mPathsIncluded;

	connect(&mIndexProcess, SIGNAL(finished(int,QProcess::ExitStatus)), SLOT(slotIndexingDone(int,QProcess::ExitStatus)));
	connect(&mIndexProcess, SIGNAL(started()), SLOT(slotIndexingStarted()));
	mLogStream << mIndexProcess.program().join(QLatin1String(" ")) << endl;
	mIndexProcess.start();
}

void BupJob::slotIndexingStarted() {
	makeNice(mIndexProcess.pid());
}

void BupJob::slotIndexingDone(int pExitCode, QProcess::ExitStatus pExitStatus) {
	mLogStream << QString::fromUtf8(mIndexProcess.readAllStandardError()) << endl;
	if(pExitStatus != QProcess::NormalExit || pExitCode != 0) {
		mLogStream << QLatin1String("Kup did not successfully complete the bup backup job: failed to index everything.") << endl;
		setError(ErrorWithLog);
		setErrorText(i18nc("notification", "Failed to index the file system. "
		                   "See log file for more details."));
		emitResult();
		return;
	}

	mSaveProcess << QLatin1String("bup");
	mSaveProcess << QLatin1String("-d") << mDestinationPath;
	mSaveProcess << QLatin1String("save");
	mSaveProcess << QLatin1String("-n") << QLatin1String("kup");
	mSaveProcess << mBackupPlan.mPathsIncluded;

	connect(&mSaveProcess, SIGNAL(finished(int,QProcess::ExitStatus)), SLOT(slotSavingDone(int,QProcess::ExitStatus)));
	connect(&mSaveProcess, SIGNAL(started()), SLOT(slotSavingStarted()));
	mLogStream << mSaveProcess.program().join(QLatin1String(" ")) << endl;
	mSaveProcess.start();
}

void BupJob::slotSavingStarted() {
	makeNice(mSaveProcess.pid());
}

void BupJob::slotSavingDone(int pExitCode, QProcess::ExitStatus pExitStatus) {
	mLogStream << QString::fromUtf8(mSaveProcess.readAllStandardError()) << endl;
	if(pExitStatus != QProcess::NormalExit || pExitCode != 0) {
		mLogStream << QLatin1String("Kup did not successfully complete the bup backup job: failed to save everything.") << endl;
		setError(ErrorWithLog);
		setErrorText(i18nc("notification", "Failed to save the complete backup. "
		                   "See log file for more details."));
		emitResult();
		return;
	}
	if(mBackupPlan.mGenerateRecoveryInfo) {
		mFsckProcess << QLatin1String("bup");
		mFsckProcess << QLatin1String("-d") << mDestinationPath;
		mFsckProcess << QLatin1String("fsck");
		mFsckProcess << QLatin1String("-g");

		connect(&mFsckProcess, SIGNAL(finished(int,QProcess::ExitStatus)), SLOT(slotRecoveryInfoDone(int,QProcess::ExitStatus)));
		connect(&mFsckProcess, SIGNAL(started()), SLOT(slotRecoveryInfoStarted()));
		mLogStream << mFsckProcess.program().join(QLatin1String(" ")) << endl;
		mFsckProcess.start();
	} else {
		mLogStream << QLatin1String("Kup successfully completed the bup backup job.") << endl;
		emitResult();
	}
}

void BupJob::slotRecoveryInfoStarted() {
	makeNice(mFsckProcess.pid());
}

void BupJob::slotRecoveryInfoDone(int pExitCode, QProcess::ExitStatus pExitStatus) {
	mLogStream << QString::fromUtf8(mFsckProcess.readAllStandardError()) << endl;
	if(pExitStatus != QProcess::NormalExit || pExitCode != 0) {
		mLogStream << QLatin1String("Kup did not successfully complete the bup backup job: failed to generate recovery info.") << endl;
		setError(ErrorWithLog);
		setErrorText(i18nc("notification", "Failed to generate recovery info for the backup. "
		                   "See log file for more details."));
	} else {
		mLogStream << QLatin1String("Kup successfully completed the bup backup job.") << endl;
	}
	emitResult();
}

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

#include "rsyncjob.h"

#include <QTimer>

#include <KLocalizedString>

RsyncJob::RsyncJob(const BackupPlan &pBackupPlan, const QString &pDestinationPath, const QString &pLogFilePath)
   :BackupJob(pBackupPlan, pDestinationPath, pLogFilePath)
{
	mRsyncProcess.setOutputChannelMode(KProcess::SeparateChannels);
}

void RsyncJob::start() {
	QTimer::singleShot(0, this, SLOT(startRsync()));
}

void RsyncJob::startRsync() {
	KProcess lVersionProcess;
	lVersionProcess.setOutputChannelMode(KProcess::SeparateChannels);
	lVersionProcess << QStringLiteral("rsync") << QStringLiteral("--version");
	if(lVersionProcess.execute() < 0) {
		setError(ErrorWithoutLog);
		setErrorText(xi18nc("@info notification",
		                    "The <application>rsync</application> program is needed but could not be found, "
		                    "maybe it is not installed?"));
		emitResult();
		return;
	}

	mLogStream << QStringLiteral("Kup is starting rsync backup job at ")
	           << QLocale().toString(QDateTime::currentDateTime())
	           << endl;

	mRsyncProcess << QStringLiteral("rsync") << QStringLiteral("-aR");
	mRsyncProcess << QStringLiteral("--delete") << QStringLiteral("--delete-excluded");
	foreach(QString lExclude, mBackupPlan.mPathsExcluded) {
		mRsyncProcess << QString(QStringLiteral("--exclude=%1")).arg(lExclude);
	}
	mRsyncProcess << mBackupPlan.mPathsIncluded;
	mRsyncProcess << mDestinationPath;

	connect(&mRsyncProcess, SIGNAL(started()), SLOT(slotRsyncStarted()));
	connect(&mRsyncProcess, SIGNAL(finished(int,QProcess::ExitStatus)), SLOT(slotRsyncFinished(int,QProcess::ExitStatus)));
	mLogStream << quoteArgs(mRsyncProcess.program()) << endl;
	mRsyncProcess.start();
}

void RsyncJob::slotRsyncStarted() {
	makeNice(mRsyncProcess.pid());
}

void RsyncJob::slotRsyncFinished(int pExitCode, QProcess::ExitStatus pExitStatus) {
	mLogStream << QString::fromUtf8(mRsyncProcess.readAllStandardError());
	if(pExitStatus != QProcess::NormalExit || pExitCode != 0) {
		mLogStream << endl << QStringLiteral("Kup did not successfully complete the rsync backup job.") << endl;
		setErrorText(xi18nc("@info notification", "Saving backup did not complete successfully. "
		                                          "See log file for more details."));
		setError(ErrorWithLog);
	} else {
		mLogStream << endl << QStringLiteral("Kup successfully completed the rsync backup job at ")
		           << QLocale().toString(QDateTime::currentDateTime()) << endl;

	}
	emitResult();
}


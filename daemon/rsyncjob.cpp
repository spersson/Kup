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

#include <KGlobal>
#include <KLocale>
#include <QTimer>

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
	lVersionProcess << QLatin1String("rsync") << QLatin1String("--version");
	if(lVersionProcess.execute() < 0) {
		setError(ErrorWithoutLog);
		setErrorText(i18nc("notification", "The \"rsync\" program is needed but could not be found, "
		                   "maybe it is not installed?"));
		emitResult();
		return;
	}

	mLogStream << QLatin1String("Kup is starting rsync backup job at ")
	           << KGlobal::locale()->formatDateTime(QDateTime::currentDateTime(),
	                                                KLocale::LongDate, true)
	           << endl;

	mRsyncProcess << QLatin1String("rsync") << QLatin1String("-aR");
	mRsyncProcess << QLatin1String("--delete") << QLatin1String("--delete-excluded");
	foreach(QString lExclude, mBackupPlan.mPathsExcluded) {
		mRsyncProcess << QString::fromLatin1("--exclude=%1").arg(lExclude);
	}
	mRsyncProcess << mBackupPlan.mPathsIncluded;
	mRsyncProcess << mDestinationPath;

	connect(&mRsyncProcess, SIGNAL(started()), SLOT(slotRsyncStarted()));
	connect(&mRsyncProcess, SIGNAL(finished(int,QProcess::ExitStatus)), SLOT(slotRsyncFinished(int,QProcess::ExitStatus)));
	mLogStream << mRsyncProcess.program().join(QLatin1String(" ")) << endl;
	mRsyncProcess.start();
}

void RsyncJob::slotRsyncStarted() {
	makeNice(mRsyncProcess.pid());
}

void RsyncJob::slotRsyncFinished(int pExitCode, QProcess::ExitStatus pExitStatus) {
	mLogStream << QString::fromUtf8(mRsyncProcess.readAllStandardError()) << endl;
	if(pExitStatus != QProcess::NormalExit || pExitCode != 0) {
		mLogStream << QLatin1String("Kup did not successfully complete the rsync backup job.") << endl;
		setError(ErrorWithLog);
		setErrorText(i18nc("notification", "Saving backup did not complete successfully. "
		                   "See log file for more details."));
	} else {
		mLogStream << QLatin1String("Kup successfully completed the rsync backup job.") << endl;
	}
	emitResult();
}


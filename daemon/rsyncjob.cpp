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

#include <KLocale>

#include <QTimer>

RsyncJob::RsyncJob(const QStringList &pPathsIncluded, const QStringList &pPathsExcluded,
                   const QString &pDestinationPath, bool pRunAsRoot)
   :BackupJob(pPathsIncluded, pPathsExcluded, pDestinationPath, pRunAsRoot)
{
	mRsyncProcess.setOutputChannelMode(KProcess::SeparateChannels);
}

void RsyncJob::start() {
	if(mRunAsRoot) {
		QVariantMap lArguments;
		startRootHelper(lArguments, BackupPlan::RsyncType);
	} else {
		QTimer::singleShot(0, this, SLOT(startRsync()));
	}
}

void RsyncJob::startRsync() {
	KProcess lVersionProcess;
	lVersionProcess.setOutputChannelMode(KProcess::SeparateChannels);
	lVersionProcess << QLatin1String("rsync") << QLatin1String("--version");
	if(lVersionProcess.execute() < 0) {
		setError(1);
		setErrorText(i18nc("notification", "The \"rsync\" program is needed but could not be found, "
		                   "maybe it is not installed?"));
		emitResult();
		return;
	}

	mRsyncProcess << QLatin1String("rsync") << QLatin1String("-aR");
	mRsyncProcess << QLatin1String("--delete") << QLatin1String("--delete-excluded");
	foreach(QString lExclude, mPathsExcluded) {
		mRsyncProcess << QString::fromLatin1("--exclude=%1").arg(lExclude);
	}
	mRsyncProcess << mPathsIncluded;
	mRsyncProcess << mDestinationPath;

	connect(&mRsyncProcess, SIGNAL(started()), SLOT(slotRsyncStarted()));
	connect(&mRsyncProcess, SIGNAL(finished(int,QProcess::ExitStatus)), SLOT(slotRsyncFinished(int,QProcess::ExitStatus)));
	mRsyncProcess.start();
}

void RsyncJob::slotRsyncStarted() {
	makeNice(mRsyncProcess.pid());
}

void RsyncJob::slotRsyncFinished(int pExitCode, QProcess::ExitStatus pExitStatus) {
	if(pExitStatus != QProcess::NormalExit || pExitCode != 0) {
		setError(1);
		setErrorText(i18nc("notification", "Backup did not complete successfully:\n%1",
		                   QString::fromLocal8Bit(mRsyncProcess.readAllStandardError())));
	}
	emitResult();
}


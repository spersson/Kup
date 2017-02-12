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
#include "kuputils.h"

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

	mRsyncProcess << QStringLiteral("rsync") << QStringLiteral("-aX") << QStringLiteral("--delete-excluded");
	// TODO: use --info=progress2 --no-i-r parameters to get progress info
	//	mRsyncProcess << QStringLiteral("--info=progress2") << QStringLiteral("--no-i-r");

	QStringList lIncludeNames;
	foreach(const QString &lInclude, mBackupPlan.mPathsIncluded) {
		lIncludeNames << lastPartOfPath(lInclude);
	}
	if(lIncludeNames.removeDuplicates() > 0) {
		// There would be a naming conflict in the destination folder, instead use full paths.
		mRsyncProcess << QStringLiteral("-R");
		foreach(const QString &lExclude, mBackupPlan.mPathsExcluded) {
			mRsyncProcess << QStringLiteral("--exclude=") + lExclude;
		}
	} else {
		// when NOT using -R, need to then strip parent paths from excludes, everything above the include. Leave the leading slash!
		foreach(QString lExclude, mBackupPlan.mPathsExcluded) {
			for(int i = 0; i < mBackupPlan.mPathsIncluded.length(); ++i) {
				const QString &lInclude = mBackupPlan.mPathsIncluded.at(i);
				QString lIncludeWithSlash = lInclude;
				ensureTrailingSlash(lIncludeWithSlash);
				if(lExclude.startsWith(lIncludeWithSlash)) {
					if(mBackupPlan.mPathsIncluded.length() == 1) {
						lExclude.remove(0, lInclude.length());
					} else {
						lExclude.remove(0, lInclude.length() - lIncludeNames.at(i).length() - 1);
					}
					break;
				}
			}
			mRsyncProcess << QStringLiteral("--exclude=") + lExclude;
		}
	}

	if(mBackupPlan.mPathsIncluded.count() == 1) {
		// Add a slash to the end so that rsync takes only the contents of the folder, not the folder itself.
		QString lInclude = mBackupPlan.mPathsIncluded.first();
		ensureTrailingSlash(lInclude);
		mRsyncProcess << lInclude;
	} else  {
		mRsyncProcess << mBackupPlan.mPathsIncluded;
	}

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


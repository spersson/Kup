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

#include <signal.h>

#include <QRegularExpression>
#include <QTextStream>

#include <KLocalizedString>



RsyncJob::RsyncJob(const BackupPlan &pBackupPlan, const QString &pDestinationPath,
                   const QString &pLogFilePath, KupDaemon *pKupDaemon)
   :BackupJob(pBackupPlan, pDestinationPath, pLogFilePath, pKupDaemon)
{
	mRsyncProcess.setOutputChannelMode(KProcess::SeparateChannels);
	setCapabilities(KJob::Suspendable | KJob::Killable);
}

void RsyncJob::performJob() {
	KProcess lVersionProcess;
	lVersionProcess.setOutputChannelMode(KProcess::SeparateChannels);
	lVersionProcess << QStringLiteral("rsync") << QStringLiteral("--version");
	if(lVersionProcess.execute() < 0) {
		jobFinishedError(ErrorWithoutLog,
		                 xi18nc("@info notification",
		                        "The <application>rsync</application> program is needed but "
		                        "could not be found, maybe it is not installed?"));
		return;
	}

	mLogStream << QStringLiteral("Kup is starting rsync backup job at ")
	           << QLocale().toString(QDateTime::currentDateTime())
	           << endl;

	emit description(this, i18n("Checking what to copy"));
	mRsyncProcess << QStringLiteral("rsync") << QStringLiteral("-avX")
	              << QStringLiteral("--delete-excluded");
	mRsyncProcess << QStringLiteral("--info=progress2") << QStringLiteral("--no-i-r");

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
		// when NOT using -R, need to then strip parent paths from excludes, everything above the
		// include. Leave the leading slash!
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
	connect(&mRsyncProcess, &KProcess::readyReadStandardOutput, this, &RsyncJob::slotReadRsyncOutput);
	connect(&mRsyncProcess, SIGNAL(finished(int,QProcess::ExitStatus)),
	        SLOT(slotRsyncFinished(int,QProcess::ExitStatus)));
	mLogStream << quoteArgs(mRsyncProcess.program()) << endl;
	mRsyncProcess.start();
	mInfoRateLimiter.start();
}

void RsyncJob::slotRsyncStarted() {
	makeNice(mRsyncProcess.pid());
}

void RsyncJob::slotRsyncFinished(int pExitCode, QProcess::ExitStatus pExitStatus) {
	mLogStream << QString::fromUtf8(mRsyncProcess.readAllStandardError());
	if(pExitStatus != QProcess::NormalExit || pExitCode != 0) {
		mLogStream << endl << QStringLiteral("Kup did not successfully complete the rsync backup job.") << endl;
		jobFinishedError(ErrorWithLog, xi18nc("@info notification", "Failed to save backup. "
		                                                            "See log file for more details."));
	} else {
		mLogStream << endl << QStringLiteral("Kup successfully completed the rsync backup job at ")
		           << QLocale().toString(QDateTime::currentDateTime()) << endl;
		jobFinishedSuccess();
	}
}

void RsyncJob::slotReadRsyncOutput() {
	bool lValidInfo = false, lValidFileName = false;
	QString lFileName;
	ulong lPercent;
	qulonglong lTransfered;
	double lSpeed;
	QChar lUnit;
	QRegularExpression lProgressInfoExp(QStringLiteral("^\\s+([\\d,\\.]+)\\s+(\\d+)%\\s+(\\d*[,\\.]\\d+)(\\S)"));
	// very ugly and rough indication that this is a file path... what else to do..
	QRegularExpression lNotFileNameExp(QStringLiteral("^(building file list|done$|deleting \\S+|.+/$|$)"));
	QString lLine;

	QTextStream lStream(mRsyncProcess.readAllStandardOutput());
	while(lStream.readLineInto(&lLine, 500)) {
		QRegularExpressionMatch lMatch = lProgressInfoExp.match(lLine);
		if(lMatch.hasMatch()) {
			lValidInfo = true;
			lTransfered = lMatch.captured(1).remove(',').remove('.').toULongLong();
			lPercent = qMax(lMatch.captured(2).toULong(), (ulong)1);
			lSpeed = QLocale().toDouble(lMatch.captured(3));
			lUnit = lMatch.captured(4).at(0);
		} else {
			lMatch = lNotFileNameExp.match(lLine);
			if(!lMatch.hasMatch()) {
				lValidFileName = true;
				lFileName = lLine;
			}
		}
	}
	if(mInfoRateLimiter.hasExpired(200)) {
		if(lValidInfo) {
			setPercent(lPercent);
			if(lUnit == 'k') {
				lSpeed *= 1e3;
			} else if(lUnit == 'M') {
				lSpeed *= 1e6;
			} else if(lUnit == 'G') {
				lSpeed *= 1e9;
			}
			emitSpeed(lSpeed);
			if(lPercent > 5) { // the rounding to integer percent gives big error with small percentages
				setProcessedAmount(KJob::Bytes, lTransfered);
				setTotalAmount(KJob::Bytes, lTransfered*100/lPercent);
			}
		}
		if(lValidFileName) {
			emit description(this, i18n("Saving backup"),
			                 qMakePair(i18nc("Label for file currently being copied", "File"), lFileName));
		}
		mInfoRateLimiter.start();
	}
}

bool RsyncJob::doKill() {
	setError(KilledJobError);
	return 0 == ::kill(mRsyncProcess.pid(), SIGINT);
}

bool RsyncJob::doSuspend() {
	return 0 == ::kill(mRsyncProcess.pid(), SIGSTOP);
}

bool RsyncJob::doResume() {
	return 0 == ::kill(mRsyncProcess.pid(), SIGCONT);
}


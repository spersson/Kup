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

#include <QDir>
#include <QDebug>
#include <KLocalizedString>

#include "restorejob.h"

#include <unistd.h>
#include <sys/resource.h>
#ifdef Q_OS_LINUX
#include <sys/syscall.h>
#endif

RestoreJob::RestoreJob(const QString &pRepositoryPath, const QString &pSourcePath, const QString &pRestorationPath,
                       int pTotalDirCount, quint64 pTotalFileSize, const QHash<QString, quint64> &pFileSizes)
 : KJob(), mRepositoryPath(pRepositoryPath), mSourcePath(pSourcePath), mRestorationPath(pRestorationPath),
   mTotalDirCount(pTotalDirCount), mTotalFileSize(pTotalFileSize), mFileSizes(pFileSizes)
{
	setCapabilities(Killable);
	mRestoreProcess.setOutputChannelMode(KProcess::SeparateChannels);
	int lOffset = mSourcePath.endsWith(QDir::separator()) ? -2: -1;
	mSourceFileName = mSourcePath.section(QDir::separator(), lOffset, lOffset);
}

void RestoreJob::start() {
	setTotalAmount(Bytes, mTotalFileSize);
	setProcessedAmount(Bytes, 0);
	setTotalAmount(Files, mFileSizes.count());
	setProcessedAmount(Files, 0);
	setTotalAmount(Directories, mTotalDirCount);
	setProcessedAmount(Directories, 0);
	setPercent(0);
	mRestoreProcess << QStringLiteral("bup");
	mRestoreProcess << QStringLiteral("-d") << mRepositoryPath;
	mRestoreProcess << QStringLiteral("restore") << QStringLiteral("-vv");
	mRestoreProcess << QStringLiteral("-C") << mRestorationPath;
	mRestoreProcess << mSourcePath;
	connect(&mRestoreProcess, SIGNAL(started()), SLOT(slotRestoringStarted()));
	connect(&mRestoreProcess, SIGNAL(finished(int,QProcess::ExitStatus)), SLOT(slotRestoringDone(int,QProcess::ExitStatus)));
	mRestoreProcess.start();
	mTimerId = startTimer(100);
}

void RestoreJob::slotRestoringStarted() {
	makeNice(mRestoreProcess.pid());
}

void RestoreJob::timerEvent(QTimerEvent *pTimerEvent) {
	Q_UNUSED(pTimerEvent)
	quint64 lProcessedDirectories = processedAmount(Directories);
	quint64 lProcessedFiles = processedAmount(Files);
	quint64 lProcessedBytes = processedAmount(Bytes);
	bool lDirWasUpdated = false;
	bool lFileWasUpdated = false;
	QString lLastFileName;

	while(mRestoreProcess.canReadLine()) {
		QString lFileName = QString::fromLocal8Bit(mRestoreProcess.readLine()).trimmed();
		if(lFileName.count() == 0) {
			break;
		}
		if(lFileName.endsWith(QLatin1Char('/'))) { // it's a directory
			lProcessedDirectories++;
			lDirWasUpdated = true;
		} else {
			if(mSourcePath.endsWith(QDir::separator())) {
				lFileName.prepend(QDir::separator());
				lFileName.prepend(mSourceFileName);
			}
			lProcessedBytes += mFileSizes.value(lFileName);
			lProcessedFiles++;
			lLastFileName = lFileName;
			lFileWasUpdated = true;
		}
	}
	if(lDirWasUpdated) {
		setProcessedAmount(Directories, lProcessedDirectories);
	}
	if(lFileWasUpdated) {
		emit description(this, i18nc("progress report, current operation", "Restoring"),
		                 qMakePair(i18nc("progress report, label", "File:"), lLastFileName));
		setProcessedAmount(Files, lProcessedFiles);
		setProcessedAmount(Bytes, lProcessedBytes); // this will also call emitPercent()
	}
}

void RestoreJob::slotRestoringDone(int pExitCode, QProcess::ExitStatus pExitStatus) {
	killTimer(mTimerId);
	if(pExitStatus != QProcess::NormalExit || pExitCode != 0) {
		setError(1);
		setErrorText(QString::fromUtf8(mRestoreProcess.readAllStandardError()));
	}
	emitResult();
}

void RestoreJob::makeNice(int pPid) {
#ifdef Q_OS_LINUX
	// See linux documentation Documentation/block/ioprio.txt for details of the syscall
	syscall(SYS_ioprio_set, 1, pPid, 3 << 13 | 7);
#endif
	setpriority(PRIO_PROCESS, pPid, 19);
}


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
#include "backupplan.h"

#include <KLocale>

#include <QDir>
#include <QTimer>

#include <unistd.h>
#include <sys/resource.h>
#ifdef Q_OS_LINUX
#include <sys/syscall.h>
#endif

static void makeNice(int pPid) {
#ifdef Q_OS_LINUX
	// See linux documentation Documentation/block/ioprio.txt for details of the syscall
	syscall(SYS_ioprio_set, 1, pPid, 3 << 13 | 7);
#endif
	setpriority(PRIO_PROCESS, pPid, 19);
}

BupJob::BupJob(const QStringList &pPathsIncluded, const QStringList &pPathsExcluded,
               const QString &pDestinationPath, int pCompressionLevel, bool pRunAsRoot,
               QObject *pParent)
   :KJob(pParent), mPathsIncluded(pPathsIncluded), mPathsExcluded(pPathsExcluded),
     mDestinationPath(pDestinationPath), mCompressionLevel(pCompressionLevel),
     mRunAsRoot(pRunAsRoot)
{
	mIndexProcess.setOutputChannelMode(KProcess::SeparateChannels);
	mSaveProcess.setOutputChannelMode(KProcess::SeparateChannels);
}

void BupJob::start() {
	if(mRunAsRoot) {
		Action lAction(QLatin1String("org.kde.kup.runner.takebackup"));
		lAction.setHelperID(QLatin1String("org.kde.kup.runner"));
		connect(lAction.watcher(), SIGNAL(actionPerformed(ActionReply)), SLOT(slotHelperDone(ActionReply)));
		QVariantMap lArguments;
		lArguments[QLatin1String("pathsIncluded")] = mPathsIncluded;
		lArguments[QLatin1String("pathsExcluded")] = mPathsExcluded;
		lArguments[QLatin1String("destinationPath")] = mDestinationPath;
		lArguments[QLatin1String("compressionLevel")] = mCompressionLevel;
		lArguments[QLatin1String("bupPath")] = QDir::homePath() + QDir::separator() + QLatin1String(".bup");
		lArguments[QLatin1String("uid")] = (uint)geteuid();
		lArguments[QLatin1String("gid")] = (uint)getegid();
		lAction.setArguments(lArguments);
		ActionReply lReply = lAction.execute();
		if(checkForError(lReply)) {
			emitResult();
		}
	} else {
		QTimer::singleShot(0, this, SLOT(startIndexing()));
	}
}

void BupJob::startIndexing() {
	KProcess lVersionProcess;
	lVersionProcess.setOutputChannelMode(KProcess::SeparateChannels);
	lVersionProcess << QLatin1String("bup") << QLatin1String("version");
	if(lVersionProcess.execute() < 0) {
		setError(1);
		setErrorText(i18nc("@info", "The \"bup\" program is needed but could not be found, "
		                   "maybe it is not installed?"));
		emitResult();
		return;
	}
	mBupVersion = lVersionProcess.readAllStandardOutput();

	KProcess lInitProcess;
	lInitProcess.setOutputChannelMode(KProcess::SeparateChannels);
	lInitProcess << QLatin1String("bup");
	lInitProcess << QLatin1String("-d") << mDestinationPath;
	lInitProcess << QLatin1String("init");
	if(lInitProcess.execute() != 0) {
		setError(1);
		setErrorText(i18nc("@info", "Backup destination could not be initialised by bup:</nl>"
		                   "<message>%1</message>", QString(lInitProcess.readAllStandardError())));
		emitResult();
		return;
	}

	mIndexProcess << QLatin1String("bup");
	if(!mBupPath.isEmpty()) {
		mIndexProcess << QLatin1String("-d") << mBupPath;
	}
	mIndexProcess << QLatin1String("index") << QLatin1String("-u");

	foreach(QString lExclude, mPathsExcluded) {
		mIndexProcess << QLatin1String("--exclude");
		mIndexProcess << lExclude;
	}
	mIndexProcess << mPathsIncluded;

	connect(&mIndexProcess, SIGNAL(finished(int,QProcess::ExitStatus)), SLOT(slotIndexingDone(int,QProcess::ExitStatus)));
	connect(&mIndexProcess, SIGNAL(started()), SLOT(slotIndexingStarted()));
	mIndexProcess.start();
	emit description(this, i18nc("@info:progress", "Checking what has changed since last backup..."));
}

void BupJob::slotIndexingStarted() {
	makeNice(mIndexProcess.pid());
}

void BupJob::slotIndexingDone(int pExitCode, QProcess::ExitStatus pExitStatus) {
	if(pExitStatus != QProcess::NormalExit || pExitCode != 0) {
		setError(1);
		setErrorText(i18nc("@info", "Indexing of file system did not complete successfully:</nl>"
		                   "<message>%1</message>", QString(mIndexProcess.readAllStandardError())));
		emitResult();
		return;
	}

	mSaveProcess << QLatin1String("bup");
	if(!mBupPath.isEmpty()) {
		mSaveProcess << QLatin1String("-d") << mBupPath;
	}
	mSaveProcess << QLatin1String("save");
	mSaveProcess << QLatin1String("-n") << QLatin1String("kup");
	mSaveProcess << QLatin1String("-r") << mDestinationPath;
	if(mBupVersion >= QLatin1String("0.25")) {
		mSaveProcess << QString("-%1").arg(mCompressionLevel);
	}
	mSaveProcess << mPathsIncluded;

	connect(&mSaveProcess, SIGNAL(finished(int,QProcess::ExitStatus)), SLOT(slotSavingDone(int,QProcess::ExitStatus)));
	connect(&mSaveProcess, SIGNAL(started()), SLOT(slotSavingStarted()));
	mSaveProcess.start();
	emit description(this, i18nc("@info:progress", "Saving changes to backup destination..."));
}

void BupJob::slotSavingStarted() {
	makeNice(mSaveProcess.pid());
}

void BupJob::slotSavingDone(int pExitCode, QProcess::ExitStatus pExitStatus) {
	if(pExitStatus != QProcess::NormalExit || pExitCode != 0) {
		setError(1);
		setErrorText(i18nc("@info", "Backup did not complete successfully:</nl>"
		                   "<message>%1</message>", QString(mSaveProcess.readAllStandardError())));
	}
	emitResult();
}

void BupJob::slotHelperDone(ActionReply pReply) {
	checkForError(pReply);
	emitResult();
}

bool BupJob::checkForError(ActionReply pReply) {
	if(pReply.failed()) {
		setError(1);
		if(pReply.type() == ActionReply::KAuthError) {
			if(pReply.errorCode() != ActionReply::UserCancelled) {
				setErrorText(i18nc("@info", "Failed to take backup as root: %1", pReply.errorDescription()));
			}
		} else {
			setErrorText(pReply.errorDescription());
		}
		return true;
	}
	return false;
}


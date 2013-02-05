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

#include <QTimer>

#ifdef Q_OS_LINUX
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/resource.h>
static void makeNice(int pPid) {
	// See linux documentation Documentation/block/ioprio.txt for details of the syscall
	syscall(SYS_ioprio_set, 1, pPid, 3 << 13 | 7);
	setpriority(PRIO_PROCESS, pPid, 19);
}
#else
static void makeNice(int) {}
#endif

BupJob::BupJob(const BackupPlan *pBackupPlan, const QString &pDestinationPath, QObject *pParent)
   :KJob(pParent), mBackupPlan(pBackupPlan), mDestinationPath(pDestinationPath)
{
	mInitProcess.setOutputChannelMode(KProcess::SeparateChannels);
	mIndexProcess.setOutputChannelMode(KProcess::SeparateChannels);
	mSaveProcess.setOutputChannelMode(KProcess::SeparateChannels);
}

void BupJob::start() {
	QTimer::singleShot(0, this, SLOT(startIndexing()));
}

void BupJob::startIndexing() {
	mInitProcess << QLatin1String("bup");
	mInitProcess << QLatin1String("-d") << mDestinationPath;
	mInitProcess << QLatin1String("init");

	if(mInitProcess.execute() != 0) {
		setError(1);
		setErrorText(i18nc("@info", "Backup destination could not be initialised by bup:</nl>"
		                   "<message>%1</message>", QString(mInitProcess.readAllStandardError())));
		emitResult();
		return;
	}

	mIndexProcess << QLatin1String("bup") << QLatin1String("index") << QLatin1String("-u");

	foreach(QString lExclude, mBackupPlan->mPathsExcluded) {
		mIndexProcess << QLatin1String("--exclude");
		mIndexProcess << lExclude;
	}

	foreach(QString lInclude, mBackupPlan->mPathsIncluded) {
		mIndexProcess << lInclude;
	}

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

	mSaveProcess << QLatin1String("bup") << QLatin1String("save");
	mSaveProcess << QLatin1String("-n") << QLatin1String("kup");
	mSaveProcess << QLatin1String("-r") << mDestinationPath;
	mSaveProcess << QString("-%1").arg(mBackupPlan->mCompressionLevel);

	foreach(QString lInclude, mBackupPlan->mPathsIncluded) {
		mSaveProcess << lInclude;
	}

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


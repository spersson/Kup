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

#include <KLocale>

#include <QDir>
#include <QTimer>

BupJob::BupJob(const QStringList &pPathsIncluded, const QStringList &pPathsExcluded,
               const QString &pDestinationPath, bool pRunAsRoot, const QString &pBupPath)
   :BackupJob(pPathsIncluded, pPathsExcluded, pDestinationPath, pRunAsRoot),
     mBupPath(pBupPath)
{
	mIndexProcess.setOutputChannelMode(KProcess::SeparateChannels);
	mSaveProcess.setOutputChannelMode(KProcess::SeparateChannels);
}

void BupJob::start() {
	if(mRunAsRoot) {
		QVariantMap lArguments;
		lArguments[QLatin1String("bupPath")] = QDir::homePath() + QDir::separator() + QLatin1String(".bup");
		startRootHelper(lArguments, BackupPlan::BupType);
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


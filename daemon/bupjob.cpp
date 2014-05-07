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
               const QString &pDestinationPath)
   :BackupJob(pPathsIncluded, pPathsExcluded, pDestinationPath)
{
	mIndexProcess.setOutputChannelMode(KProcess::SeparateChannels);
	mSaveProcess.setOutputChannelMode(KProcess::SeparateChannels);
}

void BupJob::start() {
	QTimer::singleShot(0, this, SLOT(startIndexing()));
}

void BupJob::startIndexing() {
	KProcess lVersionProcess;
	lVersionProcess.setOutputChannelMode(KProcess::SeparateChannels);
	lVersionProcess << QLatin1String("bup") << QLatin1String("version");
	if(lVersionProcess.execute() < 0) {
		setError(1);
		setErrorText(i18nc("notification", "The \"bup\" program is needed but could not be found, "
		                   "maybe it is not installed?"));
		emitResult();
		return;
	}
	mBupVersion = QString::fromUtf8(lVersionProcess.readAllStandardOutput());

	KProcess lInitProcess;
	lInitProcess.setOutputChannelMode(KProcess::SeparateChannels);
	lInitProcess << QLatin1String("bup");
	lInitProcess << QLatin1String("-d") << mDestinationPath;
	lInitProcess << QLatin1String("init");
	if(lInitProcess.execute() != 0) {
		setError(1);
		setErrorText(i18nc("notification", "Backup destination could not be initialised by bup:\n%1",
		                   QString::fromUtf8(lInitProcess.readAllStandardError())));
		emitResult();
		return;
	}

	mIndexProcess << QLatin1String("bup");
	mIndexProcess << QLatin1String("-d") << mDestinationPath;
	mIndexProcess << QLatin1String("index") << QLatin1String("-u");

	foreach(QString lExclude, mPathsExcluded) {
		mIndexProcess << QLatin1String("--exclude");
		mIndexProcess << lExclude;
	}
	mIndexProcess << mPathsIncluded;

	connect(&mIndexProcess, SIGNAL(finished(int,QProcess::ExitStatus)), SLOT(slotIndexingDone(int,QProcess::ExitStatus)));
	connect(&mIndexProcess, SIGNAL(started()), SLOT(slotIndexingStarted()));
	mIndexProcess.start();
}

void BupJob::slotIndexingStarted() {
	makeNice(mIndexProcess.pid());
}

void BupJob::slotIndexingDone(int pExitCode, QProcess::ExitStatus pExitStatus) {
	if(pExitStatus != QProcess::NormalExit || pExitCode != 0) {
		setError(1);
		setErrorText(i18nc("notification", "Indexing of file system did not complete successfully:\n%1",
		                   QString::fromUtf8(mIndexProcess.readAllStandardError())));
		emitResult();
		return;
	}

	mSaveProcess << QLatin1String("bup");
	mSaveProcess << QLatin1String("-d") << mDestinationPath;
	mSaveProcess << QLatin1String("save");
	mSaveProcess << QLatin1String("-n") << QLatin1String("kup");
	mSaveProcess << mPathsIncluded;

	connect(&mSaveProcess, SIGNAL(finished(int,QProcess::ExitStatus)), SLOT(slotSavingDone(int,QProcess::ExitStatus)));
	connect(&mSaveProcess, SIGNAL(started()), SLOT(slotSavingStarted()));
	mSaveProcess.start();
}

void BupJob::slotSavingStarted() {
	makeNice(mSaveProcess.pid());
}

void BupJob::slotSavingDone(int pExitCode, QProcess::ExitStatus pExitStatus) {
	if(pExitStatus != QProcess::NormalExit || pExitCode != 0) {
		setError(1);
		setErrorText(i18nc("notification", "Backup did not complete successfully:\n%1",
		                   QString::fromUtf8(mSaveProcess.readAllStandardError())));
	}
	emitResult();
}


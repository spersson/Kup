/***************************************************************************
 *   Copyright Simon Persson                                               *
 *   simonop@spray.se                                                      *
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
//#include "processlistener.h"
#include "backupplan.h"

#include <QDebug>
#include <KLocale>

#include <QTimer>

BupJob::BupJob(const BackupPlan *pPlan, const QString &pDestinationPath, QObject *pParent)
   :KJob(pParent), mPlan(pPlan), mDestinationPath(pDestinationPath)
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
		emit warning(this, i18n("Backup destination could not be initialised by bup."));
		emitResult();
		return;
	}

	mIndexProcess << QLatin1String("bup");
	mIndexProcess << QLatin1String("-d") << mDestinationPath;
	mIndexProcess << QLatin1String("index");

	foreach(QString lExclude, mPlan->mPathsExcluded) {
		mIndexProcess << QLatin1String("--exclude");
		mIndexProcess << lExclude;
	}

	foreach(QString lInclude, mPlan->mPathsIncluded) {
		mIndexProcess << lInclude;
	}

	connect(&mIndexProcess, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(slotIndexingDone(int,QProcess::ExitStatus)));
	qDebug() << mIndexProcess.program();
	mIndexProcess.start();
	emit description(this, i18n("Checking what has changed since last backup..."));
}

void BupJob::slotIndexingDone(int pExitCode, QProcess::ExitStatus pExitStatus) {
	if(pExitStatus != QProcess::NormalExit || pExitCode != 0)
		emit warning(this, i18n("Indexing of file system did not complete successfully: %1",
		                        QString(mIndexProcess.readAllStandardError())));

	mSaveProcess.clearProgram();
	mSaveProcess << QLatin1String("bup");
	mSaveProcess << QLatin1String("-d") << mDestinationPath;
	mSaveProcess << QLatin1String("save");
	mSaveProcess << QLatin1String("-n") << QLatin1String("kup");

	foreach(QString lInclude, mPlan->mPathsIncluded) {
		mSaveProcess << lInclude;
	}

	connect(&mSaveProcess, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(slotSavingDone(int,QProcess::ExitStatus)));
	qDebug() << mSaveProcess.program();
//	mProcessListener = new ProcessListener(&mSaveProcess);
	mSaveProcess.start();
	emit description(this, i18n("Saving changes to backup..."));
}

void BupJob::slotSavingDone(int pExitCode, QProcess::ExitStatus pExitStatus) {
//	qDebug() << mProcessListener->stdOut();
	if(pExitStatus != QProcess::NormalExit || pExitCode != 0)
		emit warning(this, i18n("Backup did not complete successfully: %1",
		                        QString(mSaveProcess.readAllStandardError())));
	emitResult();
}


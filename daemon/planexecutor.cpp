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

#include "planexecutor.h"

#include <KLocale>
#include <KProcess>
#include <KRun>
#include <KStandardDirs>
#include <KTempDir>
#include <kuiserverjobtracker.h>

#include <QAction>
#include <QDir>
#include <QMenu>

PlanExecutor::PlanExecutor(BackupPlan *pPlan, QObject *pParent)
   :QObject(pParent), mDestinationAvailable(false), mRunning(false), mPlan(pPlan),
     mBupFuseProcess(NULL)
{
	mJobTracker = new KUiServerJobTracker(this);
	mShowFilesAction = new QAction(i18n("Show Files"), this);
	mShowFilesAction->setCheckable(true);
	connect(mShowFilesAction, SIGNAL(triggered()), this, SLOT(showFiles()));
	mRunBackupAction = new QAction(i18n("Take Backup Now"), this);
	connect(mRunBackupAction, SIGNAL(triggered()), this, SLOT(startBackup()));
	mActionMenu = new QMenu(mPlan->mDescription);
	mActionMenu->addAction(mRunBackupAction);
	mActionMenu->addAction(mShowFilesAction);
}

void PlanExecutor::checkStatus() {

}

void PlanExecutor::startBackup() {

}

void PlanExecutor::showFiles() {
	if(!mDestinationAvailable)
		return;
	if(mBupFuseProcess || !mShowFilesAction->isChecked()) {
		unmountBupFuse();
	} else {
		mTempDir = KStandardDirs::locateLocal("tmp", mPlan->mDescription + "/");
		mBupFuseProcess = new KProcess(this);

		*mBupFuseProcess << QLatin1String("bup");
		*mBupFuseProcess << QLatin1String("-d") << mDestinationPath;
		*mBupFuseProcess << QLatin1String("fuse") << QLatin1String("-f");
		*mBupFuseProcess << mTempDir;

		connect(mBupFuseProcess, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(bupFuseFinished(int,QProcess::ExitStatus)));
		mBupFuseProcess->start();

		KRun::runUrl(KUrl(mTempDir + "kup"), QLatin1String("inode/directory"), 0);
		mShowFilesAction->setChecked(true);
	}
}

void PlanExecutor::bupFuseFinished(int pExitCode, QProcess::ExitStatus pExitStatus) {
	Q_UNUSED(pExitCode)
	Q_UNUSED(pExitStatus)

	mShowFilesAction->setChecked(false);
	mBupFuseProcess->deleteLater();
	mBupFuseProcess = NULL;
	QDir lDir;
	lDir.rmpath(mTempDir);
}

void PlanExecutor::unmountBupFuse() {
	QStringList lUmount;
	lUmount << QLatin1String("fusermount") << QLatin1String("-u") <<mTempDir;
	KProcess::execute(lUmount);
}


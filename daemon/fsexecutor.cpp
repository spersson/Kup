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

#include "fsexecutor.h"
#include "backupplan.h"
#include "bupjob.h"

#include <kio/directorysizejob.h>
#include <KDirWatch>
#include <KDiskFreeSpaceInfo>
#include <KLocale>
#include <KNotification>

#include <QAction>
#include <QDir>
#include <QFileInfo>
#include <QTimer>

FSExecutor::FSExecutor(BackupPlan *pPlan, QObject *pParent)
   :PlanExecutor(pPlan, pParent)
{
	mDestinationPath = QDir::cleanPath(mPlan->mFilesystemDestinationPath.toLocalFile());
	mDirWatch = new KDirWatch(this);
	connect(mDirWatch, SIGNAL(deleted(QString)), SLOT(checkStatus()));
}

void FSExecutor::checkStatus() {
	static bool lComingBackLater = false;
	if(!mWatchedParentDir.isEmpty() && !lComingBackLater) {
		// came here because something happened to a parent folder,
		// come back in a few seconds, give a new mount some time before checking
		// status of destination folder
		QTimer::singleShot(5000, this, SLOT(checkStatus()));
		lComingBackLater = true;
		return;
	}
	lComingBackLater = false;

	QDir lDir(mDestinationPath);
	if(!lDir.exists()) {
		// Destination doesn't exist, find nearest existing parent folder and
		// watch that for dirty or deleted
		if(mDirWatch->contains(mDestinationPath)) {
			mDirWatch->removeDir(mDestinationPath);
		}

		QString lExisting = mDestinationPath;
		do {
			lExisting += QLatin1String("/..");
			lDir = QDir(QDir::cleanPath(lExisting));
		} while(!lDir.exists());
		lExisting = lDir.canonicalPath();

		if(lExisting != mWatchedParentDir) { // new parent to watch
			if(!mWatchedParentDir.isEmpty()) { // were already watching a parent
				mDirWatch->removeDir(mWatchedParentDir);
			} else { // start watching a parent
				connect(mDirWatch, SIGNAL(dirty(QString)), SLOT(checkStatus()));
			}
			mWatchedParentDir = lExisting;
			mDirWatch->addDir(mWatchedParentDir);
		}
		if(mState != NOT_AVAILABLE) {
			enterNotAvailableState();
		}
	} else {
		// Destination exists... only watch for delete
		if(!mWatchedParentDir.isEmpty()) {
			disconnect(mDirWatch, SIGNAL(dirty(QString)), this, SLOT(checkStatus()));
			mDirWatch->removeDir(mWatchedParentDir);
			mWatchedParentDir.clear();
		}
		mDirWatch->addDir(mDestinationPath);

		QFileInfo lInfo(mDestinationPath);
		if(lInfo.isWritable() && mState == NOT_AVAILABLE) {
			enterAvailableState();
		}else if(!lInfo.isWritable() && mState != NOT_AVAILABLE) {
			enterNotAvailableState();
		}
	}
}

void FSExecutor::startBackup() {
	BupJob *lJob = new BupJob(mPlan->mPathsIncluded, mPlan->mPathsExcluded, mDestinationPath,
	                          mPlan->mCompressionLevel, mPlan->mRunAsRoot, this);
	connect(lJob, SIGNAL(result(KJob*)), SLOT(slotBackupDone(KJob*)));
	lJob->start();
}

void FSExecutor::slotBackupDone(KJob *pJob) {
	if(pJob->error()) {
		KNotification::event(KNotification::Error, i18nc("@title", "Problem"), pJob->errorText());
		exitBackupRunningState(false);
	} else {
		mPlan->mLastCompleteBackup = QDateTime::currentDateTime().toUTC();
		KDiskFreeSpaceInfo lSpaceInfo = KDiskFreeSpaceInfo::freeSpaceInfo(mDestinationPath);
		if(lSpaceInfo.isValid())
			mPlan->mLastAvailableSpace = (double)lSpaceInfo.available();
		else
			mPlan->mLastAvailableSpace = -1.0; //unknown size

		KIO::DirectorySizeJob *lSizeJob = KIO::directorySize(mDestinationPath);
		connect(lSizeJob, SIGNAL(result(KJob*)), SLOT(slotBackupSizeDone(KJob*)));
		lSizeJob->start();
	}
}

void FSExecutor::slotBackupSizeDone(KJob *pJob) {
	if(pJob->error()) {
		KNotification::event(KNotification::Error, i18nc("@title", "Problem"), pJob->errorText());
		mPlan->mLastBackupSize = -1.0; //unknown size
	} else {
		KIO::DirectorySizeJob *lSizeJob = qobject_cast<KIO::DirectorySizeJob *>(pJob);
		mPlan->mLastBackupSize = (double)lSizeJob->totalSize();
	}
	mPlan->writeConfig();
	exitBackupRunningState(pJob->error() == 0);
}

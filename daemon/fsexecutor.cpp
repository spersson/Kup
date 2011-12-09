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

#include "fsexecutor.h"
#include "backupplan.h"
#include "bupjob.h"

#include <kio/directorysizejob.h>
#include <KDirWatch>
#include <KDiskFreeSpaceInfo>
#include <KLocale>
#include <KNotification>
#include <kuiserverjobtracker.h>

#include <QAction>
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QTimer>

FSExecutor::FSExecutor(BackupPlan *pPlan, QObject *pParent)
   :PlanExecutor(pPlan, pParent), mQuestion(NULL)
{
	mRunBackupTimer = new QTimer(this);
	mRunBackupTimer->setSingleShot(true);
	connect(mRunBackupTimer, SIGNAL(timeout()), this, SLOT(startBackup()));

	mAskUserTimer = new QTimer(this);
	mAskUserTimer->setSingleShot(true);
	connect(mAskUserTimer, SIGNAL(timeout()), this, SLOT(askUserToStartBackup()));

	mDestinationPath = QDir::cleanPath(mPlan->mFilesystemDestinationPath.toLocalFile());

	KDirWatch::self()->addDir(mDestinationPath);
	connect(KDirWatch::self(), SIGNAL(created(QString)), SLOT(checkStatus()));
	connect(KDirWatch::self(), SIGNAL(deleted(QString)), SLOT(checkStatus()));
}

void FSExecutor::checkStatus() {
	checkAccessibility();
	mShowFilesAction->setEnabled(mDestinationAvailable);
	mRunBackupAction->setEnabled(mDestinationAvailable);
	mActionMenu->setEnabled(mDestinationAvailable);
	if(mDestinationAvailable)
		askUserToStartBackup();
}

void FSExecutor::checkAccessibility() {
	bool lPrevStatus = mDestinationAvailable;

	if(KDirWatch::self()->contains(mWatchedParentDir)) {
		KDirWatch::self()->removeDir(mWatchedParentDir);
		disconnect(KDirWatch::self(), SIGNAL(dirty(QString)), this, SLOT(checkStatus()));
		mWatchedParentDir.clear();
	}

	QDir lDir(mDestinationPath);
	if(!lDir.exists()) {
		mDestinationAvailable = false;
		QString lExisting = mDestinationPath;
		do {
			lExisting += QLatin1String("/..");
			lDir = QDir(QDir::cleanPath(lExisting));
		} while(!lDir.exists());

		mWatchedParentDir = lDir.canonicalPath();
		if(!KDirWatch::self()->contains(mWatchedParentDir)) {
			KDirWatch::self()->addDir(mWatchedParentDir);
			connect(KDirWatch::self(), SIGNAL(dirty(QString)), SLOT(checkStatus()));
		}
	} else {
		QFileInfo lInfo(mDestinationPath);
		mDestinationAvailable = lInfo.isWritable();
	}
	if(lPrevStatus != mDestinationAvailable) {
		emit statusUpdated();
	}
}

void FSExecutor::askUserToStartBackup() {
	if(mPlan->mScheduleType == 1 && mQuestion == NULL) {
		QDateTime lNextTime = mPlan->nextScheduledTime();
		QDateTime lNow = QDateTime::currentDateTimeUtc();
		if(!lNextTime.isValid() || lNextTime < lNow) {
			mQuestion = new KNotification(QLatin1String("StartBackup"), KNotification::Persistent);
			mQuestion->setTitle(i18n("Backup Device Available"));
			if(!mPlan->mLastCompleteBackup.isValid())
				mQuestion->setText(i18n("Do you want to take a first backup now?"));
			else {
				mQuestion->setText(i18n("It's been %1 since the last backup was taken, do you want to take a backup now?",
				                        KGlobal::locale()->prettyFormatDuration(mPlan->mLastCompleteBackup.secsTo(lNow) * 1000)));
			}
			QStringList lAnswers;
			lAnswers << i18n("Yes") <<i18n("No");
			mQuestion->setActions(lAnswers);
			connect(mQuestion, SIGNAL(action1Activated()), this, SLOT(startBackup()));
			connect(mQuestion, SIGNAL(action2Activated()), this, SLOT(discardNotification()));
			mQuestion->sendEvent();
		} else {
			// schedule a wakeup for asking again when the time is right.
			mAskUserTimer->start(lNow.msecsTo(lNextTime));
		}
	} else if(mPlan->mScheduleType == 2) {
		//run continous without question
	}
}

void FSExecutor::discardNotification() {
	mQuestion->deleteLater();
	mQuestion = NULL;
}

void FSExecutor::startBackup() {
	if(mQuestion) {
		mQuestion->deleteLater();
		mQuestion = NULL;
	}

	BupJob *lJob = new BupJob(mPlan, mDestinationPath, this);
	mJobTracker->registerJob(lJob);
	connect(lJob, SIGNAL(result(KJob*)), this, SLOT(slotBackupDone(KJob*)));
	lJob->start();
	mRunning = true;
	emit statusUpdated();
}

void FSExecutor::slotBackupDone(KJob *pJob) {
	if(pJob->error() == 0) {
		mPlan->mLastCompleteBackup = QDateTime::currentDateTimeUtc();
		KDiskFreeSpaceInfo lSpaceInfo = KDiskFreeSpaceInfo::freeSpaceInfo(mDestinationPath);
		if(lSpaceInfo.isValid())
			mPlan->mLastAvailableSpace = (double)lSpaceInfo.available();
		else
			mPlan->mLastAvailableSpace = -1.0; //unknown size

		KIO::DirectorySizeJob *lSizeJob = KIO::directorySize(mDestinationPath);
		connect(lSizeJob, SIGNAL(result(KJob*)), this, SLOT(slotBackupSizeDone(KJob*)));
		lSizeJob->start();

		QDateTime lNextTime = mPlan->nextScheduledTime();
		mRunBackupTimer->start(mPlan->mLastCompleteBackup.msecsTo(lNextTime));
	}
	mRunning = false;
	emit statusUpdated();
}

void FSExecutor::slotBackupSizeDone(KJob *pJob) {
	if(pJob->error() == 0) {
		KIO::DirectorySizeJob *lSizeJob = qobject_cast<KIO::DirectorySizeJob *>(pJob);
		mPlan->mLastBackupSize = (double)lSizeJob->totalSize();
	} else {
		mPlan->mLastBackupSize = -1.0; //unknown size
	}
	mPlan->writeConfig();
}


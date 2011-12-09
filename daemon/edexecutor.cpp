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

#include "edexecutor.h"
#include "backupplan.h"
#include "bupjob.h"

#include <kio/directorysizejob.h>
#include <KDiskFreeSpaceInfo>
#include <KLocale>
#include <KNotification>
#include <kuiserverjobtracker.h>

#include <Solid/DeviceNotifier>
#include <Solid/DeviceInterface>
#include <Solid/StorageDrive>
#include <Solid/StorageVolume>

#include <QAction>
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QTimer>

EDExecutor::EDExecutor(BackupPlan *pPlan, QObject *pParent)
   :PlanExecutor(pPlan, pParent), mStorageAccess(NULL), mWantsToRunBackup(false)
{
	mRunBackupTimer = new QTimer(this);
	mRunBackupTimer->setSingleShot(true);
	connect(mRunBackupTimer, SIGNAL(timeout()), this, SLOT(startBackup()));

	mAskUserTimer = new QTimer(this);
	mAskUserTimer->setSingleShot(true);
	connect(mAskUserTimer, SIGNAL(timeout()), this, SLOT(askUserToStartBackup()));

	connect(Solid::DeviceNotifier::instance(), SIGNAL(deviceAdded(QString)),
	        this, SLOT(deviceAdded(QString)));
	connect(Solid::DeviceNotifier::instance(), SIGNAL(deviceRemoved(QString)),
	        this, SLOT(deviceRemoved(QString)));
}

void EDExecutor::checkStatus() {
	QList<Solid::Device> lDeviceList = Solid::Device::listFromType(Solid::DeviceInterface::StorageVolume);
	foreach (const Solid::Device &lDevice, lDeviceList) {
		deviceAdded(lDevice.udi());
	}
	updateAccessibility();
}

void EDExecutor::deviceAdded(const QString &pUdi) {
	Solid::Device lDevice(pUdi);
	if(!lDevice.isValid() || !lDevice.is<Solid::StorageVolume>()) {
		mDestinationAvailable = false;
		return;
	}
	Solid::StorageVolume *lVolume = lDevice.as<Solid::StorageVolume>();
	if(mPlan->mExternalUUID == lVolume->uuid())
	{
		mCurrentUdi = pUdi;
		mStorageAccess = lDevice.as<Solid::StorageAccess>();
		connect(mStorageAccess, SIGNAL(accessibilityChanged(bool,QString)),
		        this, SLOT(updateAccessibility()));

		askUserToStartBackup();
	}
}

void EDExecutor::deviceRemoved(const QString &pUdi) {
	if(mCurrentUdi == pUdi) {
		mCurrentUdi.clear();
		mStorageAccess = NULL;
		updateAccessibility();
	}
}

void EDExecutor::checkAccessibility() {
	bool lPrevStatus = mDestinationAvailable;
	if(mStorageAccess && mStorageAccess->isAccessible()) {
		if(mStorageAccess->filePath().isEmpty()) {
			mDestinationAvailable = false;
		} else {
			mDestinationPath = QDir::cleanPath(mStorageAccess->filePath() + '/' + mPlan->mExternalDestinationPath);
			QDir lDir(mDestinationPath);
			if(!lDir.exists()) lDir.mkdir(mDestinationPath);
			QFileInfo lInfo(mDestinationPath);
			mDestinationAvailable = lInfo.isWritable();
		}
	} else {
		mDestinationAvailable = false;
	}

	if(lPrevStatus != mDestinationAvailable) emit statusUpdated();
}

void EDExecutor::updateAccessibility() {
	checkAccessibility();

	if(mDestinationAvailable) {
		mShowFilesAction->setEnabled(true);
		mRunBackupAction->setEnabled(true);
		mActionMenu->setEnabled(true);
		if(mWantsToRunBackup) startBackupJob();
	} else {
		mAskUserTimer->stop();
		mRunBackupTimer->stop();
		mShowFilesAction->setEnabled(false);
		mRunBackupAction->setEnabled(false);
		mActionMenu->setEnabled(false);
	}
}

void EDExecutor::askUserToStartBackup() {
	mWantsToRunBackup = false;

	if(mPlan->mScheduleType == 1) {
		QDateTime lNow = QDateTime::currentDateTimeUtc();
		QDateTime lNextTime = mPlan->nextScheduledTime();
		if(!lNextTime.isValid() || lNextTime < lNow) {
			mQuestion = new KNotification(QLatin1String("StartBackup"), KNotification::Persistent);
			mQuestion->setTitle(i18n("Backup Device Available"));
			if(!mPlan->mLastCompleteBackup.isValid())
				mQuestion->setText(i18n("Do you want to take a first backup now?"));
			else {
				QString t = KGlobal::locale()->prettyFormatDuration(mPlan->mLastCompleteBackup.secsTo(lNow) * 1000);
				mQuestion->setText(i18n("It's been %1 since the last backup was taken, "
				                        "do you want to take a backup now?", t));
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

void EDExecutor::discardNotification() {
	mQuestion->deleteLater();
	mQuestion = NULL;
}

void EDExecutor::startBackup() {
	if(mQuestion) {
		mQuestion->deleteLater();
		mQuestion = NULL;
	}

	if(mDestinationAvailable) {
		startBackupJob();
	} else if(mStorageAccess) {
		mWantsToRunBackup = true;
		mStorageAccess->setup(); //try to mount it, fail silently.
	}
}

void EDExecutor::startBackupJob() {
	BupJob *lJob = new BupJob(mPlan, mDestinationPath, this);
	mJobTracker->registerJob(lJob);
	connect(lJob, SIGNAL(result(KJob*)), this, SLOT(slotBackupDone(KJob*)));
	lJob->start();
	mRunning = true;
	emit statusUpdated();
}

void EDExecutor::slotBackupDone(KJob *pJob) {
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
	mWantsToRunBackup = false;
	mRunning = false;
	emit statusUpdated();
}

void EDExecutor::slotBackupSizeDone(KJob *pJob) {
	if(pJob->error() == 0) {
		KIO::DirectorySizeJob *lSizeJob = qobject_cast<KIO::DirectorySizeJob *>(pJob);
		mPlan->mLastBackupSize = (double)lSizeJob->totalSize();
	} else {
		mPlan->mLastBackupSize = -1.0; //unknown size
	}
	mPlan->writeConfig();
}

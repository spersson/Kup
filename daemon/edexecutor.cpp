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

#include "edexecutor.h"
#include "backupplan.h"

#include <QAction>
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QTimer>

#include <KDiskFreeSpaceInfo>
#include <KIO/DirectorySizeJob>
#include <KLocalizedString>
#include <KNotification>

#include <Solid/DeviceNotifier>
#include <Solid/DeviceInterface>
#include <Solid/StorageDrive>
#include <Solid/StorageVolume>

EDExecutor::EDExecutor(BackupPlan *pPlan, KupDaemon *pKupDaemon)
   :PlanExecutor(pPlan, pKupDaemon), mStorageAccess(NULL), mWantsToRunBackup(false), mWantsToShowFiles(false)
{
	connect(Solid::DeviceNotifier::instance(), SIGNAL(deviceAdded(QString)), SLOT(deviceAdded(QString)));
	connect(Solid::DeviceNotifier::instance(), SIGNAL(deviceRemoved(QString)), SLOT(deviceRemoved(QString)));
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
	if(!lDevice.is<Solid::StorageVolume>()) {
		return;
	}
	Solid::StorageVolume *lVolume = lDevice.as<Solid::StorageVolume>();
	QString lUUID = lVolume->uuid();
	if(lUUID.isEmpty()) { //seems to happen for vfat partitions
		Solid::Device lDriveDevice;
		if(lDevice.is<Solid::StorageDrive>()) {
			lDriveDevice = lDevice;
		} else {
			lDriveDevice = lDevice.parent();
		}
		lUUID += lDriveDevice.description();
		lUUID += QStringLiteral("|");
		lUUID += lVolume->label();
	}
	if(mPlan->mExternalUUID == lUUID) {
		mCurrentUdi = pUdi;
		mStorageAccess = lDevice.as<Solid::StorageAccess>();
		enterAvailableState();
	}
}

void EDExecutor::deviceRemoved(const QString &pUdi) {
	if(mCurrentUdi == pUdi) {
		mWantsToRunBackup = false;
		mCurrentUdi.clear();
		mStorageAccess = NULL;
		enterNotAvailableState();
	}
}

void EDExecutor::updateAccessibility() {
	if(mWantsToRunBackup) {
		startBackup(); // run startBackup again now that it has been mounted
	} else if(mWantsToShowFiles) {
		showFilesClicked();
	}
}

void EDExecutor::startBackup() {
	if(!mStorageAccess) {
		exitBackupRunningState(false);
		return;
	}
	if(mStorageAccess->isAccessible()) {
		if(!mStorageAccess->filePath().isEmpty()) {
			mDestinationPath = mStorageAccess->filePath();
			mDestinationPath += QStringLiteral("/");
			mDestinationPath += mPlan->mExternalDestinationPath;
			QDir lDir(mDestinationPath);
			if(!lDir.exists()) {
				lDir.mkdir(mDestinationPath);
			}
			QFileInfo lInfo(mDestinationPath);
			if(lInfo.isWritable()) {
				BackupJob *lJob = createBackupJob();
				if(lJob == NULL) {
					KNotification::event(KNotification::Error, xi18nc("@title:window", "Problem"),
					                     xi18nc("notification", "Invalid type of backup in configuration."));
					exitBackupRunningState(false);
					return;
				}
				connect(lJob, SIGNAL(result(KJob*)), SLOT(slotBackupDone(KJob*)));
				lJob->start();
				mWantsToRunBackup = false; //reset, only used to retrigger this state-entering if drive wasn't already mounted
			} else {
				KNotification::event(KNotification::Error, xi18nc("@title:window", "Problem"),
				                     xi18nc("notification", "You don't have write permission to backup destination."));
				exitBackupRunningState(false);
				return;
			}
		}
	} else { //not mounted yet. trigger mount and come back to this startBackup again later
		mWantsToRunBackup = true;
		connect(mStorageAccess, SIGNAL(accessibilityChanged(bool,QString)), SLOT(updateAccessibility()));
		mStorageAccess->setup(); //try to mount it, fail silently for now.
	}
}

void EDExecutor::slotBackupDone(KJob *pJob) {
	if(pJob->error()) {
		if(pJob->error() != KJob::KilledJobError) {
			notifyBackupFailed(pJob);
		}
		exitBackupRunningState(false);
	} else {
		mPlan->mLastCompleteBackup = QDateTime::currentDateTime().toUTC();
		KDiskFreeSpaceInfo lSpaceInfo = KDiskFreeSpaceInfo::freeSpaceInfo(mDestinationPath);
		if(lSpaceInfo.isValid())
			mPlan->mLastAvailableSpace = (double)lSpaceInfo.available();
		else
			mPlan->mLastAvailableSpace = -1.0; //unknown size

		KIO::DirectorySizeJob *lSizeJob = KIO::directorySize(QUrl::fromLocalFile(mDestinationPath));
		connect(lSizeJob, SIGNAL(result(KJob*)), SLOT(slotBackupSizeDone(KJob*)));
		lSizeJob->start();
	}
}

void EDExecutor::slotBackupSizeDone(KJob *pJob) {
	if(pJob->error()) {
		KNotification::event(KNotification::Error, xi18nc("@title:window", "Problem"), pJob->errorText());
		mPlan->mLastBackupSize = -1.0; //unknown size
	} else {
		KIO::DirectorySizeJob *lSizeJob = qobject_cast<KIO::DirectorySizeJob *>(pJob);
		mPlan->mLastBackupSize = (double)lSizeJob->totalSize();
	}
	mPlan->save();
	exitBackupRunningState(pJob->error() == 0);
}

void EDExecutor::showFilesClicked() {
	if(!mStorageAccess)
		return;

	if(mStorageAccess->isAccessible()) {
		if(!mStorageAccess->filePath().isEmpty()) {
			mDestinationPath = mStorageAccess->filePath();
			mDestinationPath += QStringLiteral("/");
			mDestinationPath += mPlan->mExternalDestinationPath;
			QFileInfo lDestinationInfo(mDestinationPath);
			if(lDestinationInfo.exists() && lDestinationInfo.isDir()) {
				mWantsToShowFiles = false; //reset, only used to retrigger this state-entering if drive wasn't already mounted
				PlanExecutor::showFilesClicked();
			}
		}
	} else { //not mounted yet. trigger mount and come back to this startBackup again later
		mWantsToShowFiles = true;
		connect(mStorageAccess, SIGNAL(accessibilityChanged(bool,QString)), SLOT(updateAccessibility()));
		mStorageAccess->setup(); //try to mount it, fail silently for now.
	}
}

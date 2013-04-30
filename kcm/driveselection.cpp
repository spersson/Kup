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

#include "driveselection.h"
#include "driveselectiondelegate.h"
#include "backupplan.h"

#include <QItemSelectionModel>
#include <QList>
#include <QStandardItemModel>
#include <QTimer>

#include <KConfigDialogManager>
#include <KDiskFreeSpaceInfo>
#include <KLocale>
#include <Solid/Device>
#include <Solid/DeviceNotifier>
#include <Solid/StorageAccess>
#include <Solid/StorageDrive>
#include <Solid/StorageVolume>

bool deviceLessThan(const Solid::Device &a, const Solid::Device &b) {
	return a.udi() < b.udi();
}

DriveSelection::DriveSelection(BackupPlan *pBackupPlan, QWidget *parent)
   : QListView(parent), mBackupPlan(pBackupPlan)
{
	KConfigDialogManager::changedMap()->insert("DriveSelection", SIGNAL(driveSelectionChanged()));

	mDrivesModel = new QStandardItemModel(this);
	setModel(mDrivesModel);
	setItemDelegate(new DriveSelectionDelegate(this));
	setSelectionMode(QAbstractItemView::SingleSelection);

	QList<Solid::Device> lDeviceList = Solid::Device::listFromType(Solid::DeviceInterface::StorageDrive);
	foreach (const Solid::Device &lDevice, lDeviceList) {
		deviceAdded(lDevice.udi());
	}
	connect(Solid::DeviceNotifier::instance(), SIGNAL(deviceAdded(QString)), SLOT(deviceAdded(QString)));
	connect(Solid::DeviceNotifier::instance(), SIGNAL(deviceRemoved(QString)), SLOT(deviceRemoved(QString)));
	connect(selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
	        this, SLOT(updateSelection(QItemSelection,QItemSelection)));
}


void DriveSelection::deviceAdded(const QString &pUdi) {
	Solid::Device lDevice(pUdi);
	if(!lDevice.is<Solid::StorageDrive>()) {
		return;
	}
	Solid::StorageDrive *lDrive = lDevice.as<Solid::StorageDrive>();
	if(!lDrive->isHotpluggable()) {
		return;
	}
	if(mDrivesToAdd.contains(pUdi)) {
		return;
	}
	mDrivesToAdd.append(pUdi);
	QTimer::singleShot(2000, this, SLOT(delayedDeviceAdded()));
}

void DriveSelection::delayedDeviceAdded() {
	if(mDrivesToAdd.isEmpty()) {
		return;
	}
	Solid::Device lParentDevice(mDrivesToAdd.takeFirst());
	QList<Solid::Device> lDeviceList = Solid::Device::listFromType(Solid::DeviceInterface::StorageVolume,
	                                                               lParentDevice.udi());
	// check for when there is no partitioning scheme, then the drive is also the storage volume
	if(lParentDevice.is<Solid::StorageVolume>()) {
		lDeviceList.append(lParentDevice);
	}

	// filter out some volumes that should not be visible
	QList<Solid::Device> lVolumeDeviceList;
	foreach(Solid::Device lVolumeDevice, lDeviceList) {
		Solid::StorageVolume *lVolume = lVolumeDevice.as<Solid::StorageVolume>();
		if (lVolume && lVolume->usage() == Solid::StorageVolume::FileSystem && !lVolume->isIgnored()) {
			lVolumeDeviceList.append(lVolumeDevice);
		}
	}

	// simplest attempt at getting the same partition numbering every time a device is plugged in
	qSort(lVolumeDeviceList.begin(), lVolumeDeviceList.end(), deviceLessThan);

	int lPartitionNumber = 1;
	foreach(Solid::Device lVolumeDevice, lVolumeDeviceList) {
		Solid::StorageVolume *lVolume = lVolumeDevice.as<Solid::StorageVolume>();
		QStandardItem *lItem = new QStandardItem();
		lItem->setEditable(false);
		QString lUUID = lVolume->uuid();
		if(lUUID.isEmpty()) { //seems to happen for vfat partitions
			lUUID = lParentDevice.description() + "|" + lVolume->label();
		}
		lItem->setData(lUUID, DriveSelection::UUID);
		lItem->setData(lParentDevice.description(), DriveSelection::DeviceDescription);
		lItem->setData(lVolume->label(), DriveSelection::Label);
		lItem->setData(lVolumeDevice.udi(), DriveSelection::UDI);
		lItem->setData(lPartitionNumber, DriveSelection::PartitionNumber);
		lItem->setData(lVolumeDeviceList.count(), DriveSelection::PartitionsOnDrive);
		lItem->setData(0, DriveSelection::TotalSpace);
		lItem->setData(0, DriveSelection::UsedSpace);

		Solid::StorageAccess *lAccess = lVolumeDevice.as<Solid::StorageAccess>();
		if(!lAccess->isAccessible()) {
			connect(lAccess, SIGNAL(accessibilityChanged(bool,QString)), SLOT(accessabilityChanged(bool,QString)));
			lAccess->setup();
		} else {
			KDiskFreeSpaceInfo lInfo = KDiskFreeSpaceInfo::freeSpaceInfo(lAccess->filePath());
			if(lInfo.isValid()) {
				lItem->setData(lInfo.size(), DriveSelection::TotalSpace);
				lItem->setData(lInfo.used(), DriveSelection::UsedSpace);
			}
		}
		mDrivesModel->appendRow(lItem);

		if(mSelectedUuid == lUUID) {
			QModelIndex lIndex = mDrivesModel->indexFromItem(lItem);
			setCurrentIndex(lIndex);
			removeDisconnectedItem();
			mSelectedUdi = lVolumeDevice.udi();
		}
		if(mDrivesModel->rowCount() == 1) {
			selectionModel()->select(mDrivesModel->index(0, 0), QItemSelectionModel::ClearAndSelect);
		}

		lPartitionNumber++;
	}
}

void DriveSelection::deviceRemoved(const QString &pUdi) {
	for(int lRow = 0; lRow < mDrivesModel->rowCount(); ++lRow) {
		QStandardItem *lItem = mDrivesModel->item(lRow);
		if(lItem->data(DriveSelection::UDI).toString() == pUdi) {
			if(pUdi == mSelectedUdi) {
				addDisconnectedItem();
			}
			mDrivesModel->removeRow(lRow);
			break;
		}
	}
}

void DriveSelection::accessabilityChanged(bool pAccessible, const QString &pUdi) {
	if(!pAccessible) {
		return;
	}
	Solid::Device lDevice(pUdi);
	Solid::StorageAccess *lAccess = lDevice.as<Solid::StorageAccess>();
	if(!lAccess) {
		return;
	}
	KDiskFreeSpaceInfo lInfo = KDiskFreeSpaceInfo::freeSpaceInfo(lAccess->filePath());
	if(!lInfo.isValid()) {
		return;
	}
	for(int lRow = 0; lRow < mDrivesModel->rowCount(); ++lRow) {
		QStandardItem *lItem = mDrivesModel->item(lRow);
		if(lItem->data(DriveSelection::UDI).toString() == pUdi) {
			lItem->setData(lInfo.size(), DriveSelection::TotalSpace);
			lItem->setData(lInfo.used(), DriveSelection::UsedSpace);
			break;
		}
	}
}

void DriveSelection::updateSelection(const QItemSelection &pSelected, const QItemSelection &pDeselected) {
	Q_UNUSED(pDeselected)
	if(!pSelected.indexes().isEmpty()) {
		mSelectedUuid = pSelected.indexes().first().data(DriveSelection::UUID).toString();
		emit driveSelectionChanged();
	}
}

void DriveSelection::setSelectedDrive(const QString &pUuid) {
	if(pUuid == mSelectedUuid) {
		return;
	}
	mSelectedUuid = pUuid;

	for(int lRow = 0; lRow < mDrivesModel->rowCount(); ++lRow) {
		QStandardItem *lItem = mDrivesModel->item(lRow);
		if(lItem->data(DriveSelection::UUID).toString() == mSelectedUuid) {
			QModelIndex lIndex = mDrivesModel->indexFromItem(lItem);
			setCurrentIndex(lIndex);
			mSelectedUdi = lIndex.data(DriveSelection::UDI).toString();
			return;
		}
	}
	addDisconnectedItem();
}

void DriveSelection::saveExtraData() {
	for(int lRow = 0; lRow < mDrivesModel->rowCount(); ++lRow) {
		QStandardItem *lItem = mDrivesModel->item(lRow);
		if(lItem->data(DriveSelection::UUID).toString() == mSelectedUuid) {
			lItem->data(DriveSelection::TotalSpace);
			mBackupPlan->mExternalDeviceDescription = lItem->data(DriveSelection::DeviceDescription).toString();
			mBackupPlan->mExternalPartitionNumber = lItem->data(DriveSelection::PartitionNumber).toInt();
			mBackupPlan->mExternalPartitionsOnDrive = lItem->data(DriveSelection::PartitionsOnDrive).toInt();
			mBackupPlan->mExternalVolumeCapacity = lItem->data(DriveSelection::TotalSpace).toULongLong();
			mBackupPlan->mExternalVolumeLabel = lItem->data(DriveSelection::Label).toString();
			mSelectedUdi = lItem->data(DriveSelection::UDI).toString();
			return;
		}
	}
}

void DriveSelection::addDisconnectedItem() {
	if(mSelectedUuid.isEmpty()) {
		return;
	}
	QStandardItem *lItem = new QStandardItem();
	lItem->setData(QLatin1String("DISCONNECTED_BACKUP_DEVICE"), DriveSelection::UDI);
	lItem->setData(mSelectedUuid, DriveSelection::UUID);
	lItem->setData(0, DriveSelection::UsedSpace);
	lItem->setData(mBackupPlan->mExternalPartitionNumber, DriveSelection::PartitionNumber);
	lItem->setData(mBackupPlan->mExternalPartitionsOnDrive, DriveSelection::PartitionsOnDrive);
	lItem->setData(mBackupPlan->mExternalDeviceDescription, DriveSelection::DeviceDescription);
	lItem->setData(mBackupPlan->mExternalVolumeCapacity, DriveSelection::TotalSpace);
	lItem->setData(mBackupPlan->mExternalVolumeLabel, DriveSelection::Label);
	lItem->setEditable(false);
	mDrivesModel->appendRow(lItem);

	QModelIndex lIndex = mDrivesModel->indexFromItem(lItem);
	setCurrentIndex(lIndex);

	mSelectedUdi.clear();
}

void DriveSelection::removeDisconnectedItem() {
	for(int lRow = 0; lRow < mDrivesModel->rowCount(); ++lRow) {
		QStandardItem *lItem = mDrivesModel->item(lRow);
		if(lItem->data(DriveSelection::UDI).toString() == QLatin1String("DISCONNECTED_BACKUP_DEVICE")) {
			mDrivesModel->removeRow(lRow);
			break;
		}
	}
}

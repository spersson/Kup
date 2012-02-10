#include "driveselection.h"
#include "driveselectiondelegate.h"

#include <QStringList>
#include <QList>
#include <Solid/Device>
#include <Solid/DeviceNotifier>
#include <Solid/StorageDrive>
#include <Solid/StorageVolume>
#include <Solid/StorageAccess>
#include <QItemSelectionModel>
#include <QStandardItemModel>

#include <KConfigDialogManager>
#include <KDiskFreeSpaceInfo>
#include <KLocale>

DriveSelection::DriveSelection(QWidget *parent)
   : QListView(parent)
{
	KConfigDialogManager::changedMap()->insert("DriveSelection", SIGNAL(driveSelectionChanged()));

	m_drivesModel = new QStandardItemModel(this);
	setModel(m_drivesModel);
	m_drivesDelegate = new DriveSelectionDelegate;
	setItemDelegate(m_drivesDelegate);
	setSelectionMode(QAbstractItemView::SingleSelection);

	reloadDrives();
	connect(selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
	        this, SLOT(updateSelectedDrive(QItemSelection,QItemSelection)));
	connect(Solid::DeviceNotifier::instance(), SIGNAL(deviceAdded(QString)), SLOT(reloadDrives()));
	connect(Solid::DeviceNotifier::instance(), SIGNAL(deviceRemoved(QString)), SLOT(reloadDrives()));
}

void DriveSelection::reloadDrives()
{
	m_drivesModel->clear();
	QStandardItem *selected = 0;

	QList<Solid::Device> drives = Solid::Device::listFromType(Solid::DeviceInterface::StorageDrive, QString());
	for (int vNum=0;vNum < drives.size();vNum++) {
		Solid::StorageDrive *drive = drives[vNum].as<Solid::StorageDrive>();
		if (!drive || !drive->isHotpluggable()) {
			continue;
		}
		QList<Solid::Device> storageVolumes = Solid::Device::listFromType(Solid::DeviceInterface::StorageVolume, drives[vNum].udi());
		for (int svNum=0;svNum < storageVolumes.size();svNum++) {
			Solid::StorageVolume *storageVolume = storageVolumes[svNum].as<Solid::StorageVolume>();
			if (!storageVolume || storageVolume->usage() != Solid::StorageVolume::FileSystem) {
				continue;
			}
			QStandardItem *item = new QStandardItem();
			item->setText(storageVolume->label());
			if (item->text().isEmpty())
				item->setText(drives[vNum].description());

			Solid::StorageAccess *access = storageVolumes[svNum].as<Solid::StorageAccess>();
			if(!access->isAccessible()) {
				access->setup();
				item->setData(0, Qt::UserRole + 3);
				item->setData(0, Qt::UserRole + 4);
			}
			else {
				KDiskFreeSpaceInfo info = KDiskFreeSpaceInfo::freeSpaceInfo(access->filePath());
				if (info.isValid()) {
					item->setData(info.size(), Qt::UserRole + 3);
					item->setData(info.used(), Qt::UserRole + 4);
				} else {
					item->setData(0, Qt::UserRole + 3);
					item->setData(0, Qt::UserRole + 4);
				}
			}
			item->setData(drives.at(vNum).udi(), Qt::UserRole + 1);
			item->setData(storageVolume->uuid(), Qt::UserRole + 2);
			item->setEditable(false);
			m_drivesModel->appendRow(item);
			if (storageVolume->uuid() == mSelectedUuid) {
				selected = item;
			}
			// notifier->addDevice(storageVolume); //TODO
		}
	}
	if (selected) {
		QModelIndex index = m_drivesModel->indexFromItem(selected);
		setCurrentIndex(index);
	} else if (!mSelectedUuid.isEmpty()) {
		QStandardItem *item = new QStandardItem();
//		QString name = daemon->getDeviceName();
//		if (name.isEmpty())
		item->setText(i18n("Selected Backup Device (disconnected)"));
//		else
//			item->setText(i18n("%1 (disconnected)").arg(name));

		item->setData("DISCONNECTED_BACKUP_DEVICE", Qt::UserRole + 1);
		item->setData(mSelectedUuid, Qt::UserRole + 2);
		item->setData(0, Qt::UserRole + 3);
		item->setData(0, Qt::UserRole + 4);
		item->setEditable(false);
		m_drivesModel->appendRow(item);

		QModelIndex index = m_drivesModel->indexFromItem(item);
		setCurrentIndex(index);
	}
}

void DriveSelection::updateSelectedDrive(const QItemSelection &pSelected, const QItemSelection &pDeselected) {
	Q_UNUSED(pDeselected)
	mSelectedUuid = pSelected.indexes().first().data(Qt::UserRole+2).toString();
	emit driveSelectionChanged();
}

void DriveSelection::setSelectedDrive(const QString &pUuid) {
	mSelectedUuid = pUuid;
	reloadDrives();
}


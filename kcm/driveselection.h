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

#ifndef DRIVESELECTION_H
#define DRIVESELECTION_H

#include <QListView>
#include <QStringList>

class QStandardItem;
class QStandardItemModel;

class BackupPlan;

class DriveSelection : public QListView
{
	Q_OBJECT
	Q_PROPERTY(QString selectedDrive READ selectedDrive WRITE setSelectedDrive NOTIFY selectedDriveChanged USER true)
	Q_PROPERTY(bool driveIsSelected READ driveIsSelected NOTIFY driveIsSelectedChanged)
	Q_PROPERTY(bool selectedDriveIsAccessible READ selectedDriveIsAccessible NOTIFY selectedDriveIsAccessibleChanged)
public:
	enum DataType {
		UUID = Qt::UserRole + 1,
		UDI,
		TotalSpace,
		UsedSpace,
		Label,
		DeviceDescription,
		PartitionNumber,
		PartitionsOnDrive,
		FileSystem,
		PermissionLossWarning,
		SymlinkLossWarning
	};

public:
	DriveSelection(BackupPlan *pBackupPlan, QWidget *parent=0);
	QString selectedDrive() const {return mSelectedUuid;}
	bool driveIsSelected() const {return !mSelectedUuid.isEmpty();}
	bool selectedDriveIsAccessible() const {return mSelectedAndAccessible;}
	QString mountPathOfSelectedDrive() const;

public slots:
	void setSelectedDrive(const QString &pUuid);
	void saveExtraData();
	void updateSyncWarning(bool pSyncBackupSelected);

signals:
	void selectedDriveChanged(const QString &pSelectedDrive);
	void driveIsSelectedChanged(bool pDriveIsSelected);
	void selectedDriveIsAccessibleChanged(bool pDriveIsSelectedAndAccessible);

protected slots:
	void deviceAdded(const QString &pUdi);
	void delayedDeviceAdded();
	void deviceRemoved(const QString &pUdi);
	void accessabilityChanged(bool pAccessible, const QString &pUdi);
	void updateSelection(const QItemSelection &pSelected, const QItemSelection &pDeselected);

protected:
	virtual void paintEvent(QPaintEvent *pPaintEvent);
	int findItem(const DataType pField, const QString &pSearchString, QStandardItem **pReturnedItem = NULL) const;

	QStandardItemModel *mDrivesModel;
	QString mSelectedUuid;
	BackupPlan *mBackupPlan;
	QStringList mDrivesToAdd;
	bool mSelectedAndAccessible;
	bool mSyncedBackupType;
};

#endif

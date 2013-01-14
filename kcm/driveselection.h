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

class QStandardItemModel;

class BackupPlan;

class DriveSelection : public QListView
{
	Q_OBJECT
	Q_PROPERTY(QString selectedDrive READ selectedDrive WRITE setSelectedDrive NOTIFY driveSelectionChanged USER true)
public:
	enum DataType {
		UUID = Qt::UserRole + 1,
		UDI,
		TotalSpace,
		UsedSpace,
		Label,
		DeviceDescription,
		PartitionNumber,
		PartitionsOnDrive
	};

public:
	DriveSelection(BackupPlan *pBackupPlan, QWidget *parent=0);
	QString selectedDrive() {return mSelectedUuid;}
	void setSelectedDrive(const QString &pUuid);
	void saveExtraData();

signals:
	void driveSelectionChanged();

protected slots:
	void deviceAdded(const QString &pUdi);
	void delayedDeviceAdded();
	void deviceRemoved(const QString &pUdi);
	void accessabilityChanged(bool pAccessible, const QString &pUdi);
	void updateSelection(const QItemSelection &pSelected, const QItemSelection &pDeselected);

private:
	void addDisconnectedItem();
	void removeDisconnectedItem();

	QStandardItemModel *mDrivesModel;
	QString mSelectedUuid;
	QString mSelectedUdi; //remembered for noticing when a selected drive is disconnected.
	BackupPlan *mBackupPlan;
	QStringList mDrivesToAdd;
};

#endif

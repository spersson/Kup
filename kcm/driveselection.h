#ifndef DRIVESELECTION_H
#define DRIVESELECTION_H

#include <QListView>
#include <QString>

class FolderSelectionModel;
class DriveSelectionDelegate;

class QStandardItemModel;

class DriveSelection : public QListView
{
	Q_OBJECT
	Q_PROPERTY(QString selectedDrive READ selectedDrive WRITE setSelectedDrive USER true)

public:
	DriveSelection(QWidget *parent=0);
	QString selectedDrive() {return mSelectedUuid;}
	void setSelectedDrive(const QString &pUuid);

signals:
	void driveSelectionChanged();

protected slots:
	void reloadDrives();
	void updateSelectedDrive(const QItemSelection &pSelected, const QItemSelection &pDeselected);

private:
	QStandardItemModel *m_drivesModel;
	DriveSelectionDelegate *m_drivesDelegate;
	QString mSelectedUuid;
};

#endif

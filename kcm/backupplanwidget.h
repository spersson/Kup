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

#ifndef BACKUPPLANWIDGET_H
#define BACKUPPLANWIDGET_H

#include <QDialog>
#include <QTreeView>
#include <QWidget>

class BackupPlan;
class DriveSelection;
class FolderSelectionModel;

class KDirModel;
class KLineEdit;
class KPageWidget;
class KPageWidgetItem;
class QPushButton;

class QRadioButton;

class FolderSelectionWidget : public QTreeView {
	Q_OBJECT
public:
	FolderSelectionWidget(FolderSelectionModel *pModel, QWidget *pParent = 0);

public slots:
	void setHiddenFoldersVisible(bool pVisible);
	void expandToShowSelections();

protected:
	FolderSelectionModel *mModel;
};

class ConfigIncludeDummy : public QWidget {
	Q_OBJECT
signals:
	void includeListChanged();
public:
	Q_PROPERTY(QStringList includeList READ includeList WRITE setIncludeList USER true)
	ConfigIncludeDummy(FolderSelectionModel *pModel, FolderSelectionWidget *pParent);
	QStringList includeList();
	void setIncludeList(QStringList pIncludeList);
	FolderSelectionModel *mModel;
	FolderSelectionWidget *mTreeView;
};

class ConfigExcludeDummy : public QWidget {
	Q_OBJECT
signals:
	void excludeListChanged();
public:
	Q_PROPERTY(QStringList excludeList READ excludeList WRITE setExcludeList USER true)
	ConfigExcludeDummy(FolderSelectionModel *pModel, FolderSelectionWidget *pParent);
	QStringList excludeList();
	void setExcludeList(QStringList pExcludeList);
	FolderSelectionModel *mModel;
	FolderSelectionWidget *mTreeView;
};

class DirDialog: public QDialog
{
	Q_OBJECT
public:
	explicit DirDialog(const QUrl &pRootDir, const QString &pStartSubDir, QWidget *pParent = NULL);
	QUrl url() const;

public slots:
	void createNewFolder();
	void selectEntry(QModelIndex pIndex);

protected:
	QTreeView *mTreeView;
	KDirModel *mDirModel;
};

class BackupPlanWidget : public QWidget
{
	Q_OBJECT
public:
	BackupPlanWidget(BackupPlan *pBackupPlan, const QString &pBupVersion,
	                 const QString &pRsyncVersion, bool pPar2Available);

	void saveExtraData();

	KPageWidgetItem *createTypePage(const QString &pBupVersion, const QString &pRsyncVersion);
	KPageWidgetItem *createSourcePage();
	KPageWidgetItem *createDestinationPage();
	KPageWidgetItem *createSchedulePage();
	KPageWidgetItem *createAdvancedPage(bool pPar2Available);

	KLineEdit *mDescriptionEdit;
	QPushButton *mConfigureButton;
	KPageWidget *mConfigPages;
	BackupPlan *mBackupPlan;
	DriveSelection *mDriveSelection;
	KLineEdit *mDriveDestEdit;
	QRadioButton *mVersionedRadio;
	QRadioButton *mSyncedRadio;
	FolderSelectionWidget *mSourceSelectionWidget;

protected slots:
	void openDriveDestDialog();

signals:
	void requestOverviewReturn();
};

#endif // BACKUPPLANWIDGET_H

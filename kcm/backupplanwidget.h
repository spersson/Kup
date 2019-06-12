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
#include <QSet>
#include <QWidget>

class BackupPlan;
class DirSelector;
class DriveSelection;
class FolderSelectionModel;

class KLineEdit;
class KMessageWidget;
class KPageWidget;
class KPageWidgetItem;
class QAction;
class QFileInfo;
class QPushButton;
class QRadioButton;
class QThread;
class QTimer;
class QTreeView;

class FileScanner : public QObject {
	Q_OBJECT
public:
	FileScanner();
	bool event(QEvent *pEvent) override;

public slots:
	void includePath(QString pPath);
	void excludePath(QString pPath);

signals:
	void unreadablesChanged(QPair<QSet<QString>, QSet<QString>>);
	void symlinkProblemsChanged(QHash<QString,QString>);

protected slots:
	void sendPendingUnreadables();
	void sendPendingSymlinks();

protected:
	bool isPathIncluded(const QString &pPath);
	void checkPathForProblems(const QFileInfo &pFileInfo);
	bool isSymlinkProblematic(const QString &pTarget);
	void scanFolder(const QString &pPath);

	QSet<QString> mIncludedFolders;
	QSet<QString> mExcludedFolders;

	QSet<QString> mUnreadableFolders;
	QSet<QString> mUnreadableFiles;
	QTimer *mUnreadablesTimer;

	QHash<QString,QString> mSymlinksNotOk;
	QHash<QString,QString> mSymlinksOk;
	QTimer *mSymlinkTimer;
};

class FolderSelectionWidget : public QWidget {
	Q_OBJECT
public:
	FolderSelectionWidget(FolderSelectionModel *pModel, QWidget *pParent = 0);
	virtual ~FolderSelectionWidget();

public slots:
	void setHiddenFoldersVisible(bool pVisible);
	void expandToShowSelections();
	void setUnreadables(QPair<QSet<QString>, QSet<QString>> pUnreadables);
	void setSymlinks(QHash<QString,QString> pSymlinks);
	void updateMessage();
	void executeExcludeAction();
	void executeIncludeAction();

protected:
	QTreeView *mTreeView;
	FolderSelectionModel *mModel;
	KMessageWidget *mMessageWidget;
	QThread *mWorkerThread;
	QStringList mUnreadableFolders;
	QStringList mUnreadableFiles;
	QString mExcludeActionPath;
	QAction *mExcludeAction;
	QHash<QString,QString> mSymlinkProblems;
	QString mIncludeActionPath;
	QAction *mIncludeAction;
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
	explicit DirDialog(const QUrl &pRootDir, const QString &pStartSubDir, QWidget *pParent = nullptr);
	QUrl url() const;
private:
	DirSelector *mDirSelector;
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

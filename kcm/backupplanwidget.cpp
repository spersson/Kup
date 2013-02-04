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

#include "backupplanwidget.h"
#include "backupplan.h"
#include "folderselectionmodel.h"
#include "driveselection.h"

#include <KButtonGroup>
#include <KComboBox>
#include <KConfigDialogManager>
#include <KIcon>
#include <KLineEdit>
#include <KLocale>
#include <KNumInput>
#include <KPushButton>
#include <KPageWidget>
#include <KUrlRequester>

#include <QBoxLayout>
#include <QCheckBox>
#include <QFile>
#include <QFormLayout>
#include <QLabel>
#include <QRadioButton>
#include <QTreeView>

static void expandRecursively(const QModelIndex& pIndex, QTreeView* pTreeView) {
	if(pIndex.isValid()) {
		pTreeView->expand(pIndex);
		expandRecursively(pIndex.parent(), pTreeView);
	}
}

ConfigIncludeDummy::ConfigIncludeDummy(FolderSelectionModel *pModel, QTreeView *pParent)
   : QWidget(pParent), mModel(pModel), mTreeView(pParent)
{
	connect(mModel, SIGNAL(includedPathsChanged()), this, SIGNAL(includeListChanged()));
	KConfigDialogManager::changedMap()->insert("ConfigIncludeDummy", SIGNAL(includeListChanged()));
}

QStringList ConfigIncludeDummy::includeList() {
	return mModel->includedFolders();
}

void ConfigIncludeDummy::setIncludeList(QStringList pIncludeList) {
	for(int i = 0; i < pIncludeList.count(); ++i) {
		if(!QFile::exists(pIncludeList.at(i))) {
			pIncludeList.removeAt(i--);
		}
	}

	mModel->setFolders(pIncludeList, mModel->excludedFolders());
	foreach(const QString& lFolder, mModel->includedFolders() + mModel->excludedFolders()) {
		expandRecursively(mModel->index(lFolder).parent(), mTreeView);
	}
}

ConfigExcludeDummy::ConfigExcludeDummy(FolderSelectionModel *pModel, QTreeView *pParent)
   : QWidget(pParent), mModel(pModel), mTreeView(pParent)
{
	connect(mModel, SIGNAL(excludedPathsChanged()), this, SIGNAL(excludeListChanged()));
	KConfigDialogManager::changedMap()->insert("ConfigExcludeDummy", SIGNAL(excludeListChanged()));
}

QStringList ConfigExcludeDummy::excludeList() {
	return mModel->excludedFolders();
}

void ConfigExcludeDummy::setExcludeList(QStringList pExcludeList) {
	for(int i = 0; i < pExcludeList.count(); ++i) {
		if(!QFile::exists(pExcludeList.at(i))) {
			pExcludeList.removeAt(i--);
		}
	}
	mModel->setFolders(mModel->includedFolders(), pExcludeList);
	foreach(const QString& lFolder, mModel->includedFolders() + mModel->excludedFolders()) {
		expandRecursively(mModel->index(lFolder).parent(), mTreeView);
	}
}


class FolderSelectionWidget : public QTreeView
{
public:
	FolderSelectionWidget(FolderSelectionModel *pModel, QWidget *pParent = 0)
	   : QTreeView(pParent), mModel(pModel)
	{
		mModel->setRootPath("/");
		setAnimated(true);
		setModel(mModel);
		ConfigIncludeDummy *lIncludeDummy = new ConfigIncludeDummy(mModel, this);
		lIncludeDummy->setObjectName("kcfg_Paths included");
		ConfigExcludeDummy *lExcludeDummy = new ConfigExcludeDummy(mModel, this);
		lExcludeDummy->setObjectName("kcfg_Paths excluded");
	}
	FolderSelectionModel *mModel;
};

KPageWidgetItem *BackupPlanWidget::createSourcePage() {
	mSourceSelectionModel = new FolderSelectionModel(mBackupPlan->mShowHiddenFolders, this);
	FolderSelectionWidget *lSelectionWidget = new FolderSelectionWidget(mSourceSelectionModel, this);
	KPageWidgetItem *lPage = new KPageWidgetItem(lSelectionWidget);
	lPage->setName(i18nc("@title", "Sources"));
	lPage->setHeader(i18nc("@label", "Select which folders to include in backup"));
	lPage->setIcon(KIcon("folder-important"));
	return lPage;
}

KPageWidgetItem *BackupPlanWidget::createDestinationPage() {
	KButtonGroup *lButtonGroup = new KButtonGroup(this);
	lButtonGroup->setObjectName("kcfg_Destination type");
	lButtonGroup->setFlat(true);
	QVBoxLayout *lVLayout = new QVBoxLayout;
	QRadioButton *lFileSystemRadio = new QRadioButton(i18nc("@option:radio", "Local Filesystem"));
	QRadioButton *lDriveRadio = new QRadioButton(i18nc("@option:radio", "External Storage"));
//	QRadioButton *lSshRadio = new QRadioButton(i18nc("@option:radio", "SSH Server"));

	KUrlRequester *lUrlEdit = new KUrlRequester;
	lUrlEdit->setVisible(false);
	lUrlEdit->setMode(KFile::Directory | KFile::LocalOnly);
	lUrlEdit->setObjectName("kcfg_Filesystem destination path");
	QObject::connect(lFileSystemRadio, SIGNAL(toggled(bool)), lUrlEdit, SLOT(setVisible(bool)));

	QWidget *lDriveWidget = new QWidget;
	lDriveWidget->setVisible(false);
	QObject::connect(lDriveRadio, SIGNAL(toggled(bool)), lDriveWidget, SLOT(setVisible(bool)));
	QVBoxLayout *lDriveLayout = new QVBoxLayout;
	mDriveSelection = new DriveSelection(mBackupPlan);
	mDriveSelection->setObjectName("kcfg_External drive UUID");
	lDriveLayout->addWidget(mDriveSelection);
	QHBoxLayout *lDriveHoriLayout = new QHBoxLayout;
	QLabel *lDriveDestLabel = new QLabel(i18nc("@label:textbox", "Folder on Destination Drive:"));
	KLineEdit *lDriveDestination = new KLineEdit;
	lDriveDestination->setObjectName("kcfg_External drive destination path");
	lDriveDestination->setToolTip(i18nc("@info:tooltip", "The specified folder will be created if it does not exist."));
	lDriveDestLabel->setToolTip(i18nc("@info:tooltip", "The specified folder will be created if it does not exist."));
	lDriveDestLabel->setBuddy(lDriveDestination);
	lDriveHoriLayout->addWidget(lDriveDestLabel);
	lDriveHoriLayout->addWidget(lDriveDestination);
	lDriveLayout->addLayout(lDriveHoriLayout);
	lDriveWidget->setLayout(lDriveLayout);

	//	QWidget *lSshWidget = new QWidget;
	//	lSshWidget->setVisible(false);
	//	QObject::connect(lSshRadio, SIGNAL(toggled(bool)), lSshWidget, SLOT(setVisible(bool)));
	//	QFormLayout *lFLayout = new QFormLayout;
	//	KLineEdit *lServerNameEdit = new KLineEdit;
	//	lServerNameEdit->setObjectName("kcfg_SSH server name");
	//	lFLayout->addRow(i18n("Server Name:"), lServerNameEdit);
	//	KLineEdit *lLoginNameEdit= new KLineEdit;
	//	lLoginNameEdit->setObjectName("kcfg_SSH login name");
	//	lFLayout->addRow(i18n("Login Name:"), lLoginNameEdit);
	//	KLineEdit *lPasswordEdit = new KLineEdit;
	//	lPasswordEdit->setObjectName("kcfg_SSH login password");
	//	lFLayout->addRow(i18n("Password:"), lPasswordEdit);
	//	KLineEdit *lSshDestinationEdit = new KLineEdit;
	//	lSshDestinationEdit->setObjectName("kcfg_SSH destination path");
	//	lFLayout->addRow(i18n("Destination Path on Server:"), lSshDestinationEdit);
	//	lSshWidget->setLayout(lFLayout);

	lVLayout->addWidget(lFileSystemRadio);
	lVLayout->addWidget(lUrlEdit);
	lVLayout->addWidget(lDriveRadio);
	lVLayout->addWidget(lDriveWidget);
	//	lVLayout->addWidget(lSshRadio);
	//	lVLayout->addWidget(lSshWidget);
	lVLayout->addStretch();
	lButtonGroup->setLayout(lVLayout);

	KPageWidgetItem *lPage = new KPageWidgetItem(lButtonGroup);
	lPage->setName(i18nc("@title", "Destination"));
	lPage->setHeader(i18nc("@label", "Select the backup destination"));
	lPage->setIcon(KIcon("folder-downloads"));
	return lPage;
}

KPageWidgetItem *BackupPlanWidget::createSchedulePage() {
	QWidget *lTopWidget = new QWidget(this);
	QVBoxLayout *lTopLayout = new QVBoxLayout;
	KButtonGroup *lButtonGroup = new KButtonGroup;
	lButtonGroup->setObjectName("kcfg_Schedule type");
	lButtonGroup->setFlat(true);

	int lIndentation = lButtonGroup->style()->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth) +
	      lButtonGroup->style()->pixelMetric(QStyle::PM_RadioButtonLabelSpacing);

	QVBoxLayout *lVLayout = new QVBoxLayout;
	QRadioButton *lManualRadio = new QRadioButton(i18nc("@option:radio", "Manual Only"));
	QRadioButton *lIntervalRadio = new QRadioButton(i18nc("@option:radio", "Interval"));
	QRadioButton *lUsageRadio = new QRadioButton(i18nc("@option:radio", "Active Usage Time"));

	QLabel *lManualLabel = new QLabel(i18nc("@info", "Backups are only taken when manually requested. "
	                                       "This can be done by using the popup menu from "
	                                       "the backup system tray icon."));
	lManualLabel->setVisible(false);
	lManualLabel->setWordWrap(true);
	QObject::connect(lManualRadio, SIGNAL(toggled(bool)), lManualLabel, SLOT(setVisible(bool)));
	QGridLayout *lManualLayout = new QGridLayout;
	lManualLayout->setColumnMinimumWidth(0, lIndentation);
	lManualLayout->addWidget(lManualLabel, 0, 1);

	QWidget *lIntervalWidget = new QWidget;
	lIntervalWidget->setVisible(false);
	QObject::connect(lIntervalRadio, SIGNAL(toggled(bool)), lIntervalWidget, SLOT(setVisible(bool)));
	QLabel *lIntervalLabel = new QLabel(i18nc("@info", "New backup will be triggered when backup "
	                                         "destination becomes available and more than "
	                                         "the configured interval has passed since the "
	                                         "last backup was taken."));
	lIntervalLabel->setWordWrap(true);
	QGridLayout *lIntervalVertLayout = new QGridLayout;
	lIntervalVertLayout->setColumnMinimumWidth(0, lIndentation);
	lIntervalVertLayout->addWidget(lIntervalLabel, 0, 1);
	QHBoxLayout *lIntervalLayout = new QHBoxLayout;
	KIntSpinBox *lIntervalSpinBox = new KIntSpinBox;
	lIntervalSpinBox->setObjectName("kcfg_Schedule interval");
	lIntervalSpinBox->setMinimum(1);
	lIntervalLayout->addWidget(lIntervalSpinBox);
	KComboBox *lIntervalUnit = new KComboBox;
	lIntervalUnit->setObjectName("kcfg_Schedule interval unit");
	lIntervalUnit->addItem(i18nc("@item:inlistbox", "Minutes"));
	lIntervalUnit->addItem(i18nc("@item:inlistbox", "Hours"));
	lIntervalUnit->addItem(i18nc("@item:inlistbox", "Days"));
	lIntervalUnit->addItem(i18nc("@item:inlistbox", "Weeks"));
	lIntervalLayout->addWidget(lIntervalUnit);
	lIntervalLayout->addStretch();
	lIntervalVertLayout->addLayout(lIntervalLayout, 1, 1);
	lIntervalWidget->setLayout(lIntervalVertLayout);

	QWidget *lUsageWidget = new QWidget;
	lUsageWidget->setVisible(false);
	QObject::connect(lUsageRadio, SIGNAL(toggled(bool)), lUsageWidget, SLOT(setVisible(bool)));
	QLabel *lUsageLabel = new QLabel(i18nc("@info", "New backup will be triggered when backup destination "
	                                      "becomes available and you have been using your "
	                                      "computer actively for more than the configured "
	                                      "time limit since the last backup was taken."));
	lUsageLabel->setWordWrap(true);
	QGridLayout *lUsageVertLayout = new QGridLayout;
	lUsageVertLayout->setColumnMinimumWidth(0, lIndentation);
	lUsageVertLayout->addWidget(lUsageLabel, 0, 1);
	QHBoxLayout *lUsageLayout = new QHBoxLayout;
	KIntSpinBox *lUsageSpinBox = new KIntSpinBox;
	lUsageSpinBox->setObjectName("kcfg_Usage limit");
	lUsageSpinBox->setMinimum(1);
	lUsageLayout->addWidget(lUsageSpinBox);
	lUsageLayout->addWidget(new QLabel(i18nc("@item:inlistbox", "Hours")));
	lUsageLayout->addStretch();
	lUsageVertLayout->addLayout(lUsageLayout, 1, 1);
	lUsageWidget->setLayout(lUsageVertLayout);

	QCheckBox *lAskFirstCheckBox = new QCheckBox(i18nc("@option:check", "Ask for confirmation before taking backup"));
	lAskFirstCheckBox->setObjectName("kcfg_Ask first");

	lVLayout->addWidget(lManualRadio);
	lVLayout->addLayout(lManualLayout);
	lVLayout->addWidget(lIntervalRadio);
	lVLayout->addWidget(lIntervalWidget);
	lVLayout->addWidget(lUsageRadio);
	lVLayout->addWidget(lUsageWidget);
	lButtonGroup->setLayout(lVLayout);

	lTopLayout->addWidget(lButtonGroup);
	lTopLayout->addSpacing(lAskFirstCheckBox->fontMetrics().height());
	lTopLayout->addWidget(lAskFirstCheckBox);
	lTopLayout->addStretch();
	lTopWidget->setLayout(lTopLayout);

	KPageWidgetItem *lPage = new KPageWidgetItem(lTopWidget);
	lPage->setName(i18nc("@title", "Schedule"));
	lPage->setHeader(i18nc("@label", "Specify the backup schedule"));
	lPage->setIcon(KIcon("view-time-schedule"));
	return lPage;
}

KPageWidgetItem *BackupPlanWidget::createAdvancedPage() {
	QWidget *lAdvancedWidget = new QWidget(this);
	QVBoxLayout *lAdvancedLayout = new QVBoxLayout;
	QCheckBox *lShowHiddenCheckBox = new QCheckBox(i18n("Show hidden folders in source selection"));
	lShowHiddenCheckBox->setObjectName(QLatin1String("kcfg_Show hidden folders"));
	lAdvancedLayout->addWidget(lShowHiddenCheckBox);
	lAdvancedWidget->setLayout(lAdvancedLayout);
	connect(lShowHiddenCheckBox, SIGNAL(toggled(bool)), mSourceSelectionModel, SLOT(setHiddenFoldersShown(bool)));
	KPageWidgetItem *lPage = new KPageWidgetItem(lAdvancedWidget);
	lPage->setName(i18nc("@title", "Advanced"));
	lPage->setHeader(i18nc("@label", "Extra options for advanced users"));
	lPage->setIcon(KIcon("preferences-other"));
	return lPage;
}

BackupPlanWidget::BackupPlanWidget(BackupPlan *pBackupPlan, QWidget *pParent)
   : QWidget(pParent), mBackupPlan(pBackupPlan)
{
	QVBoxLayout *lVLayout1 = new QVBoxLayout;
	QHBoxLayout *lHLayout1 = new QHBoxLayout;

	mDescriptionEdit = new KLineEdit;
	mDescriptionEdit->setObjectName("kcfg_Description");
	mDescriptionEdit->setClearButtonShown(true);
	QLabel *lDescriptionLabel = new QLabel(i18nc("@label", "Description:"));
	lDescriptionLabel->setBuddy(mDescriptionEdit);
	mConfigureButton = new KPushButton(KIcon("go-previous-view"), i18nc("@action:button", "Back to overview"));
	connect(mConfigureButton, SIGNAL(clicked()), this, SIGNAL(requestOverviewReturn()));

	mConfigPages = new KPageWidget;
	mConfigPages->addPage(createSourcePage());
	mConfigPages->addPage(createDestinationPage());
	mConfigPages->addPage(createSchedulePage());
	mConfigPages->addPage(createAdvancedPage());

	lHLayout1->addWidget(mConfigureButton);
	lHLayout1->addStretch();
	lHLayout1->addWidget(lDescriptionLabel);
	lHLayout1->addWidget(mDescriptionEdit);
	lVLayout1->addLayout(lHLayout1);
	lVLayout1->addWidget(mConfigPages);
	lVLayout1->setSpacing(0);

	setLayout(lVLayout1);
}

void BackupPlanWidget::saveExtraData() {
	mDriveSelection->saveExtraData();
}


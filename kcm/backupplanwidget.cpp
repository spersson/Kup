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
#include <KInputDialog>
#include <KFileTreeView>
#include <KLineEdit>
#include <KLocale>
#include <KMessageBox>
#include <KNumInput>
#include <KPushButton>
#include <KPageWidget>
#include <KUrlRequester>
#include <KIO/RenameDialog>

#include <QBoxLayout>
#include <QCheckBox>
#include <QFile>
#include <QFormLayout>
#include <QLabel>
#include <QRadioButton>

#include <cmath>

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
		setHeaderHidden(true);
	}
	FolderSelectionModel *mModel;
};

DirDialog::DirDialog(const KUrl &pRootDir, const QString &pStartSubDir, QWidget *pParent)
   : KDialog(pParent)
{
	setCaption(i18nc("@title:window","Select Folder"));
	setButtons(Ok | Cancel | User1);
	setButtonGuiItem(User1, KGuiItem(i18nc("@action:button","New Folder..."), QLatin1String("folder-new")));
	connect(this, SIGNAL(user1Clicked()), this, SLOT(createNewFolder()));
	setDefaultButton(Ok);

	mTreeView = new KFileTreeView(this);
	mTreeView->setDirOnlyMode(true);
	mTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
	for (int i = 1; i < mTreeView->model()->columnCount(); ++i) {
		mTreeView->hideColumn(i);
	}
	mTreeView->setHeaderHidden(true);
	setMainWidget(mTreeView);

	mTreeView->setRootUrl(pRootDir);
	KUrl lSubUrl(pRootDir);
	lSubUrl.addPath(pStartSubDir);
	mTreeView->setCurrentUrl(lSubUrl);
	mTreeView->setFocus();
}

KUrl DirDialog::url() const {
	return mTreeView->currentUrl();
}

void DirDialog::createNewFolder() {
	bool lUserAccepted;
	QString lNameSuggestion = i18nc("default folder name when creating a new folder", "New Folder");
	if(QFileInfo(url().path(KUrl::AddTrailingSlash) + lNameSuggestion).exists()) {
		lNameSuggestion = KIO::RenameDialog::suggestName(url(), lNameSuggestion);
	}

	QString lSelectedName = KInputDialog::getText(i18nc("@title:window", "New Folder" ),
	                                              i18nc("@label:textbox", "Create new folder in:\n%1", url().path()),
	                                              lNameSuggestion, &lUserAccepted, this);
	if (!lUserAccepted)
		return;

	KUrl lPartialUrl(url());
	const QStringList lDirectories = lSelectedName.split('/', QString::SkipEmptyParts);
	foreach(QString lSubDirectory, lDirectories) {
		QDir lDir(lPartialUrl.path());
		if(lDir.exists(lSubDirectory)) {
			lPartialUrl.addPath(lSubDirectory);
			KMessageBox::sorry(this, i18n("A folder named %1 already exists.", lPartialUrl.path()));
			return;
		}
		if(!lDir.mkdir(lSubDirectory)) {
			lPartialUrl.addPath(lSubDirectory);
			KMessageBox::sorry(this, i18n("You do not have permission to create %1.", lPartialUrl.path()));
			return;
		}
		lPartialUrl.addPath(lSubDirectory);
	}
	mTreeView->setCurrentUrl(lPartialUrl);
}

BackupPlanWidget::BackupPlanWidget(BackupPlan *pBackupPlan, const QString &pBupVersion, const QString &pRsyncVersion)
   : QWidget(), mBackupPlan(pBackupPlan)
{
	mDescriptionEdit = new KLineEdit;
	mDescriptionEdit->setObjectName("kcfg_Description");
	mDescriptionEdit->setClearButtonShown(true);
	QLabel *lDescriptionLabel = new QLabel(i18nc("@label", "Description:"));
	lDescriptionLabel->setBuddy(mDescriptionEdit);
	mConfigureButton = new KPushButton(KIcon("go-previous-view"), i18nc("@action:button", "Back to overview"));
	connect(mConfigureButton, SIGNAL(clicked()), this, SIGNAL(requestOverviewReturn()));

	mConfigPages = new KPageWidget;
	mConfigPages->addPage(createTypePage(pBupVersion, pRsyncVersion));
	mConfigPages->addPage(createSourcePage());
	mConfigPages->addPage(createDestinationPage());
	mConfigPages->addPage(createSchedulePage());
	mConfigPages->addPage(createAdvancedPage(pBupVersion));

	QHBoxLayout *lHLayout1 = new QHBoxLayout;
	lHLayout1->addWidget(mConfigureButton);
	lHLayout1->addStretch();
	lHLayout1->addWidget(lDescriptionLabel);
	lHLayout1->addWidget(mDescriptionEdit);

	QVBoxLayout *lVLayout1 = new QVBoxLayout;
	lVLayout1->addLayout(lHLayout1);
	lVLayout1->addWidget(mConfigPages);
	lVLayout1->setSpacing(0);
	setLayout(lVLayout1);
}

void BackupPlanWidget::saveExtraData() {
	mDriveSelection->saveExtraData();
}

KPageWidgetItem *BackupPlanWidget::createTypePage(const QString &pBupVersion, const QString &pRsyncVersion) {
	mVersionedRadio = new QRadioButton;
	QString lVersionedInfo = i18nc("@label", "This type of backup is an <em>archive</em>. It contains both "
	                               "the latest version of your files and earlier backed up versions. "
	                               "Using this type of backup allows you to recover older versions of your "
	                               "files, or files which were deleted on your computer at a later time. "
	                               "The storage space needed is minimized by looking for common parts of "
	                               "your files between versions and only storing those parts once. "
	                               "Nevertheless, the backup archive will keep growing in size as time goes by.<br>"
	                               "Also important to know is that the files in the archive can not be accessed "
	                               "directly with a general file manager, a special program is needed.");
	QLabel *lVersionedInfoLabel = new QLabel(lVersionedInfo);
	lVersionedInfoLabel->setWordWrap(true);
	QWidget *lVersionedWidget = new QWidget;
	lVersionedWidget->setVisible(false);
	QObject::connect(mVersionedRadio, SIGNAL(toggled(bool)), lVersionedWidget, SLOT(setVisible(bool)));
	if(pBupVersion.isEmpty()) {
		mVersionedRadio->setText(i18nc("@option:radio", "Versioned Backup (not available because \"bup\" is not installed)"));
		mVersionedRadio->setEnabled(false);
		lVersionedWidget->setEnabled(false);
	} else {
		mVersionedRadio->setText(i18nc("@option:radio", "Versioned Backup (recommended)"));
	}

	mSyncedRadio = new QRadioButton;
	QString lSyncedInfo = i18nc("@label", "This type of backup is a folder which is synchronized with your "
	                            "selected source folders. Taking a backup simply means making the backup destination "
	                            "contain an exact copy of your source folders as they are now and nothing else. "
	                            "If a file has been deleted in a source folder it will get deleted from the "
	                            "backup folder.<br>This type of backup can protect you against data loss due to a "
	                            "broken hard drive but it does not help you to recover from your own mistakes.");
	QLabel *lSyncedInfoLabel = new QLabel(lSyncedInfo);
	lSyncedInfoLabel->setWordWrap(true);
	QWidget *lSyncedWidget = new QWidget;
	lSyncedWidget->setVisible(false);
	QObject::connect(mSyncedRadio, SIGNAL(toggled(bool)), lSyncedWidget, SLOT(setVisible(bool)));
	if(pRsyncVersion.isEmpty()) {
		mSyncedRadio->setText(i18nc("@option:radio", "Synchronized Backup (not available because \"rsync\" is not installed)"));
		mSyncedRadio->setEnabled(false);
		lSyncedWidget->setEnabled(false);
	} else {
		mSyncedRadio->setText(i18nc("@option:radio", "Synchronized Backup"));
	}

	KButtonGroup *lButtonGroup = new KButtonGroup;
	lButtonGroup->setObjectName("kcfg_Backup type");
	lButtonGroup->setFlat(true);
	int lIndentation = lButtonGroup->style()->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth) +
	                   lButtonGroup->style()->pixelMetric(QStyle::PM_RadioButtonLabelSpacing);

	QGridLayout *lVersionedVLayout = new QGridLayout;
	lVersionedVLayout->setColumnMinimumWidth(0, lIndentation);
	lVersionedVLayout->addWidget(lVersionedInfoLabel, 0, 1);
	lVersionedWidget->setLayout(lVersionedVLayout);

	QGridLayout *lSyncedVLayout = new QGridLayout;
	lSyncedVLayout->setColumnMinimumWidth(0, lIndentation);
	lSyncedVLayout->addWidget(lSyncedInfoLabel, 0, 1);
	lSyncedWidget->setLayout(lSyncedVLayout);

	QVBoxLayout *lVLayout = new QVBoxLayout;
	lVLayout->addWidget(mVersionedRadio);
	lVLayout->addWidget(lVersionedWidget);
	lVLayout->addWidget(mSyncedRadio);
	lVLayout->addWidget(lSyncedWidget);
	lVLayout->addStretch();
	lButtonGroup->setLayout(lVLayout);
	KPageWidgetItem *lPage = new KPageWidgetItem(lButtonGroup);
	lPage->setName(i18nc("@title", "Backup Type"));
	lPage->setHeader(i18nc("@label", "Select what type of backup you want"));
	lPage->setIcon(KIcon(QLatin1String("chronometer")));
	return lPage;
}

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

	int lIndentation = lButtonGroup->style()->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth) +
	                   lButtonGroup->style()->pixelMetric(QStyle::PM_RadioButtonLabelSpacing);

	QVBoxLayout *lVLayout = new QVBoxLayout;
	QRadioButton *lFileSystemRadio = new QRadioButton(i18nc("@option:radio", "Filesystem Path"));
	QRadioButton *lDriveRadio = new QRadioButton(i18nc("@option:radio", "External Storage"));
//	QRadioButton *lSshRadio = new QRadioButton(i18nc("@option:radio", "SSH Server"));

	QWidget *lFileSystemWidget = new QWidget;
	lFileSystemWidget->setVisible(false);
	QObject::connect(lFileSystemRadio, SIGNAL(toggled(bool)), lFileSystemWidget, SLOT(setVisible(bool)));
	QLabel *lFileSystemInfoLabel = new QLabel(i18nc("@label", "You can use this option for backing up to networked storage "
	                                                "if you always mount it at the same path. The path specified here does "
	                                                "not need to exist at all times, its existance will be monitored."));
	lFileSystemInfoLabel->setWordWrap(true);
	QLabel *lFileSystemLabel = new QLabel(i18nc("@label:textbox", "Destination Path for Backup:"));
	KUrlRequester *lFileSystemUrlEdit = new KUrlRequester;
	lFileSystemUrlEdit->setMode(KFile::Directory | KFile::LocalOnly);
	lFileSystemUrlEdit->setObjectName("kcfg_Filesystem destination path");

	QGridLayout *lFileSystemVLayout = new QGridLayout;
	lFileSystemVLayout->setColumnMinimumWidth(0, lIndentation);
	lFileSystemVLayout->addWidget(lFileSystemInfoLabel, 0, 1);
	QHBoxLayout *lFileSystemHLayout = new QHBoxLayout;
	lFileSystemHLayout->addWidget(lFileSystemLabel);
	lFileSystemHLayout->addWidget(lFileSystemUrlEdit, 1);
	lFileSystemVLayout->addLayout(lFileSystemHLayout, 1, 1);
	lFileSystemWidget->setLayout(lFileSystemVLayout);

	QWidget *lDriveWidget = new QWidget;
	lDriveWidget->setVisible(false);
	QObject::connect(lDriveRadio, SIGNAL(toggled(bool)), lDriveWidget, SLOT(setVisible(bool)));
	QLabel *lDriveInfoLabel = new QLabel(i18nc("@label", "Use this option if you want to backup your files on an external "
	                                           "storage that can be plugged in to this computer, such as a USB hard drive "
	                                           "or memory stick."));
	lDriveInfoLabel->setWordWrap(true);
	mDriveSelection = new DriveSelection(mBackupPlan);
	mDriveSelection->setObjectName("kcfg_External drive UUID");
	mDriveDestEdit = new KLineEdit;
	mDriveDestEdit->setObjectName("kcfg_External drive destination path");
	mDriveDestEdit->setToolTip(i18nc("@info:tooltip", "The specified folder will be created if it does not exist."));
	mDriveDestEdit->setClearButtonShown(true);
	QLabel *lDriveDestLabel = new QLabel(i18nc("@label:textbox", "Folder on Destination Drive:"));
	lDriveDestLabel->setToolTip(i18nc("@info:tooltip", "The specified folder will be created if it does not exist."));
	lDriveDestLabel->setBuddy(mDriveDestEdit);
	KPushButton *lDriveDestButton = new KPushButton;
	lDriveDestButton->setIcon(KIcon(QLatin1String("document-open")));
	int lButtonSize = lDriveDestButton->sizeHint().expandedTo(mDriveDestEdit->sizeHint()).height();
	lDriveDestButton->setFixedSize(lButtonSize, lButtonSize);
	lDriveDestButton->setToolTip(i18nc("@info:tooltip", "Open dialog to select a folder"));
	lDriveDestButton->setEnabled(false);
	connect(mDriveSelection, SIGNAL(selectedDriveIsAccessibleChanged(bool)), lDriveDestButton, SLOT(setEnabled(bool)));
	connect(lDriveDestButton, SIGNAL(clicked()), SLOT(openDriveDestDialog()));
	QWidget *lDriveDestWidget = new QWidget;
	lDriveDestWidget->setVisible(false);
	connect(mDriveSelection, SIGNAL(driveIsSelectedChanged(bool)), lDriveDestWidget, SLOT(setVisible(bool)));

	QGridLayout *lDriveVLayout = new QGridLayout;
	lDriveVLayout->setColumnMinimumWidth(0, lIndentation);
	lDriveVLayout->addWidget(lDriveInfoLabel, 0, 1);
	lDriveVLayout->addWidget(mDriveSelection, 1, 1);
	QHBoxLayout *lDriveHLayout = new QHBoxLayout;
	lDriveHLayout->addWidget(lDriveDestLabel);
	lDriveHLayout->addWidget(mDriveDestEdit, 1);
	lDriveHLayout->addWidget(lDriveDestButton);
	lDriveDestWidget->setLayout(lDriveHLayout);
	lDriveVLayout->addWidget(lDriveDestWidget, 2, 1);
	lDriveWidget->setLayout(lDriveVLayout);

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
	lVLayout->addWidget(lFileSystemWidget);
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
	QRadioButton *lManualRadio = new QRadioButton(i18nc("@option:radio", "Manual Activation"));
	QRadioButton *lIntervalRadio = new QRadioButton(i18nc("@option:radio", "Interval"));
	QRadioButton *lUsageRadio = new QRadioButton(i18nc("@option:radio", "Active Usage Time"));

	QLabel *lManualLabel = new QLabel(i18nc("@info", "Backups are only taken when manually requested. "
	                                       "This can be done by using the popup menu from "
	                                       "the backup system tray icon."));
	lManualLabel->setVisible(false);
	lManualLabel->setWordWrap(true);
	connect(lManualRadio, SIGNAL(toggled(bool)), lManualLabel, SLOT(setVisible(bool)));
	QGridLayout *lManualLayout = new QGridLayout;
	lManualLayout->setColumnMinimumWidth(0, lIndentation);
	lManualLayout->addWidget(lManualLabel, 0, 1);

	QWidget *lIntervalWidget = new QWidget;
	lIntervalWidget->setVisible(false);
	connect(lIntervalRadio, SIGNAL(toggled(bool)), lIntervalWidget, SLOT(setVisible(bool)));
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
	connect(lUsageRadio, SIGNAL(toggled(bool)), lUsageWidget, SLOT(setVisible(bool)));
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
	connect(lManualRadio, SIGNAL(toggled(bool)), lAskFirstCheckBox, SLOT(setHidden(bool)));

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

KPageWidgetItem *BackupPlanWidget::createAdvancedPage(const QString &pBupVersion) {
	QWidget *lAdvancedWidget = new QWidget(this);
	QFormLayout *lAdvancedLayout = new QFormLayout;

	QCheckBox *lShowHiddenCheckBox = new QCheckBox(i18nc("@option:check", "Yes"));
	lShowHiddenCheckBox->setObjectName(QLatin1String("kcfg_Show hidden folders"));
	lAdvancedLayout->addRow(i18nc("@label", "Show hidden folders in source selection:"), lShowHiddenCheckBox);

	QCheckBox *lRunAsRootCheckBox = new QCheckBox(i18nc("@option:check", "Yes"));
	lRunAsRootCheckBox->setObjectName(QLatin1String("kcfg_Run as root"));
	lAdvancedLayout->addRow(i18nc("@label", "Take backups as root:"), lRunAsRootCheckBox);

	lAdvancedWidget->setLayout(lAdvancedLayout);
	connect(lShowHiddenCheckBox, SIGNAL(toggled(bool)), mSourceSelectionModel, SLOT(setHiddenFoldersShown(bool)));
	KPageWidgetItem *lPage = new KPageWidgetItem(lAdvancedWidget);
	lPage->setName(i18nc("@title", "Advanced"));
	lPage->setHeader(i18nc("@label", "Extra options for advanced users"));
	lPage->setIcon(KIcon("preferences-other"));
	return lPage;
}

void BackupPlanWidget::openDriveDestDialog() {
	QString lMountPoint = mDriveSelection->mountPathOfSelectedDrive();
	QString lSelectedPath;
	DirDialog lDirDialog(lMountPoint, mDriveDestEdit->text(), this);
	if(lDirDialog.exec() == QDialog::Accepted) {
		lSelectedPath = lDirDialog.url().path();
		lSelectedPath.remove(0, lMountPoint.length());
		while(lSelectedPath.startsWith('/')) {
			lSelectedPath.remove(0, 1);
		}
		mDriveDestEdit->setText(lSelectedPath);
	}
}


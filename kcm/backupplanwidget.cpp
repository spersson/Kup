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
#include "dirselector.h"
#include "driveselection.h"
#include "kbuttongroup.h"

#include <QAction>
#include <QBoxLayout>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QEvent>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QThread>
#include <QTimer>
#include <QTreeView>
#include <QSpinBox>
#include <QFormLayout>

#include <KComboBox>
#include <KConfigDialogManager>
#include <KConfigGroup>
#include <KLineEdit>
#include <KLocalizedString>
#include <KMessageWidget>
#include <KPageWidget>
#include <KUrlRequester>

class ScanFolderEvent : public QEvent {
public:
	ScanFolderEvent(QString pPath)
	   : QEvent(eventType) {
		mPath = pPath;
	}
	QString mPath;
	static const QEvent::Type eventType = (QEvent::Type)(QEvent::User + 1);
};

FileScanner::FileScanner() {
	// create a timer that will call a slot to send the pending updates to UI, one second
	// after the last update comes in, just to minimize risk of showing incomplete
	// information to the user.
	mUnreadablesTimer = new QTimer(this);
	mUnreadablesTimer->setSingleShot(true);
	mUnreadablesTimer->setInterval(1000);
	connect(mUnreadablesTimer, &QTimer::timeout, this, &FileScanner::sendPendingUnreadables);

	mSymlinkTimer = new QTimer(this);
	mSymlinkTimer->setSingleShot(true);
	mSymlinkTimer->setInterval(1000);
	connect(mSymlinkTimer, &QTimer::timeout, this, &FileScanner::sendPendingSymlinks);
}

bool FileScanner::event(QEvent *pEvent) {
	if(pEvent->type() == ScanFolderEvent::eventType) {
		ScanFolderEvent *lEvent = (ScanFolderEvent *)pEvent;
		if(isPathIncluded(lEvent->mPath)) {
			scanFolder(lEvent->mPath);
		}
		return true;
	}
	return QObject::event(pEvent);
}

void FileScanner::includePath(QString pPath) {
	if(!mExcludedFolders.remove(pPath)) {
		mIncludedFolders += pPath;
	}
	checkPathForProblems(QFileInfo(pPath));

	QMutableHashIterator<QString, QString> i(mSymlinksNotOk);
	while(i.hasNext()) {
		i.next();
		if(isPathIncluded(i.value())) {
			mSymlinksOk.insert(i.key(), i.value());
			i.remove();
			mSymlinkTimer->start();
		}
	}

}

void FileScanner::excludePath(QString pPath) {
	if(!mIncludedFolders.remove(pPath)) {
		mExcludedFolders += pPath;
	}
	QString lPath = pPath + QStringLiteral("/");
	QSet<QString>::iterator it = mUnreadableFiles.begin();
	while(it != mUnreadableFiles.end()) {
		if(it->startsWith(lPath)) {
			mUnreadablesTimer->start();
			it = mUnreadableFiles.erase(it);
		} else {
			++it;
		}
	}
	it = mUnreadableFolders.begin();
	while(it != mUnreadableFolders.end()) {
		if(it->startsWith(lPath) || *it == pPath) {
			mUnreadablesTimer->start();
			it = mUnreadableFolders.erase(it);
		} else {
			++it;
		}
	}

	QMutableHashIterator<QString, QString> i(mSymlinksNotOk);
	while(i.hasNext()) {
		if(!isPathIncluded(i.next().key())) {
			i.remove();
			mSymlinkTimer->start();
		}
	}

	i = mSymlinksOk;
	while(i.hasNext()) {
		i.next();
		if(!isPathIncluded(i.key())) {
			i.remove();
		} else if(isSymlinkProblematic(i.value())) {
			mSymlinksNotOk.insert(i.key(), i.value());
			mSymlinkTimer->start();
			i.remove();
		}
	}
}

void FileScanner::sendPendingUnreadables() {
	emit unreadablesChanged(QPair<QSet<QString>, QSet<QString>>(mUnreadableFolders, mUnreadableFiles));
}

void FileScanner::sendPendingSymlinks() {
	emit symlinkProblemsChanged(mSymlinksNotOk);
}

bool FileScanner::isPathIncluded(const QString &pPath) {
	int lLongestInclude = 0;
	foreach(const QString &lPath, mIncludedFolders) {
		bool lMatches = pPath == lPath || pPath.startsWith(lPath + QStringLiteral("/"));
		if(lMatches && lPath.length() > lLongestInclude) {
			lLongestInclude = lPath.length();
		}
	}
	int lLongestExclude = 0;
	foreach(const QString &lPath, mExcludedFolders) {
		bool lMatches = pPath == lPath || pPath.startsWith(lPath + QStringLiteral("/"));
		if(lMatches && lPath.length() > lLongestExclude) {
			lLongestExclude = lPath.length();
		}
	}
	return lLongestInclude > lLongestExclude;
}

void FileScanner::checkPathForProblems(const QFileInfo &pFileInfo) {
	if(pFileInfo.isSymLink()) {
		if(isSymlinkProblematic(pFileInfo.symLinkTarget())) {
			mSymlinksNotOk.insert(pFileInfo.absoluteFilePath(), pFileInfo.symLinkTarget());
			mSymlinkTimer->start();
		} else {
			mSymlinksOk.insert(pFileInfo.absoluteFilePath(), pFileInfo.symLinkTarget());
		}
	} else if(pFileInfo.isDir()) {
		QCoreApplication::postEvent(this, new ScanFolderEvent(pFileInfo.absoluteFilePath()),
		                            Qt::LowEventPriority);
	} else {
		if(!pFileInfo.isReadable()) {
			mUnreadableFiles += pFileInfo.absoluteFilePath();
			mUnreadablesTimer->start();
		}
	}
}

bool FileScanner::isSymlinkProblematic(const QString &pTarget) {
	QFileInfo lTargetInfo(pTarget);
	return lTargetInfo.exists() && !isPathIncluded(pTarget) &&
	      !pTarget.startsWith(QStringLiteral("/tmp/")) &&
	      !pTarget.startsWith(QStringLiteral("/var/tmp/"));
}

void FileScanner::scanFolder(const QString &pPath) {
	QDir lDir(pPath);
	if(!lDir.isReadable()) {
		mUnreadableFolders += pPath;
		mUnreadablesTimer->start();
	} else {
		QFileInfoList lInfoList = lDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::Hidden| QDir::NoDotAndDotDot);
		foreach(const QFileInfo &lFileInfo, lInfoList) {
			checkPathForProblems(lFileInfo);
		}
	}
}

ConfigIncludeDummy::ConfigIncludeDummy(FolderSelectionModel *pModel, FolderSelectionWidget *pParent)
   : QWidget(pParent), mModel(pModel), mTreeView(pParent)
{
	connect(mModel, &FolderSelectionModel::includedPathAdded, this, &ConfigIncludeDummy::includeListChanged);
	connect(mModel, &FolderSelectionModel::includedPathRemoved, this, &ConfigIncludeDummy::includeListChanged);
	KConfigDialogManager::changedMap()->insert(QStringLiteral("ConfigIncludeDummy"),
	                                           SIGNAL(includeListChanged()));
}

QStringList ConfigIncludeDummy::includeList() {
	QStringList lList = mModel->includedPaths().toList();
	lList.sort();
	return lList;
}

void ConfigIncludeDummy::setIncludeList(QStringList pIncludeList) {
	for(int i = 0; i < pIncludeList.count(); ++i) {
		if(!QFile::exists(pIncludeList.at(i))) {
			pIncludeList.removeAt(i--);
		}
	}
	mModel->setIncludedPaths(QSet<QString>::fromList(pIncludeList));
	mTreeView->expandToShowSelections();
}

ConfigExcludeDummy::ConfigExcludeDummy(FolderSelectionModel *pModel, FolderSelectionWidget *pParent)
   : QWidget(pParent), mModel(pModel), mTreeView(pParent)
{
	connect(mModel, &FolderSelectionModel::excludedPathAdded, this, &ConfigExcludeDummy::excludeListChanged);
	connect(mModel, &FolderSelectionModel::excludedPathRemoved, this, &ConfigExcludeDummy::excludeListChanged);
	KConfigDialogManager::changedMap()->insert(QStringLiteral("ConfigExcludeDummy"),
	                                           SIGNAL(excludeListChanged()));
}

QStringList ConfigExcludeDummy::excludeList() {
	QStringList lList = mModel->excludedPaths().toList();
	lList.sort();
	return lList;
}

void ConfigExcludeDummy::setExcludeList(QStringList pExcludeList) {
	for(int i = 0; i < pExcludeList.count(); ++i) {
		if(!QFile::exists(pExcludeList.at(i))) {
			pExcludeList.removeAt(i--);
		}
	}
	mModel->setExcludedPaths(QSet<QString>::fromList(pExcludeList));
	mTreeView->expandToShowSelections();
}

FolderSelectionWidget::FolderSelectionWidget(FolderSelectionModel *pModel, QWidget *pParent)
   : QWidget(pParent), mModel(pModel)
{
	mMessageWidget = new KMessageWidget(this);
	mMessageWidget->setCloseButtonVisible(false);
	mMessageWidget->setWordWrap(true);
	mMessageWidget->hide();
	mTreeView = new QTreeView(this);
	QVBoxLayout *lVLayout = new QVBoxLayout;
	lVLayout->addWidget(mMessageWidget);
	lVLayout->addWidget(mTreeView, 1);
	setLayout(lVLayout);

	connect(mMessageWidget, &KMessageWidget::hideAnimationFinished,
	        this, &FolderSelectionWidget::updateMessage);

	mExcludeAction = new QAction(xi18nc("@action:button", "Exclude Folder"), this);
	connect(mExcludeAction, &QAction::triggered, this, &FolderSelectionWidget::executeExcludeAction);

	mIncludeAction = new QAction(xi18nc("@action:button", "Include Folder"), this);
	connect(mIncludeAction, &QAction::triggered, this, &FolderSelectionWidget::executeIncludeAction);

	mModel->setRootPath(QStringLiteral("/"));
	mModel->setParent(this);
	mTreeView->setAnimated(true);
	mTreeView->setModel(mModel);
	mTreeView->setHeaderHidden(true);
	// always expand the root, prevents problem with empty include&exclude lists.
	mTreeView->expand(mModel->index(QStringLiteral("/")));

	ConfigIncludeDummy *lIncludeDummy = new ConfigIncludeDummy(mModel, this);
	lIncludeDummy->setObjectName(QStringLiteral("kcfg_Paths included"));
	ConfigExcludeDummy *lExcludeDummy = new ConfigExcludeDummy(mModel, this);
	lExcludeDummy->setObjectName(QStringLiteral("kcfg_Paths excluded"));

	qRegisterMetaType<QPair<QSet<QString>,QSet<QString>>>("QPair<QSet<QString>,QSet<QString>>");
	qRegisterMetaType<QHash<QString,QString>>("QHash<QString,QString>");

	mWorkerThread = new QThread(this);
	FileScanner *lFileScanner = new FileScanner;
	lFileScanner->moveToThread(mWorkerThread);
	connect(mWorkerThread, &QThread::finished, lFileScanner, &QObject::deleteLater);

	connect(mModel, &FolderSelectionModel::includedPathAdded,
	        lFileScanner, &FileScanner::includePath);
	connect(mModel, &FolderSelectionModel::excludedPathRemoved,
	        lFileScanner, &FileScanner::includePath);
	connect(mModel, &FolderSelectionModel::excludedPathAdded,
	        lFileScanner, &FileScanner::excludePath);
	connect(mModel, &FolderSelectionModel::includedPathRemoved,
	        lFileScanner, &FileScanner::excludePath);
	connect(lFileScanner, &FileScanner::unreadablesChanged,
	        this, &FolderSelectionWidget::setUnreadables);
	connect(lFileScanner, &FileScanner::symlinkProblemsChanged,
	        this, &FolderSelectionWidget::setSymlinks);
	mWorkerThread->start();
}

FolderSelectionWidget::~FolderSelectionWidget() {
	mWorkerThread->quit();
	mWorkerThread->wait();
}

void FolderSelectionWidget::setHiddenFoldersVisible(bool pVisible) {
	mModel->setHiddenFoldersVisible(pVisible);
	// give the filesystem model some time to refresh after changing filtering
	// before expanding folders again.
	if(pVisible) {
		QTimer::singleShot(2000, this, SLOT(expandToShowSelections()));
	}
}

void FolderSelectionWidget::expandToShowSelections() {
	foreach(const QString& lFolder,  mModel->includedPaths() + mModel->excludedPaths()) {
		QFileInfo lFolderInfo(lFolder);
		bool lShouldBeShown = true;
		while(lFolderInfo.absoluteFilePath() != QStringLiteral("/")) {
			if(lFolderInfo.isHidden() && !mModel->hiddenFoldersVisible()) {
				lShouldBeShown = false; // skip if this folder should not be shown.
				break;
			}
			lFolderInfo = lFolderInfo.absolutePath(); // move up one level
		}
		if(lShouldBeShown) {
			QModelIndex lIndex = mModel->index(lFolder).parent();
			while(lIndex.isValid()) {
				mTreeView->expand(lIndex);
				lIndex = lIndex.parent();
			}
		}
	}
}

void FolderSelectionWidget::setUnreadables(QPair<QSet<QString>, QSet<QString> > pUnreadables) {
	mUnreadableFolders = pUnreadables.first.toList();
	mUnreadableFiles = pUnreadables.second.toList();
	updateMessage();
}

void FolderSelectionWidget::setSymlinks(QHash<QString, QString> pSymlinks) {
	mSymlinkProblems = pSymlinks;
	updateMessage();
}

void FolderSelectionWidget::updateMessage() {
	if(mMessageWidget->isVisible() || mMessageWidget->isHideAnimationRunning()) {
		mMessageWidget->animatedHide();
		return;
	}

	mMessageWidget->removeAction(mExcludeAction);
	mMessageWidget->removeAction(mIncludeAction);

	if(!mUnreadableFolders.isEmpty()) {
		mMessageWidget->setMessageType(KMessageWidget::Error);
		mMessageWidget->setText(xi18nc("@info message bar appearing on top",
		                              "You don't have permission to read this folder: <filename>%1</filename><nl/>"
		                              "It cannot be included in the source selection. "
		                              "If it does not contain anything important to you, one possible "
		                              "solution is to exclude the folder from the backup plan.",
		                              mUnreadableFolders.first()));
		mExcludeActionPath = mUnreadableFolders.first();
		mMessageWidget->addAction(mExcludeAction);
		mMessageWidget->animatedShow();
	} else if(!mUnreadableFiles.isEmpty()) {
		mMessageWidget->setMessageType(KMessageWidget::Error);
		mMessageWidget->setText(xi18nc("@info message bar appearing on top",
		                              "You don't have permission to read this file: <filename>%1</filename><nl/>"
		                              "It cannot be included in the source selection. "
		                              "If the file is not important to you, one possible solution is "
		                              "to exclude the whole folder where the file is stored from the backup plan.",
		                              mUnreadableFiles.first()));
		QFileInfo lFileInfo = mUnreadableFiles.first();
		mExcludeActionPath = lFileInfo.absolutePath();
		mMessageWidget->addAction(mExcludeAction);
		mMessageWidget->animatedShow();
	} else if(!mSymlinkProblems.isEmpty()) {
		mMessageWidget->setMessageType(KMessageWidget::Warning);
		QHashIterator<QString,QString> i(mSymlinkProblems);
		i.next();
		QFileInfo lFileInfo =i.value();
		if(lFileInfo.isDir()) {
			mMessageWidget->setText(xi18nc("@info message bar appearing on top",
			                              "The symbolic link <filename>%1</filename> is currently included but it points "
			                              "to a folder which is not: <filename>%2</filename>.<nl/>That is probably not "
			                              "what you want. One solution is to simply include the target folder in the "
			                              "backup plan.",
			                              i.key(), i.value()));
			mIncludeActionPath = i.value();
		} else {
			mMessageWidget->setText(xi18nc("@info message bar appearing on top",
			                              "The symbolic link <filename>%1</filename> is currently included but it points "
			                              "to a file which is not: <filename>%2</filename>.<nl/>That is probably not "
			                              "what you want. One solution is to simply include the folder where the file "
			                              "is stored in the backup plan.",
			                              i.key(), i.value()));
			mIncludeActionPath = lFileInfo.absolutePath();
		}
		mMessageWidget->addAction(mIncludeAction);
		mMessageWidget->animatedShow();
	}
}

void FolderSelectionWidget::executeExcludeAction() {
	mModel->excludePath(mExcludeActionPath);
}

void FolderSelectionWidget::executeIncludeAction() {
	mModel->includePath(mIncludeActionPath);
}

DirDialog::DirDialog(const QUrl &pRootDir, const QString &pStartSubDir, QWidget *pParent)
   : QDialog(pParent)
{
	setWindowTitle(xi18nc("@title:window","Select Folder"));

	mDirSelector = new DirSelector(this);
	mDirSelector->setRootUrl(pRootDir);
	QUrl lSubUrl = QUrl::fromLocalFile(pRootDir.adjusted(QUrl::StripTrailingSlash).path() + '/' +
	                                    pStartSubDir);
	mDirSelector->expandToUrl(lSubUrl);

	QDialogButtonBox *lButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
	connect(lButtonBox, SIGNAL(accepted()), this, SLOT(accept()));
	connect(lButtonBox, SIGNAL(rejected()), this, SLOT(reject()));
	QPushButton *lNewFolderButton = new QPushButton(xi18nc("@action:button", "New Folder..."));
	connect(lNewFolderButton, SIGNAL(clicked()), mDirSelector, SLOT(createNewFolder()));
	lButtonBox->addButton(lNewFolderButton, QDialogButtonBox::ActionRole);
	QPushButton *lOkButton = lButtonBox->button(QDialogButtonBox::Ok);
	lOkButton->setDefault(true);
	lOkButton->setShortcut(Qt::Key_Return);

	QVBoxLayout *lMainLayout = new QVBoxLayout;
	lMainLayout->addWidget(mDirSelector);
	lMainLayout->addWidget(lButtonBox);
	setLayout(lMainLayout);

	mDirSelector->setFocus();
}

QUrl DirDialog::url() const {
	return mDirSelector->url();
}


BackupPlanWidget::BackupPlanWidget(BackupPlan *pBackupPlan, const QString &pBupVersion,
                                   const QString &pResticVersion,
                                   const QString &pRsyncVersion, bool pPar2Available)
   : QWidget(), mBackupPlan(pBackupPlan)
{
	mDescriptionEdit = new KLineEdit;
	mDescriptionEdit->setObjectName(QStringLiteral("kcfg_Description"));
	mDescriptionEdit->setClearButtonShown(true);
	QLabel *lDescriptionLabel = new QLabel(xi18nc("@label", "Description:"));
	lDescriptionLabel->setBuddy(mDescriptionEdit);
	mConfigureButton = new QPushButton(QIcon::fromTheme(QStringLiteral("go-previous-view")),
	                                   xi18nc("@action:button", "Back to overview"));
	connect(mConfigureButton, SIGNAL(clicked()), this, SIGNAL(requestOverviewReturn()));

	mConfigPages = new KPageWidget;
	mConfigPages->addPage(createTypePage(pBupVersion, pResticVersion, pRsyncVersion));
	mConfigPages->addPage(createSourcePage());
	mConfigPages->addPage(createDestinationPage());
	mConfigPages->addPage(createSchedulePage());
	mConfigPages->addPage(createAdvancedPage(pPar2Available));

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

KPageWidgetItem *BackupPlanWidget::createTypePage(const QString &pBupVersion,
												  const QString &pResticVersion,
												  const QString &pRsyncVersion) {
	mVersionedRadio = new QRadioButton;
	QString lVersionedInfo = xi18nc("@info", "This type of backup is an <emphasis>archive</emphasis>. It contains both "
	                               "the latest version of your files and earlier backed up versions. "
	                               "Using this type of backup allows you to recover older versions of your "
	                               "files, or files which were deleted on your computer at a later time. "
	                               "The storage space needed is minimized by looking for common parts of "
	                               "your files between versions and only storing those parts once. "
	                               "Nevertheless, the backup archive will keep growing in size as time goes by.<nl/>"
	                               "Also important to know is that the files in the archive can not be accessed "
	                               "directly with a general file manager, a special program is needed.");
	QLabel *lVersionedInfoLabel = new QLabel(lVersionedInfo);
	lVersionedInfoLabel->setWordWrap(true);
	QWidget *lVersionedWidget = new QWidget;
	lVersionedWidget->setVisible(false);
	QObject::connect(mVersionedRadio, SIGNAL(toggled(bool)), lVersionedWidget, SLOT(setVisible(bool)));
	if(pBupVersion.isEmpty()) {
		mVersionedRadio->setText(xi18nc("@option:radio", "Versioned Backup (not available "
		                                                 "because <application>bup</application> is not installed)"));
		mVersionedRadio->setEnabled(false);
		lVersionedWidget->setEnabled(false);
	} else {
		mVersionedRadio->setText(xi18nc("@option:radio", "Versioned Backup (recommended)"));
	}

	mSyncedRadio = new QRadioButton;
	QString lSyncedInfo = xi18nc("@info",
	                            "This type of backup is a folder which is synchronized with your "
	                            "selected source folders. Taking a backup simply means making the "
	                            "backup destination contain an exact copy of your source folders as "
	                            "they are now and nothing else. If a file has been deleted in a "
	                            "source folder it will get deleted from the backup folder.<nl/>"
	                            "This type of backup can protect you against data loss due to a broken "
	                            "hard drive but it does not help you to recover from your own mistakes.");
	QLabel *lSyncedInfoLabel = new QLabel(lSyncedInfo);
	lSyncedInfoLabel->setWordWrap(true);
	QWidget *lSyncedWidget = new QWidget;
	lSyncedWidget->setVisible(false);
	QObject::connect(mSyncedRadio, SIGNAL(toggled(bool)), lSyncedWidget, SLOT(setVisible(bool)));
	if(pRsyncVersion.isEmpty()) {
		mSyncedRadio->setText(xi18nc("@option:radio", "Synchronized Backup (not available "
		                                              "because <application>rsync</application> is not installed)"));
		mSyncedRadio->setEnabled(false);
		lSyncedWidget->setEnabled(false);
	} else {
		mSyncedRadio->setText(xi18nc("@option:radio", "Synchronized Backup"));
	}
	
	mResticRadio = new QRadioButton;
	QString lResticInfo = xi18nc("@info",
								 "This type of backup is a versioned backup based on <application>Restic</application>.");
	QLabel* lResticInfoLabel = new QLabel(lResticInfo);
	lResticInfoLabel->setWordWrap(true);
	QWidget *lResticWidget = new QWidget;
	lResticWidget->setVisible(false);
	connect(mResticRadio, SIGNAL(toggled(bool)), lResticWidget, SLOT(setVisible(bool)));
	if(pResticVersion.isEmpty()) {
		mResticRadio->setText(xi18nc("@option:radio", "Versioned backup based on Restic (not available "
													  "because <application>Restic</application> is not installed)"));
		mResticRadio->setEnabled(false);
		lResticWidget->setEnabled(false);
	} else {
		mResticRadio->setText(xi18nc("@option:radio", "Versioned backup based on Restic"));
	}
	
	KButtonGroup *lButtonGroup = new KButtonGroup;
	lButtonGroup->setObjectName(QStringLiteral("kcfg_Backup type"));
	lButtonGroup->setFlat(true);
	int lIndentation = lButtonGroup->style()->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth) +
	                   lButtonGroup->style()->pixelMetric(QStyle::PM_RadioButtonLabelSpacing);

	QGridLayout *lVersionedVLayout = new QGridLayout;
	lVersionedVLayout->setColumnMinimumWidth(0, lIndentation);
	lVersionedVLayout->setContentsMargins(0, 0, 0, 0);
	lVersionedVLayout->addWidget(lVersionedInfoLabel, 0, 1);
	lVersionedWidget->setLayout(lVersionedVLayout);

	QGridLayout *lSyncedVLayout = new QGridLayout;
	lSyncedVLayout->setColumnMinimumWidth(0, lIndentation);
	lSyncedVLayout->setContentsMargins(0, 0, 0, 0);
	lSyncedVLayout->addWidget(lSyncedInfoLabel, 0, 1);
	lSyncedWidget->setLayout(lSyncedVLayout);

	QGridLayout *lResticVLayout = new QGridLayout;
	lResticVLayout->setColumnMinimumWidth(0, lIndentation);
	lResticVLayout->setContentsMargins(0, 0, 0, 0);
	lResticVLayout->addWidget(lResticInfoLabel, 0, 1);
	lResticWidget->setLayout(lResticVLayout);

	QVBoxLayout *lVLayout = new QVBoxLayout;
	lVLayout->addWidget(mVersionedRadio);
	lVLayout->addWidget(lVersionedWidget);
	lVLayout->addWidget(mSyncedRadio);
	lVLayout->addWidget(lSyncedWidget);
	lVLayout->addWidget(mResticRadio);
	lVLayout->addWidget(lResticWidget);
	lVLayout->addStretch();
	lButtonGroup->setLayout(lVLayout);

	KPageWidgetItem *lPage = new KPageWidgetItem(lButtonGroup);
	lPage->setName(xi18nc("@title", "Backup Type"));
	lPage->setHeader(xi18nc("@label", "Select what type of backup you want"));
	lPage->setIcon(QIcon::fromTheme(QStringLiteral("folder-sync")));
	return lPage;
}

KPageWidgetItem *BackupPlanWidget::createSourcePage() {
	mSourceSelectionWidget = new FolderSelectionWidget(new FolderSelectionModel(mBackupPlan->mShowHiddenFolders), this);
	KPageWidgetItem *lPage = new KPageWidgetItem(mSourceSelectionWidget);
	lPage->setName(xi18nc("@title", "Sources"));
	lPage->setHeader(xi18nc("@label", "Select which folders to include in backup"));
	lPage->setIcon(QIcon::fromTheme(QStringLiteral("cloud-upload")));
	return lPage;
}

KPageWidgetItem *BackupPlanWidget::createDestinationPage() {
	KButtonGroup *lButtonGroup = new KButtonGroup(this);
	lButtonGroup->setObjectName(QStringLiteral("kcfg_Destination type"));
	lButtonGroup->setFlat(true);

	int lIndentation = lButtonGroup->style()->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth) +
	                   lButtonGroup->style()->pixelMetric(QStyle::PM_RadioButtonLabelSpacing);

	QVBoxLayout *lVLayout = new QVBoxLayout;
	QRadioButton *lFileSystemRadio = new QRadioButton(xi18nc("@option:radio", "Filesystem Path"));
	QRadioButton *lDriveRadio = new QRadioButton(xi18nc("@option:radio", "External Storage"));

	QWidget *lFileSystemWidget = new QWidget;
	lFileSystemWidget->setVisible(false);
	QObject::connect(lFileSystemRadio, SIGNAL(toggled(bool)), lFileSystemWidget, SLOT(setVisible(bool)));
	QLabel *lFileSystemInfoLabel = new QLabel(xi18nc("@info",
	                                                 "You can use this option for backing up to a secondary "
	                                                 "internal harddrive, an external eSATA drive or networked "
	                                                 "storage. The requirement is just that you always mount "
	                                                 "it at the same path in the filesystem. The path "
	                                                 "specified here does not need to exist at all times, its "
	                                                 "existance will be monitored."));
	lFileSystemInfoLabel->setWordWrap(true);
	QLabel *lFileSystemLabel = new QLabel(xi18nc("@label:textbox", "Destination Path for Backup:"));
	KUrlRequester *lFileSystemUrlEdit = new KUrlRequester;
	lFileSystemUrlEdit->setMode(KFile::Directory | KFile::LocalOnly);
	lFileSystemUrlEdit->setObjectName(QStringLiteral("kcfg_Filesystem destination path"));

	QGridLayout *lFileSystemVLayout = new QGridLayout;
	lFileSystemVLayout->setColumnMinimumWidth(0, lIndentation);
	lFileSystemVLayout->setContentsMargins(0, 0, 0, 0);
	lFileSystemVLayout->addWidget(lFileSystemInfoLabel, 0, 1);
	QHBoxLayout *lFileSystemHLayout = new QHBoxLayout;
	lFileSystemHLayout->addWidget(lFileSystemLabel);
	lFileSystemHLayout->addWidget(lFileSystemUrlEdit, 1);
	lFileSystemVLayout->addLayout(lFileSystemHLayout, 1, 1);
	lFileSystemWidget->setLayout(lFileSystemVLayout);

	QWidget *lDriveWidget = new QWidget;
	lDriveWidget->setVisible(false);
	QObject::connect(lDriveRadio, SIGNAL(toggled(bool)), lDriveWidget, SLOT(setVisible(bool)));
	QLabel *lDriveInfoLabel = new QLabel(xi18nc("@info",
	                                           "Use this option if you want to backup your "
	                                           "files on an external storage that can be plugged in "
	                                           "to this computer, such as a USB hard drive or memory "
	                                           "stick."));
	lDriveInfoLabel->setWordWrap(true);
	mDriveSelection = new DriveSelection(mBackupPlan);
	mDriveSelection->setObjectName(QStringLiteral("kcfg_External drive UUID"));
	mDriveDestEdit = new KLineEdit;
	mDriveDestEdit->setObjectName(QStringLiteral("kcfg_External drive destination path"));
	mDriveDestEdit->setToolTip(xi18nc("@info:tooltip",
	                                  "The specified folder will be created if it does not exist."));
	mDriveDestEdit->setClearButtonShown(true);
	QLabel *lDriveDestLabel = new QLabel(xi18nc("@label:textbox", "Folder on Destination Drive:"));
	lDriveDestLabel->setToolTip(xi18nc("@info:tooltip",
	                                   "The specified folder will be created if it does not exist."));
	lDriveDestLabel->setBuddy(mDriveDestEdit);
	QPushButton *lDriveDestButton = new QPushButton;
	lDriveDestButton->setIcon(QIcon::fromTheme(QStringLiteral("document-open")));
	int lButtonSize = lDriveDestButton->sizeHint().expandedTo(mDriveDestEdit->sizeHint()).height();
	lDriveDestButton->setFixedSize(lButtonSize, lButtonSize);
	lDriveDestButton->setToolTip(xi18nc("@info:tooltip", "Open dialog to select a folder"));
	lDriveDestButton->setEnabled(false);
	connect(mDriveSelection, SIGNAL(selectedDriveIsAccessibleChanged(bool)), lDriveDestButton, SLOT(setEnabled(bool)));
	connect(lDriveDestButton, SIGNAL(clicked()), SLOT(openDriveDestDialog()));
	QWidget *lDriveDestWidget = new QWidget;
	lDriveDestWidget->setVisible(false);
	connect(mDriveSelection, SIGNAL(driveIsSelectedChanged(bool)), lDriveDestWidget, SLOT(setVisible(bool)));
	connect(mSyncedRadio, SIGNAL(toggled(bool)), mDriveSelection, SLOT(updateSyncWarning(bool)));

	QGridLayout *lDriveVLayout = new QGridLayout;
	lDriveVLayout->setColumnMinimumWidth(0, lIndentation);
	lDriveVLayout->setContentsMargins(0, 0, 0, 0);
	lDriveVLayout->addWidget(lDriveInfoLabel, 0, 1);
	lDriveVLayout->addWidget(mDriveSelection, 1, 1);
	QHBoxLayout *lDriveHLayout = new QHBoxLayout;
	lDriveHLayout->addWidget(lDriveDestLabel);
	lDriveHLayout->addWidget(mDriveDestEdit, 1);
	lDriveHLayout->addWidget(lDriveDestButton);
	lDriveDestWidget->setLayout(lDriveHLayout);
	lDriveVLayout->addWidget(lDriveDestWidget, 2, 1);
	lDriveWidget->setLayout(lDriveVLayout);

	lVLayout->addWidget(lFileSystemRadio);
	lVLayout->addWidget(lFileSystemWidget);
	lVLayout->addWidget(lDriveRadio);
	lVLayout->addWidget(lDriveWidget, 1);
	lVLayout->addStretch();
	lButtonGroup->setLayout(lVLayout);

	KPageWidgetItem *lPage = new KPageWidgetItem(lButtonGroup);
	lPage->setName(xi18nc("@title", "Destination"));
	lPage->setHeader(xi18nc("@label", "Select the backup destination"));
	lPage->setIcon(QIcon::fromTheme(QStringLiteral("cloud-download")));
	return lPage;
}

KPageWidgetItem *BackupPlanWidget::createSchedulePage() {
	QWidget *lTopWidget = new QWidget(this);
	QVBoxLayout *lTopLayout = new QVBoxLayout;
	KButtonGroup *lButtonGroup = new KButtonGroup;
	lButtonGroup->setObjectName(QStringLiteral("kcfg_Schedule type"));
	lButtonGroup->setFlat(true);

	int lIndentation = lButtonGroup->style()->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth) +
	                   lButtonGroup->style()->pixelMetric(QStyle::PM_RadioButtonLabelSpacing);

	QVBoxLayout *lVLayout = new QVBoxLayout;
	lVLayout->setContentsMargins(0, 0, 0, 0);
	QRadioButton *lManualRadio = new QRadioButton(xi18nc("@option:radio", "Manual Activation"));
	QRadioButton *lIntervalRadio = new QRadioButton(xi18nc("@option:radio", "Interval"));
	QRadioButton *lUsageRadio = new QRadioButton(xi18nc("@option:radio", "Active Usage Time"));

	QLabel *lManualLabel = new QLabel(xi18nc("@info", "Backups are only taken when manually requested. "
	                                                  "This can be done by using the popup menu from "
	                                                  "the backup system tray icon."));
	lManualLabel->setVisible(false);
	lManualLabel->setWordWrap(true);
	connect(lManualRadio, SIGNAL(toggled(bool)), lManualLabel, SLOT(setVisible(bool)));
	QGridLayout *lManualLayout = new QGridLayout;
	lManualLayout->setColumnMinimumWidth(0, lIndentation);
	lManualLayout->setContentsMargins(0, 0, 0, 0);
	lManualLayout->addWidget(lManualLabel, 0, 1);

	QWidget *lIntervalWidget = new QWidget;
	lIntervalWidget->setVisible(false);
	connect(lIntervalRadio, SIGNAL(toggled(bool)), lIntervalWidget, SLOT(setVisible(bool)));
	QLabel *lIntervalLabel = new QLabel(xi18nc("@info", "New backup will be triggered when backup "
	                                                    "destination becomes available and more than "
	                                                    "the configured interval has passed since the "
	                                                    "last backup was taken."));
	lIntervalLabel->setWordWrap(true);
	QGridLayout *lIntervalVertLayout = new QGridLayout;
	lIntervalVertLayout->setColumnMinimumWidth(0, lIndentation);
	lIntervalVertLayout->setContentsMargins(0, 0, 0, 0);
	lIntervalVertLayout->addWidget(lIntervalLabel, 0, 1);
	QHBoxLayout *lIntervalLayout = new QHBoxLayout;
	lIntervalLayout->setContentsMargins(0, 0, 0, 0);
	QSpinBox *lIntervalSpinBox = new QSpinBox;
	lIntervalSpinBox->setObjectName(QStringLiteral("kcfg_Schedule interval"));
	lIntervalSpinBox->setMinimum(1);
	lIntervalLayout->addWidget(lIntervalSpinBox);
	KComboBox *lIntervalUnit = new KComboBox;
	lIntervalUnit->setObjectName(QStringLiteral("kcfg_Schedule interval unit"));
	lIntervalUnit->addItem(xi18nc("@item:inlistbox", "Minutes"));
	lIntervalUnit->addItem(xi18nc("@item:inlistbox", "Hours"));
	lIntervalUnit->addItem(xi18nc("@item:inlistbox", "Days"));
	lIntervalUnit->addItem(xi18nc("@item:inlistbox", "Weeks"));
	lIntervalLayout->addWidget(lIntervalUnit);
	lIntervalLayout->addStretch();
	lIntervalVertLayout->addLayout(lIntervalLayout, 1, 1);
	lIntervalWidget->setLayout(lIntervalVertLayout);

	QWidget *lUsageWidget = new QWidget;
	lUsageWidget->setVisible(false);
	connect(lUsageRadio, SIGNAL(toggled(bool)), lUsageWidget, SLOT(setVisible(bool)));
	QLabel *lUsageLabel = new QLabel(xi18nc("@info",
	                                        "New backup will be triggered when backup destination "
	                                        "becomes available and you have been using your "
	                                        "computer actively for more than the configured "
	                                        "time limit since the last backup was taken."));
	lUsageLabel->setWordWrap(true);
	QGridLayout *lUsageVertLayout = new QGridLayout;
	lUsageVertLayout->setColumnMinimumWidth(0, lIndentation);
	lUsageVertLayout->setContentsMargins(0, 0, 0, 0);
	lUsageVertLayout->addWidget(lUsageLabel, 0, 1);
	QHBoxLayout *lUsageLayout = new QHBoxLayout;
	lUsageLayout->setContentsMargins(0, 0, 0, 0);
	QSpinBox *lUsageSpinBox = new QSpinBox;
	lUsageSpinBox->setObjectName(QStringLiteral("kcfg_Usage limit"));
	lUsageSpinBox->setMinimum(1);
	lUsageLayout->addWidget(lUsageSpinBox);
	lUsageLayout->addWidget(new QLabel(xi18nc("@item:inlistbox", "Hours")));
	lUsageLayout->addStretch();
	lUsageVertLayout->addLayout(lUsageLayout, 1, 1);
	lUsageWidget->setLayout(lUsageVertLayout);

	QCheckBox *lAskFirstCheckBox = new QCheckBox(xi18nc("@option:check",
	                                                    "Ask for confirmation before taking backup"));
	lAskFirstCheckBox->setObjectName(QStringLiteral("kcfg_Ask first"));
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
	lPage->setName(xi18nc("@title", "Schedule"));
	lPage->setHeader(xi18nc("@label", "Specify the backup schedule"));
	lPage->setIcon(QIcon::fromTheme(QStringLiteral("view-calendar")));
	return lPage;
}

KPageWidgetItem *BackupPlanWidget::createAdvancedPage(bool pPar2Available) {
	QWidget *lAdvancedWidget = new QWidget(this);
	QVBoxLayout *lAdvancedLayout = new QVBoxLayout;

	int lIndentation = lAdvancedWidget->style()->pixelMetric(QStyle::PM_IndicatorWidth) +
	                   lAdvancedWidget->style()->pixelMetric(QStyle::PM_CheckBoxLabelSpacing);

	QCheckBox *lShowHiddenCheckBox = new QCheckBox(xi18nc("@option:check",
	                                                      "Show hidden folders in source selection"));
	lShowHiddenCheckBox->setObjectName(QStringLiteral("kcfg_Show hidden folders"));
	connect(lShowHiddenCheckBox, SIGNAL(toggled(bool)),
	        mSourceSelectionWidget, SLOT(setHiddenFoldersVisible(bool)));

	QLabel *lShowHiddenLabel = new QLabel(xi18nc("@info",
	                                             "This makes it possible to explicitly include or "
	                                             "exclude hidden folders in the backup source "
	                                             "selection. Hidden folders have a name that starts "
	                                             "with a dot. They are typically located in your home "
	                                             "folder and are used to store settings and temporary "
	                                             "files for your applications."));
	lShowHiddenLabel->setWordWrap(true);
	QGridLayout *lShowHiddenLayout = new QGridLayout;
	lShowHiddenLayout->setContentsMargins(0, 0, 0, 0);
	lShowHiddenLayout->setColumnMinimumWidth(0, lIndentation);
	lShowHiddenLayout->addWidget(lShowHiddenLabel, 0, 1);

	QWidget *lRecoveryWidget = new QWidget;
	lRecoveryWidget->setVisible(false);
	QCheckBox *lRecoveryCheckBox = new QCheckBox;
	lRecoveryCheckBox->setObjectName(QStringLiteral("kcfg_Generate recovery info"));

	QLabel *lRecoveryLabel = new QLabel(xi18nc("@info",
	                                           "This will make your backups use around 10% more storage "
	                                           "space and saving backups will take slightly longer time. In "
	                                           "return it will be possible to recover from a partially corrupted "
	                                           "backup."));
	lRecoveryLabel->setWordWrap(true);
	if(pPar2Available) {
		lRecoveryCheckBox->setText(xi18nc("@option:check", "Generate recovery information"));
	} else {
		lRecoveryCheckBox->setText(xi18nc("@option:check", "Generate recovery information (not available "
		                                                   "because <application>par2</application> is not installed)"));
		lRecoveryCheckBox->setEnabled(false);
		lRecoveryLabel->setEnabled(false);
	}
	QGridLayout *lRecoveryLayout = new QGridLayout;
	lRecoveryLayout->setContentsMargins(0, 0, 0, 0);
	lRecoveryLayout->setSpacing(0);
	lRecoveryLayout->setColumnMinimumWidth(0, lIndentation);
	lRecoveryLayout->addWidget(lRecoveryCheckBox,0, 0, 1, 2);
	lRecoveryLayout->addWidget(lRecoveryLabel, 1, 1);
	lRecoveryWidget->setLayout(lRecoveryLayout);
	connect(mVersionedRadio, SIGNAL(toggled(bool)), lRecoveryWidget, SLOT(setVisible(bool)));

	QWidget *lVerificationWidget = new QWidget;
	lVerificationWidget->setVisible(false);
	QCheckBox *lVerificationCheckBox = new QCheckBox(xi18nc("@option:check", "Verify integrity of backups"));
	lVerificationCheckBox->setObjectName(QStringLiteral("kcfg_Check backups"));

	QLabel *lVerificationLabel = new QLabel(xi18nc("@info",
	                                               "Checks the whole backup archive for corruption "
	                                               "every time you save new data. Saving backups will take a "
	                                               "little bit longer time but it allows you to catch corruption "
	                                               "problems sooner than at the time you need to use a backup, "
	                                               "at that time it could be too late."));
	lVerificationLabel->setWordWrap(true);
	QGridLayout *lVerificationLayout = new QGridLayout;
	lVerificationLayout->setContentsMargins(0, 0, 0, 0);
	lVerificationLayout->setSpacing(0);
	lVerificationLayout->setColumnMinimumWidth(0, lIndentation);
	lVerificationLayout->addWidget(lVerificationCheckBox,0, 0, 1, 2);
	lVerificationLayout->addWidget(lVerificationLabel, 1, 1);
	lVerificationWidget->setLayout(lVerificationLayout);
	connect(mVersionedRadio, SIGNAL(toggled(bool)), lVerificationWidget, SLOT(setVisible(bool)));

	QWidget *removalWidget = createRemovalGroup(lIndentation);
	removalWidget->setVisible(mResticRadio->isChecked());
	connect(mResticRadio, SIGNAL(toggled(bool)),
			removalWidget, SLOT(setVisible(bool)));

	lAdvancedLayout->addWidget(lShowHiddenCheckBox);
	lAdvancedLayout->addLayout(lShowHiddenLayout);
	lAdvancedLayout->addWidget(lVerificationWidget);
	lAdvancedLayout->addWidget(lRecoveryWidget);
	lAdvancedLayout->addWidget(removalWidget);
	lAdvancedLayout->addStretch();
	lAdvancedWidget->setLayout(lAdvancedLayout);

	KPageWidgetItem *lPage = new KPageWidgetItem(lAdvancedWidget);
	lPage->setName(xi18nc("@title", "Advanced"));
	lPage->setHeader(xi18nc("@label", "Extra options for advanced users"));
	lPage->setIcon(QIcon::fromTheme(QStringLiteral("preferences-other")));
	return lPage;
}

QWidget *BackupPlanWidget::createRemovalGroup(int pIndentation)
{
	// TODO: Remove this param if unneeded.
	Q_UNUSED(pIndentation);

	QGroupBox *lGroup = new QGroupBox;
	lGroup->setTitle(i18n("Removal options"));

	QLabel *lDescription = new QLabel(i18n("A versioned backup keeps older versions "
										   "of your files and stores also files that "
										   "were deleted. You can use these options "
										   "to prevent your backup from growing indefinitely."));
	lDescription->setWordWrap(true);

	// Keep last n.
	QCheckBox *lKeepLastNCb;
	QSpinBox *lKeepLastNSb;
	createRemoveRow(&lKeepLastNCb, "Always keep the last n snapshots:",
					&lKeepLastNSb, mResticRadio->isChecked(), 5,
					QStringLiteral("kcfg_Keep last n"),
					i18n("Never delete the last (more recent) n snapshots."));

	// Keep hourly.
	QCheckBox *lKeepHourlyCb;
	QSpinBox *lKeepHourlySb;
	createRemoveRow(&lKeepHourlyCb, "Keep hourly for last n hours:",
					&lKeepHourlySb, mResticRadio->isChecked(), 5,
					QStringLiteral("kcfg_Keep hourly"),
					i18n("For the last n hours in which a snapshot was "
						 "made, keep only the last snapshot for each hour."));

	// Keep daily.
	QCheckBox *lKeepDailyCb;
	QSpinBox *lKeepDailySb;
	createRemoveRow(&lKeepDailyCb, "Keep daily for last n days:",
					&lKeepDailySb, mResticRadio->isChecked(), 5,
					QStringLiteral("kcfg_Keep daily"),
					i18n("For the last n days in which a snapshot was "
						 "made, keep only the last snapshot for that day."));

	// Keep monthly.
	QCheckBox *lKeepMonthlyCb;
	QSpinBox *lKeepMonthlySb;
	createRemoveRow(&lKeepMonthlyCb, "Keep monthly for last n months:",
					&lKeepMonthlySb, mResticRadio->isChecked(), 5,
					QStringLiteral("kcfg_Keep monthly"),
					i18n("For the last n months in which a snapshot was "
						 "made, keep only the last snapshot for each month."));

	// Keep yearly.
	QCheckBox *lKeepYearlyCb;
	QSpinBox *lKeepYearlySb;
	createRemoveRow(&lKeepYearlyCb, "Keep yearly for the last n years:",
					&lKeepYearlySb, mResticRadio->isChecked(), 5,
					QStringLiteral("kcfg_Keep yearly"),
					i18n("For the last n years in which a snapshot was "
						 "made, keep only the last snapshot for each year."));

	// Keep within duration.
	QString lKeepWithinDurationTT = i18n("Keep <b>all</b> snapshots which "
										 "have been made within a specified "
										 "duration from the latest snapshot.");
	QCheckBox *lKeepWithinDuration = new QCheckBox(xi18nc("@option:check",
														  "Keep all snapshots within duration:"));
	lKeepWithinDuration->setObjectName(QStringLiteral("kcfg_Keep within duration"));
	lKeepWithinDuration->setToolTip(lKeepWithinDurationTT);

	mDurationYears = new QSpinBox;
	mDurationYears->setEnabled(mResticRadio->isChecked());
	mDurationYears->setValue(1);
	mDurationYears->setObjectName(QStringLiteral("kcfg_Keep within duration years"));
	mDurationYears->setToolTip(lKeepWithinDurationTT);
	connect(lKeepWithinDuration, SIGNAL(toggled(bool)),
			mDurationYears, SLOT(setEnabled(bool)));
	connect(mDurationYears, SIGNAL(valueChanged(int)),
			this, SLOT(refreshDuration()));

	mDurationMonths = new QSpinBox;
	mDurationMonths->setEnabled(mResticRadio->isChecked());
	mDurationMonths->setValue(0);
	mDurationMonths->setObjectName(QStringLiteral("kcfg_Keep within duration months"));
	mDurationMonths->setToolTip(lKeepWithinDurationTT);
	connect(lKeepWithinDuration, SIGNAL(toggled(bool)),
			mDurationMonths, SLOT(setEnabled(bool)));
	connect(mDurationMonths, SIGNAL(valueChanged(int)),
			this, SLOT(refreshDuration()));

	mDurationDays = new QSpinBox;
	mDurationDays->setEnabled(mResticRadio->isChecked());
	mDurationDays->setValue(0);
	mDurationDays->setObjectName(QStringLiteral("kcfg_Keep within duration days"));
	mDurationDays->setToolTip(lKeepWithinDurationTT);
	connect(lKeepWithinDuration, SIGNAL(toggled(bool)),
			mDurationDays, SLOT(setEnabled(bool)));
	connect(mDurationDays, SIGNAL(valueChanged(int)),
			this, SLOT(refreshDuration()));

	QHBoxLayout *lYearsLayout = new QHBoxLayout;
	lYearsLayout->addWidget(mDurationYears);
	lYearsLayout->addWidget(new QLabel(i18n("years")));

	QHBoxLayout *lMonthsLayout = new QHBoxLayout;
	lMonthsLayout->addWidget(mDurationMonths);
	lMonthsLayout->addWidget(new QLabel(i18n("months")));

	QHBoxLayout *lDaysLayout = new QHBoxLayout;
	lDaysLayout->addWidget(mDurationDays);
	lDaysLayout->addWidget(new QLabel(i18n("days")));

	// Build summary.
	mDurationSummary = new QLabel;
	mDurationSummary->setEnabled(mResticRadio->isChecked());
	connect(lKeepWithinDuration, SIGNAL(toggled(bool)),
			mDurationSummary, SLOT(setEnabled(bool)));

	refreshDuration();

	// Separator.
	QFrame* f = new QFrame;
	f->setFrameStyle(QFrame::Plain);
	f->setFrameShape(QFrame::HLine);

	QFormLayout *layout = new QFormLayout;
	layout->setContentsMargins(20, 20, 20, 20);
	layout->setSpacing(20);
	layout->addRow(lDescription);
	layout->addRow(f);
	layout->addRow(lKeepLastNCb, lKeepLastNSb);
	layout->addRow(lKeepHourlyCb, lKeepHourlySb);
	layout->addRow(lKeepDailyCb, lKeepDailySb);
	layout->addRow(lKeepWithinDuration, lYearsLayout);
	layout->addRow(new QWidget, lMonthsLayout);
	layout->addRow(new QWidget, lDaysLayout);
	layout->addRow(mDurationSummary);

	lGroup->setLayout(layout);

	return lGroup;
}

void BackupPlanWidget::createRemoveRow(QCheckBox **cb,
									   const char *label,
									   QSpinBox **sb,
									   bool cbValue,
									   int sbValue,
									   const QString &kcfgLabel,
									   const QString &tooltip)
{
	(*cb) = new QCheckBox(xi18nc("@option:check", label));
	(*cb)->setChecked(cbValue);
	(*cb)->setObjectName(kcfgLabel);
	(*cb)->setToolTip(tooltip);

	(*sb) = new QSpinBox;
	(*sb)->setValue(sbValue);
	(*sb)->setEnabled(false);
	(*sb)->setObjectName(QString("%1 value").arg(kcfgLabel));
	(*sb)->setToolTip(tooltip);
	connect(*cb, SIGNAL(toggled(bool)),
			*sb, SLOT(setEnabled(bool)));
}

void BackupPlanWidget::refreshDuration()
{
	// Build summary.
	QString summary;
	if (mDurationYears->value() > 0
			&& mDurationMonths->value() > 0
			&& mDurationDays->value() > 0)
		summary = i18n("All snapshots within %1 year(s), %2 month(s) and %3 day(s) will be preserved.",
					   mDurationYears->value(),
					   mDurationMonths->value(),
					   mDurationDays->value());
	else if (mDurationYears->value() > 0 && mDurationMonths->value() > 0)
		summary = i18n("All snapshots within %1 year(s) and %2 month(s) will be preserved.",
					   mDurationYears->value(),
					   mDurationMonths->value());
	else if (mDurationYears->value() > 0 && mDurationDays->value() > 0)
		summary = i18n("All snapshots within %1 year(s) and %2 day(s) will be preserved.",
					   mDurationYears->value(),
					   mDurationDays->value());
	else if (mDurationMonths->value() > 0 && mDurationDays->value() > 0)
		summary = i18n("All snapshots within %1 month(s) and %2 day(s) will be preserved.",
					   mDurationMonths->value(),
					   mDurationDays->value());
	else if (mDurationYears->value() > 0)
		summary = i18n("All snapshots within %1 year(s) will be preserved.",
					   mDurationYears->value());
	else if (mDurationMonths->value() > 0)
		summary = i18n("All snapshots within %1 month(s) will be preserved.",
					   mDurationMonths->value());
	else if (mDurationDays->value() > 0)
		summary = i18n("All snapshots within %1 day(s) will be preserved.",
					   mDurationDays->value());
	else
		summary = QString();

	mDurationSummary->setText(summary);
}

void BackupPlanWidget::openDriveDestDialog() {
	QString lMountPoint = mDriveSelection->mountPathOfSelectedDrive();
	QString lSelectedPath;
	DirDialog lDirDialog(QUrl::fromLocalFile(lMountPoint), mDriveDestEdit->text(), this);
	if(lDirDialog.exec() == QDialog::Accepted) {
		lSelectedPath = lDirDialog.url().path();
		lSelectedPath.remove(0, lMountPoint.length());
		while(lSelectedPath.startsWith(QLatin1Char('/'))) {
			lSelectedPath.remove(0, 1);
		}
		mDriveDestEdit->setText(lSelectedPath);
	}
}

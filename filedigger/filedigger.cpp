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

#include "filedigger.h"
#include "mergedvfsmodel.h"
#include "mergedvfsrestic.h"
#include "restoredialog.h"
#include "versionlistmodel.h"
#include "versionlistdelegate.h"
#include "fsminer.h"
#include "fsminerrestic.h"
#include "fsminerbup.h"
#include "kupfiledigger_debug.h"

#include <KDirOperator>
#include <KFilePlacesView>
#include <KFilePlacesModel>
#include <KGuiItem>
#include <KLocalizedString>
#include <KMessageBox>
#include <KRun>
#include <KStandardAction>
#include <KToolBar>

#include <QLabel>
#include <QListView>
#include <QMimeData>
#include <QPushButton>
#include <QSplitter>
#include <QTreeView>
#include <QVBoxLayout>
#include <QApplication>
#include <QClipboard>
#include <QTableWidget>
#include <QCloseEvent>
#include <QDesktopServices>

SnapshotListModel::SnapshotListModel(QObject *parent) : QAbstractListModel(parent)
{
    // Do nothing.
}

void SnapshotListModel::setSnapshots(const QList<BackupSnapshot> &snapshots)
{
    mSnapshots = snapshots;
    emit dataChanged(index(0, 0), index(snapshots.size() - 1, 0));
}

QVariant SnapshotListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    if (index.row() >= mSnapshots.size())
        return QVariant();
    if (role != Qt::DisplayRole)
        return QVariant();

    return mSnapshots[index.row()].mDateTime.toString();
}

int SnapshotListModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return mSnapshots.size();
}

BupMountLock::BupMountLock(const QString &repoPath, QObject *parent) :
    MountLock(ResticMountManager::generateMountPrivateDir(), parent)
{
    connect(&mProcess, &QProcess::started,
            this, &MountLock::servingData);

    QStringList args = QStringList() << QSL("-d")
                                     << repoPath << QSL("fuse") << mMountPath;
    mProcess.start(QSL("bup"), args);
}

bool BupMountLock::isLocked()
{
    return mProcess.state() != QProcess::NotRunning;
}

bool BupMountLock::unlock()
{
    bool ret = umountMountPoint();
    removeMountPoint();

    return ret;
}

FileDigger::FileDigger(BackupType type, const QString &pRepoPath, const QString &pBranchName, QWidget *pParent)
	: KMainWindow(pParent), mRepoPath(pRepoPath), mBranchName(pBranchName), mDirOperator(nullptr), mBackupType(type)
{
    setWindowIcon(QIcon::fromTheme(QStringLiteral("kup")));
    KToolBar *lAppToolBar = toolBar();
    lAppToolBar->addAction(KStandardAction::quit(this, SLOT(close()), this));

    repoPathAvailable();
}

void FileDigger::updateVersionModel(const QModelIndex &pCurrent, const QModelIndex &pPrevious) {
	Q_UNUSED(pPrevious)
	mVersionModel->setNode(mMergedVfsModel->node(pCurrent));
	mVersionView->selectionModel()->setCurrentIndex(mVersionModel->index(0,0),
	                                                QItemSelectionModel::Select);
}

void FileDigger::open(const QModelIndex &pIndex) {
	KRun::runUrl(pIndex.data(VersionUrlRole).value<QUrl>(),
	             pIndex.data(VersionMimeTypeRole).toString(), this);

}

void FileDigger::restore(const QModelIndex &pIndex) {
	if (mBackupType == BackupType::B_T_BUP) {
		RestoreDialog *lDialog = new RestoreDialog(mBackupType, pIndex.data(VersionSourceInfoRole).value<BupSourceInfo>(), this);
		lDialog->setAttribute(Qt::WA_DeleteOnClose);
		lDialog->show();
	}
	else {
		QMessageBox::warning(this, i18n("Not available"),
							 i18n("Sorry, the restore procedure is not "
								  "implemented yet for restic. Use the copy button instead."));
	}
}

void FileDigger::copy(const QModelIndex &pIndex)
{
	QUrl url = pIndex.data(VersionUrlRole).value<QUrl>();
	if (url.isEmpty()) {
		QMessageBox::warning(this, i18n("Cannot find source file"),
							 i18n("An unknown problem occurred while extracting data."));
		return;
	}

	QMimeData* mime = new QMimeData;
	mime->setData(QStringLiteral("text/uri-list"), url.toString().toUtf8());
	QApplication::clipboard()->setMimeData(mime);
}

void FileDigger::repoPathAvailable() {
	if(mRepoPath.isEmpty()) {
		createSelectionView();
	} else {
        createRepoView();
	}
}

void FileDigger::checkFileWidgetPath() {
	KFileItemList lList = mDirOperator->selectedItems();
	if(lList.isEmpty()) {
		mRepoPath = mDirOperator->url().toLocalFile();
	} else {
		mRepoPath = lList.first().url().toLocalFile();
	}
	mBranchName = QStringLiteral("kup");
	repoPathAvailable();
}

void FileDigger::enterUrl(QUrl pUrl) {
    mDirOperator->setUrl(pUrl, true);
}

void FileDigger::setInfo(QString info)
{
    mInfoLabel->setText(info);
}

void FileDigger::onBackupDescriptorReceived(BackupDescriptor descriptor)
{
    qCDebug(KUPFILEDIGGER) << "Received descriptor";
    setInfo(i18n("Your backup was opened for reading. "
                 "Keep this window open while you need it.\n"
                 "Closing this window will also close the backup."));
    //setBackupOpen(true);

    mDescriptor = descriptor;
    mSnapshotModel.setSnapshots(descriptor.mSnapshots);

    mMergedVfsModel = new MergedVfsModel(createRepo(), this);
    mMergedVfsView->setModel(mMergedVfsModel);
    connect(mMergedVfsView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),
            this, SLOT(updateVersionModel(QModelIndex,QModelIndex)));

    //expand all levels from the top until the node has more than one child
    QModelIndex lIndex;
    forever {
        mMergedVfsView->expand(lIndex);
        if(mMergedVfsModel->rowCount(lIndex) == 1) {
            lIndex = mMergedVfsModel->index(0, 0, lIndex);
        } else {
            break;
        }
    }
    mMergedVfsView->selectionModel()->setCurrentIndex(lIndex.child(0,0), QItemSelectionModel::Select);

    VersionListDelegate *lVersionDelegate = new VersionListDelegate(mBackupType, mVersionView, this);
    mVersionView->setItemDelegate(lVersionDelegate);
    connect(lVersionDelegate, SIGNAL(openRequested(QModelIndex)), SLOT(open(QModelIndex)));
    connect(lVersionDelegate, SIGNAL(restoreRequested(QModelIndex)), SLOT(restore(QModelIndex)));
    connect(lVersionDelegate, SIGNAL(copyRequested(QModelIndex)), SLOT(copy(QModelIndex)));
    mMergedVfsView->setFocus();

    if (descriptor.mSnapshots.isEmpty()) {
        m_tab->setTabEnabled(0, mBackupType == B_T_BUP);
        m_tab->setTabEnabled(1, false);
    }
    else {
        m_tab->setTabEnabled(0, true);
        m_tab->setTabEnabled(1, true);
    }
}

void FileDigger::requestMount()
{
    if (mBackupType == B_T_RESTIC) {
        mMountLock.reset(ResticMountManager::instance().mount(mRepoPath));
        if (!mMountLock) {
            setInfo(i18n("Failed to open backup"));
            return;
        }

        connect(mMountLock.data(), &ResticMountLock::servingData, [this]() {
            setInfo(i18n("Reading backup data..."));
            mFsMiner.reset(new FsMinerRestic(mMountLock->mountPath()));
            connect(mFsMiner.data(), SIGNAL(backupProcessed(BackupDescriptor)),
                    this, SLOT(onBackupDescriptorReceived(BackupDescriptor)));
            mFsMiner->process();
        });
    }
    else {
        mMountLock.reset(new BupMountLock(mRepoPath));

        connect(mMountLock.data(), &BupMountLock::servingData, [this]() {
            setInfo(i18n("Reading backup data..."));
            mFsMiner.reset(new FsMinerBup(mMountLock->mountPath()));
            connect(mFsMiner.data(), SIGNAL(backupProcessed(BackupDescriptor)),
                    this, SLOT(onBackupDescriptorReceived(BackupDescriptor)));
            mFsMiner->process();
        });
    }
}

void FileDigger::closeEvent(QCloseEvent *event)
{
    qWarning() << "closing";

    if (!mMountLock)
        return;

    qWarning() << "Unlocking";
    if (!mMountLock->unlock()) {
        event->ignore();

        QMessageBox::warning(this,
                             i18n("Failed to close backup"),
                             i18n("It was not possible to close the backup. "
                                  "Before trying to close the backup, please ensure "
                                  "there is no application trying to use its content. "
                                  "If you are sure no application is trying to use the "
                                  "content of the backup, please try again in a few minutes."));

        return;
    }

    qWarning() << "reset";
    mMountLock.reset(nullptr);
}

MergedRepository *FileDigger::createRepo() {
	MergedRepository* lRepository;
	if (mBackupType == BackupType::B_T_BUP)
		lRepository = new MergedRepositoryBup(nullptr, mRepoPath, mBranchName);
    else
        lRepository = new MergedRepositoryResitc(nullptr, mMountLock->mountPath());
    
	if(!lRepository->open()) {
        KMessageBox::sorry(nullptr, xi18nc("@info messagebox, %1 is a folder path",
                                       "The backup archive <filename>%1</filename> could not be opened."
                                       "Check if the backups really are located there.",
                                       mRepoPath));
        return nullptr;
    }
    
    if(!lRepository->readBranch()) {
        if(!lRepository->permissionsOk()) {
            KMessageBox::sorry(nullptr, xi18nc("@info messagebox",
                                           "You do not have permission needed to read this backup archive."));
        } else {
			lRepository->rootNode()->askForIntegrityCheck();
        }
        return nullptr;
    }
    
    return lRepository;
}

void FileDigger::createRepoView() {
    mInfoLabel = new QLabel;
    mInfoLabel->setAlignment(Qt::AlignCenter);

    mSnapshotView = new QListView;
    mSnapshotView->setModel(&mSnapshotModel);
    connect(mSnapshotView, &QListView::doubleClicked, [this](const QModelIndex &index) {
        if (mDescriptor.mSnapshots.size() <= index.row())
            return;
        QUrl url = QUrl::fromLocalFile(mDescriptor.mSnapshots[index.row()].mAbsPath);
        QDesktopServices::openUrl(url);
    });

    QSplitter *lSplitter = new QSplitter();
    //mMergedVfsModel = new MergedVfsModel(pRepository, this);
    mMergedVfsView = new QTreeView();
    mMergedVfsView->setHeaderHidden(true);
    mMergedVfsView->setSelectionMode(QAbstractItemView::SingleSelection);
    lSplitter->addWidget(mMergedVfsView);

	mVersionView = new QListView();
	mVersionView->setSelectionMode(QAbstractItemView::SingleSelection);
	mVersionModel = new VersionListModel(this);
    mVersionView->setModel(mVersionModel);
    lSplitter->addWidget(mVersionView);

    m_tab = new QTabWidget;
    m_tab->addTab(lSplitter, i18n("File digger"));
    m_tab->addTab(mSnapshotView, i18n("Snapshots"));
    m_tab->setTabEnabled(0, mBackupType == B_T_BUP);
    m_tab->setTabEnabled(1, false);

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addWidget(mInfoLabel);
    mainLayout->addWidget(m_tab);

    QWidget* mainWidget = new QWidget;
    mainWidget->setLayout(mainLayout);

    setCentralWidget(mainWidget);

    requestMount();
}

void FileDigger::createSelectionView() {
	if(mDirOperator != nullptr) {
		return;
	}
	QLabel *lLabel = new QLabel(i18n("Select location of backup archive to open."));

	KFilePlacesView *lPlaces = new KFilePlacesView;
	lPlaces->setModel(new KFilePlacesModel);

	mDirOperator = new KDirOperator();
	mDirOperator->setView(KFile::Tree);
	mDirOperator->setMode(KFile::Directory);
	mDirOperator->setEnableDirHighlighting(true);
	mDirOperator->setShowHiddenFiles(true);

	connect(lPlaces, &KFilePlacesView::urlChanged, this, &FileDigger::enterUrl);

	QPushButton *lOkButton = new QPushButton(this);
	KGuiItem::assign(lOkButton, KStandardGuiItem::ok());
	connect(lOkButton, &QPushButton::pressed, this, &FileDigger::checkFileWidgetPath);


	QWidget *lSelectionView = new QWidget;
	QVBoxLayout *lVLayout1 = new QVBoxLayout;
	QSplitter *lSplitter = new QSplitter;

	lVLayout1->addWidget(lLabel);
	lSplitter->addWidget(lPlaces);
	lSplitter->addWidget(mDirOperator);
	lVLayout1->addWidget(lSplitter, 1);
	lVLayout1->addWidget(lOkButton);
	lSelectionView->setLayout(lVLayout1);
	setCentralWidget(lSelectionView);
}

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
#include "restoredialog.h"
#include "versionlistmodel.h"
#include "versionlistdelegate.h"

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
#include <QPushButton>
#include <QSplitter>
#include <QTreeView>
#include <QVBoxLayout>

FileDigger::FileDigger(const QString &pRepoPath, const QString &pBranchName, QWidget *pParent)
    : KMainWindow(pParent), mRepoPath(pRepoPath), mBranchName(pBranchName), mDirOperator(nullptr)
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
	KRun::runUrl(pIndex.data(VersionBupUrlRole).value<QUrl>(),
	             pIndex.data(VersionMimeTypeRole).toString(), this);

}

void FileDigger::restore(const QModelIndex &pIndex) {
	RestoreDialog *lDialog = new RestoreDialog(pIndex.data(VersionSourceInfoRole).value<BupSourceInfo>(), this);
	lDialog->setAttribute(Qt::WA_DeleteOnClose);
	lDialog->show();
}

void FileDigger::repoPathAvailable() {
	if(mRepoPath.isEmpty()) {
		createSelectionView();
	} else {
		MergedRepository *lRepository = createRepo();
		if(lRepository != nullptr) {
			createRepoView(lRepository);
		}
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

MergedRepository *FileDigger::createRepo() {
    MergedRepository *lRepository = new MergedRepository(nullptr, mRepoPath, mBranchName);
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
            lRepository->askForIntegrityCheck();
        }
        return nullptr;
    }
    return lRepository;
}

void FileDigger::createRepoView(MergedRepository *pRepository) {
    QSplitter *lSplitter = new QSplitter();
    mMergedVfsModel = new MergedVfsModel(pRepository, this);
    mMergedVfsView = new QTreeView();
    mMergedVfsView->setHeaderHidden(true);
    mMergedVfsView->setSelectionMode(QAbstractItemView::SingleSelection);
    mMergedVfsView->setModel(mMergedVfsModel);
    lSplitter->addWidget(mMergedVfsView);
    connect(mMergedVfsView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),
            this, SLOT(updateVersionModel(QModelIndex,QModelIndex)));

	mVersionView = new QListView();
	mVersionView->setSelectionMode(QAbstractItemView::SingleSelection);
	mVersionModel = new VersionListModel(this);
	mVersionView->setModel(mVersionModel);
	VersionListDelegate *lVersionDelegate = new VersionListDelegate(mVersionView,this);
	mVersionView->setItemDelegate(lVersionDelegate);
	lSplitter->addWidget(mVersionView);
	connect(lVersionDelegate, SIGNAL(openRequested(QModelIndex)), SLOT(open(QModelIndex)));
	connect(lVersionDelegate, SIGNAL(restoreRequested(QModelIndex)), SLOT(restore(QModelIndex)));
	mMergedVfsView->setFocus();

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
	setCentralWidget(lSplitter);
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

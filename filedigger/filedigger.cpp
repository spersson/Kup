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

#include <KAction>
#include <KFileDialog>
#include <KRun>
#include <KStandardAction>
#include <KToolBar>
#include <QListView>
#include <QSplitter>
#include <QTreeView>


FileDigger::FileDigger(MergedRepository *pRepository, QWidget *pParent)
   : KMainWindow(pParent)
{
	setWindowIcon(KIcon(QLatin1String("chronometer")));
	KToolBar *lAppToolBar = toolBar();
	lAppToolBar->addAction(KStandardAction::quit(this, SLOT(close()), this));
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

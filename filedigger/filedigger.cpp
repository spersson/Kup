#include "filedigger.h"
#include "mergedvfsmodel.h"
#include "versionlistmodel.h"
#include "versionlistdelegate.h"

#include <KLocale>
#include <KRun>
#include <QListView>
#include <QTreeView>

FileDigger::FileDigger(MergedRepository *pRepository, QWidget *pParent)
   : QSplitter(pParent)
{
	mMergedVfsModel = new MergedVfsModel(pRepository, this);
	mMergedVfsView = new QTreeView();
	mMergedVfsView->setHeaderHidden(true);
	mMergedVfsView->setSelectionMode(QAbstractItemView::SingleSelection);
	mMergedVfsView->setModel(mMergedVfsModel);
	addWidget(mMergedVfsView);
	connect(mMergedVfsView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),
	        this, SLOT(updateVersionModel(QModelIndex,QModelIndex)));

	mVersionView = new QListView();
	mVersionView->setSelectionMode(QAbstractItemView::SingleSelection);
	mVersionModel = new VersionListModel(this);
	mVersionView->setModel(mVersionModel);
	VersionListDelegate *lVersionDelegate = new VersionListDelegate(mVersionView,this);
	mVersionView->setItemDelegate(lVersionDelegate);
	addWidget(mVersionView);
	connect(lVersionDelegate, SIGNAL(updateRequested(QModelIndex)),
	        mVersionView, SLOT(update(QModelIndex)));
	connect(lVersionDelegate, SIGNAL(openRequested(int)), SLOT(open(int)));
	connect(lVersionDelegate, SIGNAL(restoreRequested(int)), SLOT(restore(int)));
	mMergedVfsView->setFocus();
}

void FileDigger::updateVersionModel(const QModelIndex &pCurrent, const QModelIndex &pPrevious) {
	Q_UNUSED(pPrevious)
	mVersionModel->setNode(mMergedVfsModel->node(pCurrent));
}

void FileDigger::open(int pRow) {
	QModelIndex lIndex = mVersionModel->index(pRow, 0);
	KRun::runUrl(lIndex.data(VersionBupUrlRole).value<KUrl>(),
	             lIndex.data(VersionMimeTypeRole).toString(), this);

}

void FileDigger::restore(int pRow) {
}


#include "filedigger.h"
#include "mergedvfsmodel.h"
#include "versionlistmodel.h"

#include <QListView>
#include <QTreeView>
#include <KLocale>
#include <KRun>

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

	mListView = new QListView();
	mListView->setSelectionMode(QAbstractItemView::SingleSelection);
	mVersionModel = new VersionListModel(this);
	mListView->setModel(mVersionModel);
	addWidget(mListView);
	connect(mListView, SIGNAL(activated(QModelIndex)), SLOT(open(QModelIndex)));

	mMergedVfsView->setFocus();
}

void FileDigger::updateVersionModel(const QModelIndex &pCurrent, const QModelIndex &pPrevious) {
	Q_UNUSED(pPrevious)
	mVersionModel->setNode(mMergedVfsModel->node(pCurrent));
}

void FileDigger::open(const QModelIndex &pIndex) {
	KRun::runUrl(pIndex.data(VersionBupUrlRole).value<KUrl>(), pIndex.data(VersionMimeTypeRole).toString(), this);
}

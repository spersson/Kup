#include "filedigger.h"
#include "mergedvfsmodel.h"
#include "versionlistmodel.h"

#include <QListView>
#include <QTreeView>
#include <KLocale>
#include <KMessageBox>
#include <KRun>

FileDigger::FileDigger(const QString &pRepositoryPath, const QString &pBranchName, QWidget *parent) :
   QSplitter(parent)
{
	mTreeView = new QTreeView();
	mTreeView->setHeaderHidden(true);
	mTreeView->setSelectionMode(QAbstractItemView::SingleSelection);
	MergedRepository *lRepository = new MergedRepository(this, pRepositoryPath, pBranchName);
	if(!lRepository->openedSuccessfully()) {
		KMessageBox::sorry(this, i18nc("@info:label messagebox", "The specified bup repository could not be opened."));
		return;
	}
	mTreeModel = new MergedVfsModel(lRepository, this);
	mTreeView->setModel(mTreeModel);
	addWidget(mTreeView);
	connect(mTreeView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),
	        this, SLOT(updateVersionModel(QModelIndex,QModelIndex)));

	mListView = new QListView();
	mListView->setSelectionMode(QAbstractItemView::SingleSelection);
	mVersionModel = new VersionListModel(this);
	mListView->setModel(mVersionModel);
	addWidget(mListView);
	connect(mListView, SIGNAL(activated(QModelIndex)), SLOT(open(QModelIndex)));

	mTreeView->setFocus();
}

void FileDigger::updateVersionModel(const QModelIndex &pCurrent, const QModelIndex &pPrevious) {
	Q_UNUSED(pPrevious)
	mVersionModel->setNode(mTreeModel->node(pCurrent));
}

void FileDigger::open(const QModelIndex &pIndex) {
	KRun::runUrl(pIndex.data(VersionBupUrlRole).value<KUrl>(), pIndex.data(VersionMimeTypeRole).toString(), this);
}

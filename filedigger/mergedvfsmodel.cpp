#include "mergedvfsmodel.h"
#include "mergedvfs.h"
#include "../kioslave/vfshelpers.h"

#include <KMimeType>
#include <KIconLoader>
#include <QPixmap>

MergedVfsModel::MergedVfsModel(MergedRepository *pRoot, QObject *pParent) :
   QAbstractItemModel(pParent), mRoot(pRoot)
{
}

int MergedVfsModel::columnCount(const QModelIndex &pParent) const {
	Q_UNUSED(pParent)
	return 1;
}

QVariant MergedVfsModel::data(const QModelIndex &pIndex, int pRole) const {
	if(!pIndex.isValid()) {
		return QVariant();
	}
	MergedNode *lNode = static_cast<MergedNode *>(pIndex.internalPointer());
	switch (pRole) {
	case Qt::DisplayRole:
		return lNode->objectName();
	case Qt::DecorationRole:
			return KIconLoader::global()->loadMimeTypeIcon(
			         KMimeType::iconNameForUrl(lNode->objectName(), lNode->mode()),
			         KIconLoader::Small);
	default:
		return QVariant();
	}
}

QModelIndex MergedVfsModel::index(int pRow, int pColumn, const QModelIndex &pParent) const {
	if(pColumn != 0 || pRow < 0) {
		return QModelIndex(); // invalid
	}
	if(!pParent.isValid()) {
		if(pRow >= mRoot->subNodes().count()) {
			return QModelIndex(); // invalid
		}
		return createIndex(pRow, 0, mRoot->subNodes().at(pRow));
	}
	MergedNode *lParentNode = static_cast<MergedNode *>(pParent.internalPointer());
	if(pRow >= lParentNode->subNodes().count()) {
		return QModelIndex(); // invalid
	}
	return createIndex(pRow, 0, lParentNode->subNodes().at(pRow));
}

QModelIndex MergedVfsModel::parent(const QModelIndex &pChild) const {
	if(!pChild.isValid()) {
		return QModelIndex();
	}
	MergedNode *lChild = static_cast<MergedNode *>(pChild.internalPointer());
	MergedNode *lParent = qobject_cast<MergedNode *>(lChild->parent());
	if(lParent == NULL || lParent == mRoot) {
		return QModelIndex(); //invalid
	}
	MergedNode *lGrandParent = qobject_cast<MergedNode *>(lParent->parent());
	if(lGrandParent == NULL) {
		return QModelIndex(); //invalid
	}
	return createIndex(lGrandParent->subNodes().indexOf(lParent), 0, lParent);
}

int MergedVfsModel::rowCount(const QModelIndex &pParent) const {
	if(!pParent.isValid()) {
		return mRoot->subNodes().count();
	}
	MergedNode *lParent = static_cast<MergedNode *>(pParent.internalPointer());
	if(lParent == NULL) {
		return 0;
	}
	return lParent->subNodes().count();
}

const VersionList *MergedVfsModel::versionList(const QModelIndex &pIndex) {
	MergedNode *lNode = static_cast<MergedNode *>(pIndex.internalPointer());
	return lNode->versionList();
}

const MergedNode *MergedVfsModel::node(const QModelIndex &pIndex) {
	return static_cast<MergedNode *>(pIndex.internalPointer());
}


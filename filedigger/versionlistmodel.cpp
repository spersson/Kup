#include "versionlistmodel.h"
#include "../kioslave/vfshelpers.h"

#include <KMimeType>

VersionListModel::VersionListModel(QObject *parent) :
   QAbstractListModel(parent)
{
	mVersionList = NULL;
}

void VersionListModel::setNode(const MergedNode *pNode) {
	beginResetModel();
	mNode = pNode;
	mVersionList = mNode->versionList();
	endResetModel();
}

int VersionListModel::rowCount(const QModelIndex &pParent) const {
	Q_UNUSED(pParent)
	if(mVersionList != NULL) {
		return mVersionList->count();
	}
	return 0;
}

QVariant VersionListModel::data(const QModelIndex &pIndex, int pRole) const {
	if(!pIndex.isValid() || mVersionList == NULL) {
		return QVariant();
	}

	VersionData *lData = mVersionList->at(pIndex.row());
	switch (pRole) {
	case Qt::DisplayRole:
		return vfsTimeToString(lData->mModifiedDate);
	case VersionBupUrlRole:
		return mNode->getBupUrl(pIndex.row());
	case VersionMimeTypeRole:
		return KMimeType::findByUrl(mNode->objectName(), mNode->mode())->name();
	default:
		return QVariant();
	}
}

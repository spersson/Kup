#ifndef MERGEDVFSMODEL_H
#define MERGEDVFSMODEL_H

#include <QAbstractItemModel>

#include "mergedvfs.h"

class MergedVfsModel : public QAbstractItemModel
{
	Q_OBJECT
public:
	explicit MergedVfsModel(MergedRepository *pRoot, QObject *pParent = 0);
	int columnCount(const QModelIndex &pParent) const;
	QVariant data(const QModelIndex &pIndex, int pRole) const;
	QModelIndex index(int pRow, int pColumn, const QModelIndex &pParent) const;
	QModelIndex parent(const QModelIndex &pChild) const;
	int rowCount(const QModelIndex &pParent) const;

	const VersionList *versionList(const QModelIndex &pIndex);
	const MergedNode *node(const QModelIndex &pIndex);

protected:
	MergedRepository *mRoot;

};

#endif // MERGEDVFSMODEL_H

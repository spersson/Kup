#ifndef VERSIONLISTMODEL_H
#define VERSIONLISTMODEL_H

#include <QAbstractListModel>
#include "mergedvfs.h"

class VersionListModel : public QAbstractListModel
{
	Q_OBJECT
public:
	explicit VersionListModel(QObject *parent = 0);
	void setNode(const MergedNode *pNode);
	int rowCount(const QModelIndex &pParent) const;
	QVariant data(const QModelIndex &pIndex, int pRole) const;

protected:
	const VersionList *mVersionList;
	const MergedNode *mNode;
};

enum VersionDataRole {
	VersionBupUrlRole = Qt::UserRole + 1,
	VersionMimeTypeRole
};

#endif // VERSIONLISTMODEL_H

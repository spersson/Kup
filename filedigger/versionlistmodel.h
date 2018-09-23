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

#ifndef VERSIONLISTMODEL_H
#define VERSIONLISTMODEL_H

#include <QAbstractListModel>
#include "mergedvfs.h"

struct BupSourceInfo {
	QUrl mBupKioPath;
	QString mRepoPath;
	QString mBranchName;
	QString mPathInRepo;
	quint64 mCommitTime;
	quint64 mSize;
	bool mIsDirectory;
};

Q_DECLARE_METATYPE(BupSourceInfo)

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
	VersionUrlRole = Qt::UserRole + 1, // QUrl
	VersionMimeTypeRole, // QString
	VersionSizeRole, // quint64
	VersionSourceInfoRole, // PathInfo
	VersionIsDirectoryRole // bool
};

#endif // VERSIONLISTMODEL_H

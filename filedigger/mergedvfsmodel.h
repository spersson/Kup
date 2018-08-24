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

#ifndef MERGEDVFSMODEL_H
#define MERGEDVFSMODEL_H

#include <QAbstractItemModel>

#include "mergedvfs.h"

class MergedVfsModel : public QAbstractItemModel
{
	Q_OBJECT
public:
	explicit MergedVfsModel(MergedRepository *pRoot, QObject *pParent = 0);
	~MergedVfsModel();
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

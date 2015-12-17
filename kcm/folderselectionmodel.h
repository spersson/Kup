/* This file is part of the KDE Project
   Copyright (c) 2008 Sebastian Trueg <trueg@kde.org>

   Based on CollectionSetup.h from the Amarok project
   (C) 2003 Scott Wheeler <wheeler@kde.org>
   (C) 2004 Max Howell <max.howell@methylblue.com>
   (C) 2004 Mark Kretschmann <markey@web.de>
   (C) 2008 Seb Ruiz <ruiz@kde.org>
   (C) 2008-2009 Sebastian Trueg <trueg@kde.org>
   (C) 2012 Simon Persson <simonpersson1@gmail.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef _FOLDER_SELECTION_MODEL_H_
#define _FOLDER_SELECTION_MODEL_H_

#include <QFileSystemModel>
#include <QSet>


class FolderSelectionModel : public QFileSystemModel
{
	Q_OBJECT

public:
	FolderSelectionModel(bool pHiddenFoldersVisible = false, QObject *pParent = 0);
	virtual ~FolderSelectionModel();

	enum InclusionState {
		StateNone,
		StateIncluded,
		StateExcluded,
		StateIncludeInherited,
		StateExcludeInherited
	};

	enum CustomRoles {
		IncludeStateRole = 7777
	};

	Qt::ItemFlags flags(const QModelIndex &pIndex) const Q_DECL_OVERRIDE;
	QVariant data(const QModelIndex& pIndex, int pRole = Qt::DisplayRole) const Q_DECL_OVERRIDE;
	bool setData(const QModelIndex& pIndex, const QVariant& pValue, int pRole = Qt::EditRole) Q_DECL_OVERRIDE;

	void setFolders(const QStringList& pIncludedFolders, const QStringList& pExcludedFolders);
	QStringList includedFolders() const;
	QStringList excludedFolders() const;

	/**
	* Include the specified path. All subdirs will be reset.
	*/
	void includePath(const QString &pPath);

	/**
	* Exclude the specified path. All subdirs will be reset.
	*/
	void excludePath(const QString &pPath);

	int columnCount(const QModelIndex&) const Q_DECL_OVERRIDE { return 1; }

	InclusionState inclusionState(const QModelIndex &pIndex) const;
	InclusionState inclusionState(const QString &pPath) const;

	bool hiddenFoldersVisible() const;

public Q_SLOTS:
	void setHiddenFoldersVisible(bool pVisible);

signals:
	void includedPathsChanged();
	void excludedPathsChanged();

private:
	QSet<QString> mIncludedFolderList;
	QSet<QString> mExcludedFolderList;
};

#endif

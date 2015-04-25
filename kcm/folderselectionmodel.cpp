/* This file is part of the KDE Project
   Copyright (c) 2008 Sebastian Trueg <trueg@kde.org>

   Based on CollectionSetup.cpp from the Amarok project
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

#include "folderselectionmodel.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtGui/QColor>
#include <QtGui/QBrush>
#include <QtGui/QPalette>

#include <KDebug>
#include <KIcon>
#include <KLocalizedString>

namespace {
void removeSubDirs( const QString& path, QSet<QString>& set ) {
	QSet<QString>::iterator it = set.begin();
	while( it != set.end() ) {
		if( it->startsWith( path ) )
			it = set.erase( it );
		else
			++it;
	}
}

QModelIndex findLastLeaf( const QModelIndex& index, FolderSelectionModel* model ) {
	int rows = model->rowCount( index );
	if( rows > 0 ) {
		return findLastLeaf( model->index( rows-1, 0, index ), model );
	}
	else {
		return index;
	}
}

// we need the trailing slash to be able to use the startsWith() function to check for parent dirs.
QString ensureTrailingSlash(const QString &pPath) {
	return pPath.endsWith( QDir::separator() ) ? pPath : pPath + QDir::separator();
}

bool isForbiddenPath(const QString& pPath) {
	const QString lPathWithSlash = ensureTrailingSlash(pPath);
	QFileInfo lFileInfo(lPathWithSlash);
	return (lPathWithSlash.startsWith(QLatin1String("/proc/")) ||
	        lPathWithSlash.startsWith(QLatin1String("/dev/")) ||
	        lPathWithSlash.startsWith(QLatin1String("/sys/")) ||
	        !lFileInfo.isReadable() ||
	        !lFileInfo.isExecutable()
	        );
}

bool setContainsSubdir(QSet<QString> pSet, const QString &pParentDir) {
	const QString lPathWithSlash = ensureTrailingSlash(pParentDir);
	foreach(QString lTestedPath, pSet) {
		if(lTestedPath.startsWith(lPathWithSlash)) {
			return true;
		}
	}
	return false;
}

}


FolderSelectionModel::FolderSelectionModel(bool pHiddenFoldersVisible, QObject *pParent)
   : QFileSystemModel(pParent)
{
	setHiddenFoldersVisible(pHiddenFoldersVisible);
}

FolderSelectionModel::~FolderSelectionModel() {
}

void FolderSelectionModel::setHiddenFoldersVisible(bool pVisible){
	if(pVisible) {
		setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Hidden);
	} else {
		setFilter(QDir::AllDirs | QDir::NoDotAndDotDot);
	}
}

Qt::ItemFlags FolderSelectionModel::flags(const QModelIndex &pIndex) const {
	Qt::ItemFlags lFlags = QFileSystemModel::flags(pIndex);
	lFlags |= Qt::ItemIsUserCheckable;
	if(isForbiddenPath(filePath(pIndex))) {
		lFlags ^= Qt::ItemIsEnabled; //disabled!
	}
	return lFlags;
}

QVariant FolderSelectionModel::data(const QModelIndex& pIndex, int pRole) const {
	if(!pIndex.isValid() || pIndex.column() != 0) {
		return QFileSystemModel::data(pIndex, pRole);
	}
	const QString lPath = filePath(pIndex);
	const InclusionState lState = inclusionState(lPath);
	switch(pRole) {
	case Qt::CheckStateRole: {
		switch(lState) {
		case StateIncluded:
		case StateIncludeInherited:
			if(setContainsSubdir(mExcludedFolderList, lPath)) {
				return Qt::PartiallyChecked;
			}
			return Qt::Checked;
		default:
			return Qt::Unchecked;
		}
	}
	case IncludeStateRole:
		return inclusionState(pIndex);
	case Qt::ForegroundRole: {
		switch(lState) {
		case StateIncluded:
		case StateIncludeInherited:
			return QVariant::fromValue(QPalette().brush(QPalette::Active, QPalette::Text));
		default:
			if(setContainsSubdir(mIncludedFolderList, lPath)) {
				return QVariant::fromValue(QPalette().brush( QPalette::Active, QPalette::Text));
			}
			return QVariant::fromValue(QPalette().brush( QPalette::Disabled, QPalette::Text));
		}
	}
	case Qt::ToolTipRole: {
		switch(lState) {
		case StateIncluded:
		case StateIncludeInherited:
			if(setContainsSubdir(mExcludedFolderList, lPath)) {
				return i18nc("@info:tooltip %1 is the path of the folder in a listview",
				             "<filename>%1</filename><nl/>will be included in the backup, except "
				             "for unchecked subfolders", filePath(pIndex));
			}
			return i18nc("@info:tooltip %1 is the path of the folder in a listview",
			             "<filename>%1</filename><nl/>will be included in the backup", filePath(pIndex));
		default:
			if(setContainsSubdir(mIncludedFolderList, lPath)) {
				return i18nc("@info:tooltip %1 is the path of the folder in a listview",
				             "<filename>%1</filename><nl/> will <emphasis>not</emphasis> be included "
				             "in the backup but contains folders that will", filePath(pIndex));
			}
			return i18nc("@info:tooltip %1 is the path of the folder in a listview",
			             "<filename>%1</filename><nl/> will <emphasis>not</emphasis> be included "
			             "in the backup", filePath(pIndex));
		}
	}
	case Qt::DecorationRole:
		if(lPath == QDir::homePath()) {
			return KIcon(QLatin1String("user-home"));
		}
		break;
	}

	return QFileSystemModel::data(pIndex, pRole);
}

bool FolderSelectionModel::setData(const QModelIndex& pIndex, const QVariant& pValue, int pRole) {
	if(!pIndex.isValid() || pIndex.column() != 0 || pRole != Qt::CheckStateRole) {
		return QFileSystemModel::setData(pIndex, pValue, pRole);
	}

	// here we ignore the check value, we treat it as a toggle
	// This is due to our using the Qt checking system in a virtual way
	const QString lPath = filePath(pIndex);
	const InclusionState lState = inclusionState(lPath);
	switch(lState) {
	case StateIncluded:
	case StateIncludeInherited:
		excludePath(lPath);
		break;
	default:
		includePath(lPath);
	}
	QModelIndex lRecurseIndex = pIndex;
	while(lRecurseIndex.isValid()) {
		emit dataChanged(lRecurseIndex, lRecurseIndex);
		lRecurseIndex = lRecurseIndex.parent();
	}
	return true;
}

void FolderSelectionModel::includePath(const QString &pPath) {
	if(mIncludedFolderList.contains(pPath)) {
		return;
	}
	const InclusionState lState = inclusionState(pPath);
	removeSubDirs(pPath, mIncludedFolderList);
	removeSubDirs(pPath, mExcludedFolderList);
	if(lState == StateExcluded) {
		emit excludedPathsChanged();
	} else if(lState != StateIncludeInherited) {
		mIncludedFolderList.insert(pPath);
		emit includedPathsChanged();
	}
	emit dataChanged(index(pPath), findLastLeaf(index(pPath), this));
}

void FolderSelectionModel::excludePath(const QString& pPath) {
	if(mExcludedFolderList.contains(pPath)) {
		return;
	}
	const InclusionState lState = inclusionState(pPath);
	removeSubDirs(pPath, mIncludedFolderList);
	removeSubDirs(pPath, mExcludedFolderList);
	if(lState == StateIncluded) {
		emit includedPathsChanged();
	} else if(lState == StateIncludeInherited) {
		mExcludedFolderList.insert(pPath);
		emit excludedPathsChanged();
	}
	emit dataChanged(index(pPath), findLastLeaf(index(pPath), this));
}

void FolderSelectionModel::setFolders(const QStringList& pIncludedFolders, const QStringList& pExcludedFolders) {
	beginResetModel();
	mIncludedFolderList = QSet<QString>::fromList(pIncludedFolders);
	mExcludedFolderList = QSet<QString>::fromList(pExcludedFolders);
	endResetModel();
}

QStringList FolderSelectionModel::includedFolders() const {
	return mIncludedFolderList.toList();
}

QStringList FolderSelectionModel::excludedFolders() const {
	return mExcludedFolderList.toList();
}

FolderSelectionModel::InclusionState FolderSelectionModel::inclusionState(const QModelIndex& pIndex) const {
	return inclusionState(filePath(pIndex));
}

FolderSelectionModel::InclusionState FolderSelectionModel::inclusionState(const QString& pPath) const {
	if(mIncludedFolderList.contains(pPath)) {
		return StateIncluded;
	}
	else if(mExcludedFolderList.contains(pPath)) {
		return StateExcluded;
	}
	else {
		QString lParent = pPath.section(QDir::separator(), 0, -2, QString::SectionSkipEmpty|QString::SectionIncludeLeadingSep);
		if(lParent.isEmpty()) {
			return StateNone;
		}
		else {
			InclusionState state = inclusionState(lParent);
			if(state == StateNone) {
				return StateNone;
			} else if(state == StateIncluded || state == StateIncludeInherited) {
				return StateIncludeInherited;
			} else {
				return StateExcludeInherited;
			}
		}
	}
}

bool FolderSelectionModel::hiddenFoldersVisible() const {
	return filter() & QDir::Hidden;
}


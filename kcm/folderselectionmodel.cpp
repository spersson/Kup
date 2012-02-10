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
#include <KLocale>


FolderSelectionModel::FolderSelectionModel( QObject* parent )
   : QFileSystemModel( parent )
{
	setHiddenFoldersShown( false );
}


FolderSelectionModel::~FolderSelectionModel()
{
}


void FolderSelectionModel::setHiddenFoldersShown( bool shown )
{
	if ( shown )
		setFilter( QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Hidden );
	else
		setFilter( QDir::AllDirs | QDir::NoDotAndDotDot );
}


Qt::ItemFlags FolderSelectionModel::flags( const QModelIndex &index ) const
{
	Qt::ItemFlags flags = QFileSystemModel::flags( index );
	flags |= Qt::ItemIsUserCheckable;
	if( isForbiddenPath( filePath( index ) ) )
		flags ^= Qt::ItemIsEnabled; //disabled!
	return flags;
}


QVariant FolderSelectionModel::data( const QModelIndex& index, int role ) const
{
	if( index.isValid() && index.column() == 0 ) {
		if( role == Qt::CheckStateRole ) {
			QString path = filePath(index);
			const InclusionState state = inclusionState(path);
			switch( state ) {
			case StateNone:
			case StateExcluded:
			case StateExcludeInherited:
				foreach(QString tested, m_included) {
					if(tested.startsWith(path)) {
						return Qt::PartiallyChecked;
					}
				}
				return Qt::Unchecked;
			case StateIncluded:
			case StateIncludeInherited:
				foreach(QString tested, m_excluded) {
					if(tested.startsWith(path)) {
						return Qt::PartiallyChecked;
					}
				}
				return Qt::Checked;
			}
		}
		else if( role == IncludeStateRole ) {
			return inclusionState( index );
		}
		else if( role == Qt::ForegroundRole ) {
			InclusionState state = inclusionState( index );
			QBrush brush = QFileSystemModel::data( index, role ).value<QBrush>();
			switch( state ) {
			case StateIncluded:
			case StateIncludeInherited:
				brush = QPalette().brush( QPalette::Active, QPalette::Text );
				break;
			case StateNone:
			case StateExcluded:
			case StateExcludeInherited:
				brush = QPalette().brush( QPalette::Disabled, QPalette::Text );
				break;
			}
			return QVariant::fromValue( brush );
		}
		else if ( role == Qt::ToolTipRole ) {
			InclusionState state = inclusionState( index );
			if ( state == StateIncluded || state == StateIncludeInherited ) {
				return i18nc("@info:tooltip %1 is the path of the folder in a listview",
				             "<filename>%1</filename><nl/>(will be included in the backup)", filePath( index ) );
			}
			else {
				return i18nc("@info:tooltip %1 is the path of the folder in a listview",
				             "<filename>%1</filename><nl/> (will <emphasis>not</emphasis> be included in the backup)", filePath( index ) );
			}
		}
		else if ( role == Qt::DecorationRole ) {
			if ( filePath( index ) == QDir::homePath() ) {
				return KIcon( "user-home" );
			}
		}
	}

	return QFileSystemModel::data( index, role );
}


bool FolderSelectionModel::setData( const QModelIndex& index, const QVariant& value, int role )
{
	if( index.isValid() && index.column() == 0 && role == Qt::CheckStateRole ) {
		const QString path = filePath( index );
		const InclusionState state = inclusionState( path );

		// here we ignore the check value, we treat it as a toggle
		// This is due to our using the Qt checking system in a virtual way
		if( state == StateIncluded ||
		    state == StateIncludeInherited ) {
			excludePath( path );
			emit excludedPathsChanged();
			QModelIndex lRecurseIndex = index;
			while(lRecurseIndex.isValid()) {
				emit dataChanged(lRecurseIndex, lRecurseIndex);
				lRecurseIndex = lRecurseIndex.parent();
			}
			return true;
		}
		else {
			includePath(path);
			emit includedPathsChanged();
			QModelIndex lRecurseIndex = index;
			while(lRecurseIndex.isValid()) {
				emit dataChanged(lRecurseIndex, lRecurseIndex);
				lRecurseIndex = lRecurseIndex.parent();
			}
			return true;
		}
		return false;
	}

	return QFileSystemModel::setData( index, value, role );
}


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
}

void FolderSelectionModel::includePath( const QString& path )
{
	if( !m_included.contains( path ) ) {
		// remove all subdirs
		removeSubDirs( path, m_included );
		removeSubDirs( path, m_excluded );
		m_excluded.remove( path );

		// only really include if the parent is not already included
		if( inclusionState( path ) != StateIncludeInherited ) {
			m_included.insert( path );
		}
		emit dataChanged( index( path ), findLastLeaf( index( path ), this ) );
	}
}


void FolderSelectionModel::excludePath( const QString& path )
{
	if( !m_excluded.contains( path ) ) {
		// remove all subdirs
		removeSubDirs( path, m_included );
		removeSubDirs( path, m_excluded );
		m_included.remove( path );

		// only really exclude the path if a parent is included
		if( inclusionState( path ) == StateIncludeInherited ) {
			m_excluded.insert( path );
		}
		emit dataChanged( index( path ), findLastLeaf( index( path ), this ) );
	}
}


void FolderSelectionModel::setFolders( const QStringList& includeDirs, const QStringList& excludeDirs )
{
	beginResetModel();
	m_included = QSet<QString>::fromList( includeDirs );
	m_excluded = QSet<QString>::fromList( excludeDirs );
	endResetModel();
}


QStringList FolderSelectionModel::includedFolders() const
{
	return m_included.toList();
}


QStringList FolderSelectionModel::excludedFolders() const
{
	return m_excluded.toList();
}


inline bool FolderSelectionModel::isForbiddenPath( const QString& path ) const
{
	// we need the trailing slash otherwise we could forbid "/dev-music" for example
	QString _path = path.endsWith( '/' ) ? path : path + '/';
	QFileInfo fi( _path );
	return( _path.startsWith( QLatin1String( "/proc/" ) ) ||
	       _path.startsWith( QLatin1String( "/dev/" ) ) ||
	       _path.startsWith( QLatin1String( "/sys/" ) ) ||
	       _path.startsWith( QLatin1String( "/run/" ) ) ||
	       !fi.isReadable() ||
	       !fi.isExecutable() );
}


FolderSelectionModel::InclusionState FolderSelectionModel::inclusionState( const QModelIndex& index ) const
{
	return inclusionState( filePath( index ) );
}


FolderSelectionModel::InclusionState FolderSelectionModel::inclusionState( const QString& path ) const
{
	if( m_included.contains( path ) ) {
		return StateIncluded;
	}
	else if( m_excluded.contains( path ) ) {
		return StateExcluded;
	}
	else {
		QString parent = path.section( QDir::separator(), 0, -2, QString::SectionSkipEmpty|QString::SectionIncludeLeadingSep );
		if( parent.isEmpty() ) {
			return StateNone;
		}
		else if ( QFileInfo( path ).isHidden() ) {
			// we treat hidden files special - they are disabled by default
			return StateNone;
		}
		else {
			InclusionState state = inclusionState( parent );
			if( state == StateNone )
				return StateNone;
			else if( state == StateIncluded || state == StateIncludeInherited )
				return StateIncludeInherited;
			else
				return StateExcludeInherited;
		}
	}
}


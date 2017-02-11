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
#include "kuputils.h"

#include <QDir>

void ensureTrailingSlash(QString &pPath) {
	if(!pPath.endsWith(QDir::separator())) {
		pPath.append(QDir::separator());
	}
}

void ensureNoTrailingSlash(QString &pPath) {
	while(pPath.endsWith(QDir::separator())) {
		pPath.chop(1);
	}
}

QString lastPartOfPath(const QString &pPath) {
	return pPath.section(QDir::separator(), -1, -1, QString::SectionSkipEmpty);
}

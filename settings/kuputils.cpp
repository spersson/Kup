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
#include <QFile>
#include <QDataStream>
#include <QDateTime>

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

bool fastDiff(const QString &absPath1, const QString &absPath2)
{
	QFileInfo f1(absPath1);
	QFileInfo f2(absPath2);

	if (f1.exists() ^ f2.exists())
		return true;
	if (f1.size() != f2.size())
		return true;
	if (f1.lastModified() != f2.lastModified())
		return true;

#if 0
	if (!f1.open(QIODevice::ReadOnly))
		return true;
	if (!f2.open(QIODevice::ReadOnly))
		return true;

	QDataStream s1(&f1);
	QDataStream s2(&f2);
	QByteArray ba1; ba1.resize(512);
	QByteArray ba2; ba2.resize(512);
	while (!s1.atEnd()) {
		s1.readRawData(ba1.data(), 512);
		s2.readRawData(ba2.data(), 512);
		if (ba1 != ba2)
			return true;
	}
#endif

	return false;
}

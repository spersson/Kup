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

#ifndef BACKUPJOB_H
#define BACKUPJOB_H

#include <KJob>
#include <QStringList>

#include "backupplan.h"

class BackupJob : public KJob
{
	Q_OBJECT

protected:
	BackupJob(const QStringList &pPathsIncluded, const QStringList &pPathsExcluded,
	          const QString &pDestinationPath);
	static void makeNice(int pPid);
	QStringList mPathsIncluded;
	QStringList mPathsExcluded;
	QString mDestinationPath;
};

#endif // BACKUPJOB_H

/***************************************************************************
 *   Copyright Luca Carlon                                                 *
 *   carlon.luca@gmail.com                                                 *
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

#include <QDir>

#include "fsminerrestic.h"
#include "kupfiledigger_debug.h"

FsMinerRestic::FsMinerRestic(const QString &mountPath, QObject *parent) :
	FsMiner(mountPath, parent)
{

}

void FsMinerRestic::doProcess()
{
#define EMIT_AND_RETURN(snapshots) {                          \
		emit backupProcessed(BackupDescriptor { snapshots }); \
		return;                                               \
	}

#define EMIT_AND_RETURN_EMPTY EMIT_AND_RETURN(QList<BackupSnapshot>())

	QString snapshotsPath = QString("%1%2snapshots")
			.arg(mMountPath).arg(QDir::separator());
	QDir snapshotsDir(snapshotsPath);
	if (!snapshotsDir.exists()) {
        qCWarning(KUPFILEDIGGER) << "Cannot find snapshots dir";
		EMIT_AND_RETURN_EMPTY;
	}

	QFileInfoList snapshotListDir = snapshotsDir
			.entryInfoList(QStringList(), QDir::Dirs | QDir::NoDotAndDotDot);
	BackupDescriptor descriptor;
	foreach (QFileInfo fileInfo, snapshotListDir) {
		QDateTime snapshotDateTime =
				QDateTime::fromString(fileInfo.fileName(), Qt::ISODate);
		if (snapshotDateTime.isNull() || !snapshotDateTime.isValid())
			continue;
		BackupSnapshot snaphot {
			snapshotDateTime,
			fileInfo.absoluteFilePath()
		};
		descriptor.mSnapshots.append(snaphot);
	}

	emit backupProcessed(descriptor);
}

QString FsMinerRestic::pathToLatest()
{
	return QString("%1%2snapshots%2latest")
			.arg(mMountPath).arg(QDir::separator());
}

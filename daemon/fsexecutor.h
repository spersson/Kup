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

#ifndef FSEXECUTOR_H
#define FSEXECUTOR_H

#include "planexecutor.h"

#include <QThread>

class BackupPlan;

class KDirWatch;
class KJob;

class QTimer;

// KDirWatch (well, inotify) does not detect when something gets mounted on a watched directory.
// work around this problem by monitoring the mounts of the system in a separate thread.
class MountWatcher: public QThread {
	Q_OBJECT

signals:
	void mountsChanged();

protected:
	virtual void run();
};


// Plan executor that stores the backup to a path in the local
// filesystem, uses KDirWatch to monitor for when the folder
// becomes available/unavailable. Can be used for external
// drives or networked filesystems if you always mount it at
// the same mountpoint.
class FSExecutor: public PlanExecutor
{
Q_OBJECT

public:
	FSExecutor(BackupPlan *pPlan, QObject *pParent);

public slots:
	virtual void checkStatus();

protected slots:
	virtual void startBackup();
	void slotBackupDone(KJob *pJob);
	void slotBackupSizeDone(KJob *pJob);
	void checkMountPoints();

protected:
	QString mWatchedParentDir;
	KDirWatch *mDirWatch;
	MountWatcher mMountWatcher;
};

#endif // FSEXECUTOR_H

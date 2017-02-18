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

#ifndef EDEXECUTOR_H
#define EDEXECUTOR_H

#include "planexecutor.h"

#include <Solid/Device>
#include <Solid/StorageAccess>

class BackupPlan;

class KJob;

// Plan executor that stores the backup to an external disk.
// Uses libsolid to monitor for when it becomes available.
class EDExecutor: public PlanExecutor
{
Q_OBJECT

public:
	EDExecutor(BackupPlan *pPlan, KupDaemon *pKupDaemon);

public slots:
	virtual void checkStatus();
	virtual void showFilesClicked();

protected slots:
	void deviceAdded(const QString &pUdi);
	void deviceRemoved(const QString &pUdi);
	void updateAccessibility();
	virtual void startBackup();
	void slotBackupDone(KJob *pJob);
	void slotBackupSizeDone(KJob *pJob);

protected:
	Solid::StorageAccess *mStorageAccess;
	QString mCurrentUdi;
	bool mWantsToRunBackup;
	bool mWantsToShowFiles;
};

#endif // EDEXECUTOR_H

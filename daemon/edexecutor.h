/***************************************************************************
 *   Copyright Simon Persson                                               *
 *   simonop@spray.se                                                      *
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
class KNotification;

class QTimer;

class EDExecutor: public PlanExecutor
{
Q_OBJECT

public:
	EDExecutor(BackupPlan *pPlan, QObject *pParent);

public slots:
	virtual void checkStatus();
	void deviceAdded(const QString &pUdi);
	void deviceRemoved(const QString &pUdi);
	void checkAccessibility();
	void updateAccessibility();
	void askUserToStartBackup();
	virtual void startBackup();
	void startBackupJob();
	void discardNotification();
	void slotBackupDone(KJob *pJob);
	void slotBackupSizeDone(KJob *pJob);
//	void deviceRemoved(const QString &pUdi);


protected:
	Solid::StorageAccess *mStorageAccess;
	QString mCurrentUdi;
	bool mWantsToRunBackup;
	KNotification *mQuestion;
	QTimer *mRunBackupTimer;
	QTimer *mAskUserTimer;
};

#endif // EDEXECUTOR_H

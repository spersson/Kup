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

#ifndef FSEXECUTOR_H
#define FSEXECUTOR_H

#include "planexecutor.h"

class BackupPlan;

class KDirWatch;
class KJob;
class KNotification;

class QTimer;

class FSExecutor: public PlanExecutor
{
Q_OBJECT

public:
	FSExecutor(BackupPlan *pPlan, QObject *pParent);

public slots:
	virtual void checkStatus();
	void checkAccessibility();
	void askUserToStartBackup();
	virtual void startBackup();
	void discardNotification();
	void slotBackupDone(KJob *pJob);
	void slotBackupSizeDone(KJob *pJob);

protected:
	KNotification *mQuestion;
	QTimer *mRunBackupTimer;
	QTimer *mAskUserTimer;
	QString mWatchedParentDir;
};

#endif // FSEXECUTOR_H

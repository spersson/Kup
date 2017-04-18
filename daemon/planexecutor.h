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

#ifndef PLANEXECUTOR_H
#define PLANEXECUTOR_H

#include "backupplan.h"
#include "backupjob.h"

#include <KProcess>

class KupDaemon;

class KRun;
class KNotification;
class KProcess;

class QTimer;

// Accumulate usage time every KUP_USAGE_MONITOR_INTERVAL_S while user is active.
// Consider user inactive after KUP_IDLE_TIMEOUT_S s of no keyboard or mouse activity.
#define KUP_USAGE_MONITOR_INTERVAL_S 2*60
#define KUP_IDLE_TIMEOUT_S 30

class PlanExecutor : public QObject
{
	Q_OBJECT
public:
	PlanExecutor(BackupPlan *pPlan, KupDaemon *pKupDaemon);
	virtual ~PlanExecutor();

	BackupPlan::ScheduleType scheduleType() {
		return (BackupPlan::ScheduleType)mPlan->mScheduleType;
	}

	bool busy() {
		return mState == BACKUP_RUNNING || mState == INTEGRITY_TESTING || mState == REPAIRING;
	}
	bool destinationAvailable() {
		return mState != NOT_AVAILABLE;
	}

	QString currentActivityTitle();

	enum ExecutorState {NOT_AVAILABLE, WAITING_FOR_FIRST_BACKUP,
		                 WAITING_FOR_BACKUP_AGAIN, BACKUP_RUNNING, WAITING_FOR_MANUAL_BACKUP,
		                 INTEGRITY_TESTING, REPAIRING};
	ExecutorState mState;
	QString mDestinationPath;
	QString mLogFilePath;
	BackupPlan *mPlan;

public slots:
	virtual void checkStatus() = 0;
	virtual void showBackupFiles();
	void updateAccumulatedUsageTime();
	void startIntegrityCheck();
	void startRepairJob();
	void startBackupSaveJob();
	void showLog();

signals:
	void stateChanged();
	void backupStatusChanged();

protected slots:
	virtual void startBackup() = 0;

	void enterBackupRunningState();
	void exitBackupRunningState(bool pWasSuccessful);
	void enterAvailableState();
	void enterNotAvailableState();

	void askUser(const QString &pQuestion);
	void discardUserQuestion();

	void notifyBackupFailed(KJob *pFailedJob);
	void discardFailNotification();

	void notifyBackupSucceeded();

	void integrityCheckFinished(KJob *pJob);
	void discardIntegrityNotification();
	void repairFinished(KJob *pJob);
	void discardRepairNotification();

	void startSleepInhibit();
	void endSleepInhibit();

protected:
	BackupJob *createBackupJob();
	bool powerSaveActive();

	KNotification *mQuestion;
	QTimer *mSchedulingTimer;
	KNotification *mFailNotification;
	KNotification *mIntegrityNotification;
	KNotification *mRepairNotification;
	ExecutorState mLastState;
	KupDaemon *mKupDaemon;
	uint mSleepCookie;
};

#endif // PLANEXECUTOR_H

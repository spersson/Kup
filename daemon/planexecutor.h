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

#include <KProcess>

class KRun;
class KNotification;
class KProcess;

class QAction;
class QMenu;
class QTimer;

// Accumulate usage time every KUP_USAGE_MONITOR_INTERVAL_S while user is active.
// Consider user inactive after KUP_IDLE_TIMEOUT_S s of no keyboard or mouse activity.
#define KUP_USAGE_MONITOR_INTERVAL_S 2*60
#define KUP_IDLE_TIMEOUT_S 30

class PlanExecutor : public QObject
{
	Q_OBJECT
public:
	PlanExecutor(BackupPlan *pPlan, QObject *pParent);
	virtual ~PlanExecutor();

	bool destinationAvailable() {
		return mState != NOT_AVAILABLE;
	}

	bool running() {
		return mState == RUNNING;
	}

	virtual QMenu *planActions() {
		return mActionMenu;
	}

	QString description() {
		return mPlan->mDescription;
	}

	BackupPlan::Status planStatus() {
		return mPlan->backupStatus();
	}

	BackupPlan::ScheduleType scheduleType() {
		return (BackupPlan::ScheduleType)mPlan->mScheduleType;
	}

public slots:
	virtual void checkStatus() = 0;
	virtual void showFilesClicked();
	void updateAccumulatedUsageTime();

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

	void mountBupFuse();
	void unmountBupFuse();
	void bupFuseFinished(int pExitCode, QProcess::ExitStatus pExitStatus);
	void showMountedBackup();

protected:
	enum ExecutorState {NOT_AVAILABLE, WAITING_FOR_FIRST_BACKUP,
		                 WAITING_FOR_BACKUP_AGAIN, RUNNING, WAITING_FOR_MANUAL_BACKUP};
	ExecutorState mState;

	QString mDestinationPath;
	BackupPlan *mPlan;
	QMenu *mActionMenu;
	QAction *mShowFilesAction;
	QAction *mRunBackupAction;
	KProcess *mBupFuseProcess;
	QString mTempDir;
	KNotification *mQuestion;
	QTimer *mSchedulingTimer;

	bool mOkToShowBackup;
};

#endif // PLANEXECUTOR_H

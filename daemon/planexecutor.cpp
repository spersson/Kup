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

#include "planexecutor.h"
#include "bupjob.h"
#include "bupverificationjob.h"
#include "buprepairjob.h"
#include "rsyncjob.h"

#include <QAction>
#include <QDir>
#include <QMenu>
#include <QTimer>

#include <KFormat>
#include <KLocalizedString>
#include <KNotification>
#include <KRun>

PlanExecutor::PlanExecutor(BackupPlan *pPlan, QObject *pParent)
   :QObject(pParent), mState(NOT_AVAILABLE), mPlan(pPlan), mQuestion(NULL),
     mFailNotification(NULL), mIntegrityNotification(NULL), mRepairNotification(NULL)
{
	QString lCachePath = QString::fromLocal8Bit(qgetenv("XDG_CACHE_HOME").constData());
	if(lCachePath.isEmpty()) {
		lCachePath = QDir::homePath();
		lCachePath.append(QLatin1String("/.cache"));
	}
	lCachePath.append(QLatin1String("/kup"));
	QDir lCacheDir(lCachePath);
	if(!lCacheDir.exists()) {
		if(!lCacheDir.mkpath(lCachePath)) {
			lCachePath = QLatin1String("/tmp");
		}
	}
	mLogFilePath = lCachePath;
	mLogFilePath.append(QLatin1String("/kup_plan"));
	mLogFilePath.append(QString::number(mPlan->planNumber()));
	mLogFilePath.append(QLatin1String(".log"));

	mRunBackupAction = new QAction(i18nc("@action:inmenu", "Take Backup Now"), this);
	mRunBackupAction->setEnabled(false);
	connect(mRunBackupAction, SIGNAL(triggered()), SLOT(enterBackupRunningState()));

	mShowFilesAction = new QAction(i18nc("@action:inmenu", "Show Files"), this);
	mShowFilesAction->setEnabled(false);
	connect(mShowFilesAction, SIGNAL(triggered()), SLOT(showFilesClicked()));

	mShowLogFileAction = new QAction(i18nc("@action:inmenu", "Show Log File"), this);
	mShowLogFileAction->setEnabled(QFileInfo(mLogFilePath).exists());
	connect(mShowLogFileAction, SIGNAL(triggered()), SLOT(showLog()));

	mActionMenu = new QMenu(mPlan->mDescription);
	mActionMenu->addAction(mRunBackupAction);
	mActionMenu->addAction(mShowFilesAction);
	mActionMenu->addAction(mShowLogFileAction);

	mSchedulingTimer = new QTimer(this);
	mSchedulingTimer->setSingleShot(true);
	connect(mSchedulingTimer, SIGNAL(timeout()), SLOT(enterAvailableState()));
}

PlanExecutor::~PlanExecutor() {
}

QString PlanExecutor::currentActivityTitle() {
	switch(mState) {
	case BACKUP_RUNNING:
		return i18nc("@info:tooltip", "Taking new backup");
	case INTEGRITY_TESTING:
		return i18nc("@info:tooltip", "Checking backup integrity");
	case REPAIRING:
		return i18nc("@info:tooltip", "Repairing backups");
	default:
		return QString();
	}
}

// dispatcher code for entering one of the available states
void PlanExecutor::enterAvailableState() {
	if(mState == NOT_AVAILABLE) {
		mShowFilesAction->setEnabled(true);
		mRunBackupAction->setEnabled(true);
		mState = WAITING_FOR_FIRST_BACKUP; //initial child state of "Available" state
		emit stateChanged();
	}

	bool lShouldBeTakenNow = false;
	bool lShouldBeTakenLater = false;
	int lTimeUntilNextWakeup;
	QString lUserQuestion;
	QDateTime lNow = QDateTime::currentDateTime().toUTC();

	switch(mPlan->mScheduleType) {
	case BackupPlan::MANUAL:
		break;
	case BackupPlan::INTERVAL: {
		QDateTime lNextTime = mPlan->nextScheduledTime();
		if(!lNextTime.isValid() || lNextTime < lNow) {
			lShouldBeTakenNow = true;
			if(!mPlan->mLastCompleteBackup.isValid())
				lUserQuestion = i18nc("@info", "Do you want to take a first backup now?");
			else {
				QString t = KFormat().formatSpelloutDuration(mPlan->mLastCompleteBackup.secsTo(lNow) * 1000);
				lUserQuestion = i18nc("@info", "It's been %1 since the last backup was taken, "
				                     "do you want to take a backup now?", t);
			}
		} else {
			lShouldBeTakenLater = true;
			lTimeUntilNextWakeup = lNow.secsTo(lNextTime)*1000;
		}
		break;
	}
	case BackupPlan::USAGE:
		if(!mPlan->mLastCompleteBackup.isValid()) {
			lShouldBeTakenNow = true;
			lUserQuestion = i18nc("@info", "Do you want to take a first backup now?");
		} else if(mPlan->mAccumulatedUsageTime > (quint32)mPlan->mUsageLimit * 3600) {
			lShouldBeTakenNow = true;
			QString t = KFormat().formatSpelloutDuration(mPlan->mAccumulatedUsageTime * 1000);
			lUserQuestion = i18nc("@info", "You've been active with this computer for %1 since the last backup was taken, "
			                     "do you want to take a backup now?", t);
		}
		break;
	}

	if(lShouldBeTakenNow) {
		// only ask the first time after destination has become available
		if(mPlan->mAskBeforeTakingBackup && mState == WAITING_FOR_FIRST_BACKUP) {
			askUser(lUserQuestion);
		} else {
			enterBackupRunningState();
		}
	} else if(lShouldBeTakenLater){
		// schedule a wakeup for asking again when the time is right.
		mSchedulingTimer->start(lTimeUntilNextWakeup);
	}
}

void PlanExecutor::enterNotAvailableState() {
	mSchedulingTimer->stop();
	mShowFilesAction->setEnabled(false);
	mRunBackupAction->setEnabled(false);
	mState = NOT_AVAILABLE;
	emit stateChanged();
}

void PlanExecutor::askUser(const QString &pQuestion) {
	discardUserQuestion();
	mQuestion = new KNotification(QLatin1String("StartBackup"), KNotification::Persistent);
	mQuestion->setTitle(i18nc("@title:window", "Backup Device Available - %1", mPlan->mDescription));
	mQuestion->setText(pQuestion);
	QStringList lAnswers;
	lAnswers << i18nc("@action:button", "Yes") << i18nc("@action:button", "No");
	mQuestion->setActions(lAnswers);
	connect(mQuestion, SIGNAL(action1Activated()), SLOT(enterBackupRunningState()));
	connect(mQuestion, SIGNAL(action2Activated()), SLOT(discardUserQuestion()));
	connect(mQuestion, SIGNAL(closed()), SLOT(discardUserQuestion()));
	connect(mQuestion, SIGNAL(ignored()), SLOT(discardUserQuestion()));
	// enter this "do nothing" state, if user answers "no" or ignores, remain there
	mState = WAITING_FOR_MANUAL_BACKUP;
	emit stateChanged();
	mQuestion->sendEvent();
}

void PlanExecutor::discardUserQuestion() {
	if(mQuestion) {
		mQuestion->deleteLater();
		mQuestion = NULL;
	}
}

void PlanExecutor::notifyBackupFailed(KJob *pFailedJob) {
	discardFailNotification();
	mFailNotification = new KNotification(QLatin1String("BackupFailed"), KNotification::Persistent);
	mFailNotification->setTitle(i18nc("@title:window", "Saving of Backup Failed"));
	mFailNotification->setText(pFailedJob->errorText());

	QStringList lAnswers;
	if(pFailedJob->error() == BackupJob::ErrorWithLog) {
		lAnswers << i18nc("@action:button", "Show log file");
		connect(mFailNotification, SIGNAL(action1Activated()), SLOT(showLog()));
	} else if(pFailedJob->error() == BackupJob::ErrorSuggestRepair) {
		lAnswers << i18nc("@action:button", "Yes");
		lAnswers << i18nc("@action:button", "No");
		connect(mFailNotification, SIGNAL(action1Activated()), SLOT(startRepairJob()));
	}
	mFailNotification->setActions(lAnswers);

	connect(mFailNotification, SIGNAL(action2Activated()), SLOT(discardFailNotification()));
	connect(mFailNotification, SIGNAL(closed()), SLOT(discardFailNotification()));
	connect(mFailNotification, SIGNAL(ignored()), SLOT(discardFailNotification()));
	mFailNotification->sendEvent();
}

void PlanExecutor::discardFailNotification() {
	if(mFailNotification) {
		mFailNotification->deleteLater();
		mFailNotification = NULL;
	}
}

void PlanExecutor::showLog() {
	KRun::runUrl(mLogFilePath, QLatin1String("text/x-log"), NULL);
}

void PlanExecutor::startIntegrityCheck() {
	if(mPlan->mBackupType != BackupPlan::BupType || busy() || mState == NOT_AVAILABLE) {
		return;
	}
	KJob *lJob = new BupVerificationJob(*mPlan, mDestinationPath, mLogFilePath);
	connect(lJob, SIGNAL(result(KJob*)), SLOT(integrityCheckFinished(KJob*)));
	lJob->start();
	mLastState = mState;
	mState = INTEGRITY_TESTING;
	emit stateChanged();
	mRunBackupAction->setEnabled(false);
}

void PlanExecutor::startRepairJob() {
	if(mPlan->mBackupType != BackupPlan::BupType || busy() || mState == NOT_AVAILABLE) {
		return;
	}
	KJob *lJob = new BupRepairJob(*mPlan, mDestinationPath, mLogFilePath);
	connect(lJob, SIGNAL(result(KJob*)), SLOT(repairFinished(KJob*)));
	lJob->start();
	mLastState = mState;
	mState = REPAIRING;
	emit stateChanged();
	mRunBackupAction->setEnabled(false);
}

void PlanExecutor::integrityCheckFinished(KJob *pJob) {
	discardIntegrityNotification();
	mIntegrityNotification = new KNotification(QLatin1String("IntegrityCheckCompleted"), KNotification::Persistent);
	mIntegrityNotification->setTitle(i18nc("@title:window", "Integrity Check Completed"));
	mIntegrityNotification->setText(pJob->errorText());
	QStringList lAnswers;
	if(pJob->error() == BackupJob::ErrorWithLog) {
		lAnswers << i18nc("@action:button", "Show log file");
		connect(mIntegrityNotification, SIGNAL(action1Activated()), SLOT(showLog()));
	} else if(pJob->error() == BackupJob::ErrorSuggestRepair) {
		lAnswers << i18nc("@action:button", "Yes");
		lAnswers << i18nc("@action:button", "No");
		connect(mIntegrityNotification, SIGNAL(action1Activated()), SLOT(startRepairJob()));
	}
	mIntegrityNotification->setActions(lAnswers);

	connect(mIntegrityNotification, SIGNAL(action2Activated()), SLOT(discardIntegrityNotification()));
	connect(mIntegrityNotification, SIGNAL(closed()), SLOT(discardIntegrityNotification()));
	connect(mIntegrityNotification, SIGNAL(ignored()), SLOT(discardIntegrityNotification()));
	mIntegrityNotification->sendEvent();

	if(mState == INTEGRITY_TESTING) { //only restore if nothing has changed during the run
		mState = mLastState;
	}
	emit stateChanged();
	mRunBackupAction->setEnabled(true);
}

void PlanExecutor::discardIntegrityNotification() {
	if(mIntegrityNotification) {
		mIntegrityNotification->deleteLater();
		mIntegrityNotification = NULL;
	}
}

void PlanExecutor::repairFinished(KJob *pJob) {
	discardRepairNotification();
	mRepairNotification = new KNotification(QLatin1String("RepairCompleted"), KNotification::Persistent);
	mRepairNotification->setTitle(i18nc("@title:window", "Repair Completed"));
	mRepairNotification->setText(pJob->errorText());
	QStringList lAnswers;
	lAnswers << i18nc("@action:button", "Show log file");
	mRepairNotification->setActions(lAnswers);
	connect(mRepairNotification, SIGNAL(action1Activated()), SLOT(showLog()));
	connect(mRepairNotification, SIGNAL(closed()), SLOT(discardRepairNotification()));
	connect(mRepairNotification, SIGNAL(ignored()), SLOT(discardRepairNotification()));
	mRepairNotification->sendEvent();

	if(mState == REPAIRING) { //only restore if nothing has changed during the run
		mState = mLastState;
	}
	emit stateChanged();
	mRunBackupAction->setEnabled(true);
}

void PlanExecutor::discardRepairNotification() {
	if(mRepairNotification) {
		mRepairNotification->deleteLater();
		mRepairNotification = NULL;
	}
}

void PlanExecutor::enterBackupRunningState() {
	discardUserQuestion();
	mState = BACKUP_RUNNING;
	emit stateChanged();
	mRunBackupAction->setEnabled(false);
	startBackup();
}

void PlanExecutor::exitBackupRunningState(bool pWasSuccessful) {
	mRunBackupAction->setEnabled(true);
	mShowLogFileAction->setEnabled(QFileInfo(mLogFilePath).exists());
	if(pWasSuccessful) {
		if(mPlan->mScheduleType == BackupPlan::USAGE) {
			//reset usage time after successful backup
			mPlan->mAccumulatedUsageTime =0;
			mPlan->save();
		}
		mState = WAITING_FOR_BACKUP_AGAIN;
		emit stateChanged();

		//don't know if status actually changed, potentially did... so trigger a re-read of status
		emit backupStatusChanged();

		// re-enter the main "available" state dispatcher
		enterAvailableState();
	} else {
		mState = WAITING_FOR_MANUAL_BACKUP;
		emit stateChanged();
	}
}

void PlanExecutor::updateAccumulatedUsageTime() {
	if(mState == BACKUP_RUNNING) { //usage time during backup doesn't count...
		return;
	}

	if(mPlan->mScheduleType == BackupPlan::USAGE) {
		mPlan->mAccumulatedUsageTime += KUP_USAGE_MONITOR_INTERVAL_S;
		mPlan->save();
	}

	// trigger refresh of backup status, potentially changed since some time has passed...
	// this is the reason why this slot is called repeatedly even when
	// not in BackupPlan::USAGE mode
	emit backupStatusChanged();

	//if we're waiting to run backup again, check if it is time now.
	if(mPlan->mScheduleType == BackupPlan::USAGE &&
	      (mState == WAITING_FOR_FIRST_BACKUP || mState == WAITING_FOR_BACKUP_AGAIN)) {
		enterAvailableState();
	}
}

void PlanExecutor::showFilesClicked() {
	if(mState == NOT_AVAILABLE)
		return;
	if(mPlan->mBackupType == BackupPlan::BupType) {
		QString lCommandLine = QString::fromLatin1("kup-filedigger --title \"");
		lCommandLine.append(mPlan->mDescription);
		lCommandLine.append(QLatin1String("\" \""));
		lCommandLine.append(mDestinationPath);
		lCommandLine.append(QLatin1String("\""));
		KRun::runCommand(lCommandLine, NULL);
	} else if(mPlan->mBackupType == BackupPlan::RsyncType) {
		KRun::runUrl(mDestinationPath, QLatin1String("inode/directory"), NULL);
	}
}

BackupJob *PlanExecutor::createBackupJob() {
	if(mPlan->mBackupType == BackupPlan::BupType) {
		return new BupJob(*mPlan, mDestinationPath, mLogFilePath);
	} else if(mPlan->mBackupType == BackupPlan::RsyncType) {
		return new RsyncJob(*mPlan, mDestinationPath, mLogFilePath);
	}
	qWarning("Invalid backup type in configuration!");
	return NULL;
}

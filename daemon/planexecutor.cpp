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
#include "kupdaemon.h"
#include "kupdaemon_debug.h"
#include "rsyncjob.h"

#include <QDBusConnection>
#include <QDBusReply>
#include <QDir>
#include <QTimer>

#include <KFormat>
#include <KLocalizedString>
#include <KNotification>
#include <KRun>

static QString sPwrMgmtServiceName = QStringLiteral("org.freedesktop.PowerManagement");
static QString sPwrMgmtPath = QStringLiteral("/org/freedesktop/PowerManagement");
static QString sPwrMgmtInhibitInterface = QStringLiteral("org.freedesktop.PowerManagement.Inhibit");
static QString sPwrMgmtInterface = QStringLiteral("org.freedesktop.PowerManagement");

PlanExecutor::PlanExecutor(BackupPlan *pPlan, KupDaemon *pKupDaemon)
   :QObject(pKupDaemon), mState(NOT_AVAILABLE), mPlan(pPlan), mQuestion(nullptr),
     mFailNotification(nullptr), mIntegrityNotification(nullptr), mRepairNotification(nullptr),
     mKupDaemon(pKupDaemon), mSleepCookie(0)
{
	QString lCachePath = QString::fromLocal8Bit(qgetenv("XDG_CACHE_HOME").constData());
	if(lCachePath.isEmpty()) {
		lCachePath = QDir::homePath();
		lCachePath.append(QStringLiteral("/.cache"));
	}
	lCachePath.append(QStringLiteral("/kup"));
	QDir lCacheDir(lCachePath);
	if(!lCacheDir.exists()) {
		if(!lCacheDir.mkpath(lCachePath)) {
			lCachePath = QStringLiteral("/tmp");
		}
	}
	mLogFilePath = lCachePath;
	mLogFilePath.append(QStringLiteral("/kup_plan"));
	mLogFilePath.append(QString::number(mPlan->planNumber()));
	mLogFilePath.append(QStringLiteral(".log"));

	mSchedulingTimer = new QTimer(this);
	mSchedulingTimer->setSingleShot(true);
	connect(mSchedulingTimer, SIGNAL(timeout()), SLOT(enterAvailableState()));
}

PlanExecutor::~PlanExecutor() {
}

QString PlanExecutor::currentActivityTitle() {
	switch(mState) {
	case BACKUP_RUNNING:
		return i18nc("status in tooltip", "Saving backup");
	case INTEGRITY_TESTING:
		return i18nc("status in tooltip", "Checking backup integrity");
	case REPAIRING:
		return i18nc("status in tooltip", "Repairing backups");
	default:
		switch (mPlan->backupStatus()) {
		case BackupPlan::GOOD:
			return i18nc("status in tooltip", "Backup status OK");
		case BackupPlan::MEDIUM:
			return i18nc("status in tooltip", "New backup suggested");
		case BackupPlan::BAD:
			return i18nc("status in tooltip", "New backup needed");
		case BackupPlan::NO_STATUS:
			return QStringLiteral("");
		default:
			return QString();
		}
	}
}

// dispatcher code for entering one of the available states
void PlanExecutor::enterAvailableState() {
	if(mState == NOT_AVAILABLE) {
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
				lUserQuestion = xi18nc("@info", "Do you want to save a first backup now?");
			else {
				QString t = KFormat().formatSpelloutDuration(mPlan->mLastCompleteBackup.secsTo(lNow) * 1000);
				lUserQuestion = xi18nc("@info", "It has been %1 since last backup was saved.\n"
				                                "Save a new backup now?", t);
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
			lUserQuestion = xi18nc("@info", "Do you want to save a first backup now?");
		} else if(mPlan->mAccumulatedUsageTime > (quint32)mPlan->mUsageLimit * 3600) {
			lShouldBeTakenNow = true;
			QString t = KFormat().formatSpelloutDuration(mPlan->mAccumulatedUsageTime * 1000);
			lUserQuestion = xi18nc("@info", "You have been active for %1 since last backup was saved.\n"
			                                "Save a new backup now?", t);
		}
		break;
	}

	if(lShouldBeTakenNow) {
		// Only ask the first time after destination has become available.
		// Always ask if power saving is active.
		if( (mPlan->mAskBeforeTakingBackup && mState == WAITING_FOR_FIRST_BACKUP) ||
		    powerSaveActive()) {
			askUser(lUserQuestion);
		} else {
			startBackupSaveJob();
		}
	} else if(lShouldBeTakenLater){
		// schedule a wakeup for asking again when the time is right.
		mSchedulingTimer->start(lTimeUntilNextWakeup);
	}
}

void PlanExecutor::enterNotAvailableState() {
	discardUserQuestion();
	mSchedulingTimer->stop();
	mState = NOT_AVAILABLE;
	emit stateChanged();
}

void PlanExecutor::askUser(const QString &pQuestion) {
	discardUserQuestion();
	mQuestion = new KNotification(QStringLiteral("StartBackup"), KNotification::Persistent);
	mQuestion->setTitle(mPlan->mDescription);
	mQuestion->setText(pQuestion);
	QStringList lAnswers;
	lAnswers << xi18nc("@action:button", "Yes") << xi18nc("@action:button", "No");
	mQuestion->setActions(lAnswers);
	connect(mQuestion, SIGNAL(action1Activated()), SLOT(startBackupSaveJob()));
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
		mQuestion = nullptr;
	}
}

void PlanExecutor::notifyBackupFailed(KJob *pFailedJob) {
	discardFailNotification();
	mFailNotification = new KNotification(QStringLiteral("BackupFailed"), KNotification::Persistent);
	mFailNotification->setTitle(xi18nc("@title:window", "Saving of Backup Failed"));
	mFailNotification->setText(pFailedJob->errorText());

	QStringList lAnswers;
	if(pFailedJob->error() == BackupJob::ErrorWithLog) {
		lAnswers << xi18nc("@action:button", "Show log file");
		connect(mFailNotification, SIGNAL(action1Activated()), SLOT(showLog()));
	} else if(pFailedJob->error() == BackupJob::ErrorSuggestRepair) {
		lAnswers << xi18nc("@action:button", "Yes");
		lAnswers << xi18nc("@action:button", "No");
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
		mFailNotification = nullptr;
	}
}

void PlanExecutor::notifyBackupSucceeded() {
	KNotification *lNotification = new KNotification(QStringLiteral("BackupSucceeded"));
	lNotification->setTitle(xi18nc("@title:window", "Backup Saved"));
	lNotification->setText(xi18nc("@info notification", "Saving backup completed successfully."));
	lNotification->sendEvent();
}

void PlanExecutor::showLog() {
	KRun::runUrl(QUrl::fromLocalFile(mLogFilePath), QStringLiteral("text/x-log"), nullptr);
}

void PlanExecutor::startIntegrityCheck() {
	if(mPlan->mBackupType != BackupPlan::BupType || busy() || !destinationAvailable()) {
		return;
	}
	KJob *lJob = new BupVerificationJob(*mPlan, mDestinationPath, mLogFilePath, mKupDaemon);
	connect(lJob, SIGNAL(result(KJob*)), SLOT(integrityCheckFinished(KJob*)));
	lJob->start();
	mLastState = mState;
	mState = INTEGRITY_TESTING;
	emit stateChanged();
	startSleepInhibit();
}

void PlanExecutor::startRepairJob() {
	if(mPlan->mBackupType != BackupPlan::BupType || busy() || !destinationAvailable()) {
		return;
	}
	KJob *lJob = new BupRepairJob(*mPlan, mDestinationPath, mLogFilePath, mKupDaemon);
	connect(lJob, SIGNAL(result(KJob*)), SLOT(repairFinished(KJob*)));
	lJob->start();
	mLastState = mState;
	mState = REPAIRING;
	emit stateChanged();
	startSleepInhibit();
}

void PlanExecutor::startBackupSaveJob() {
	if(busy() || !destinationAvailable()) {
		return;
	}
	discardUserQuestion();
	mState = BACKUP_RUNNING;
	emit stateChanged();
	startSleepInhibit();
	startBackup();
}

void PlanExecutor::integrityCheckFinished(KJob *pJob) {
	endSleepInhibit();
	discardIntegrityNotification();
	mIntegrityNotification = new KNotification(QStringLiteral("IntegrityCheckCompleted"), KNotification::Persistent);
	mIntegrityNotification->setTitle(xi18nc("@title:window", "Integrity Check Completed"));
	mIntegrityNotification->setText(pJob->errorText());
	QStringList lAnswers;
	if(pJob->error() == BackupJob::ErrorWithLog) {
		lAnswers << xi18nc("@action:button", "Show log file");
		connect(mIntegrityNotification, SIGNAL(action1Activated()), SLOT(showLog()));
	} else if(pJob->error() == BackupJob::ErrorSuggestRepair) {
		lAnswers << xi18nc("@action:button", "Yes");
		lAnswers << xi18nc("@action:button", "No");
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
}

void PlanExecutor::discardIntegrityNotification() {
	if(mIntegrityNotification) {
		mIntegrityNotification->deleteLater();
		mIntegrityNotification = nullptr;
	}
}

void PlanExecutor::repairFinished(KJob *pJob) {
	endSleepInhibit();
	discardRepairNotification();
	mRepairNotification = new KNotification(QStringLiteral("RepairCompleted"), KNotification::Persistent);
	mRepairNotification->setTitle(xi18nc("@title:window", "Repair Completed"));
	mRepairNotification->setText(pJob->errorText());
	QStringList lAnswers;
	lAnswers << xi18nc("@action:button", "Show log file");
	mRepairNotification->setActions(lAnswers);
	connect(mRepairNotification, SIGNAL(action1Activated()), SLOT(showLog()));
	connect(mRepairNotification, SIGNAL(closed()), SLOT(discardRepairNotification()));
	connect(mRepairNotification, SIGNAL(ignored()), SLOT(discardRepairNotification()));
	mRepairNotification->sendEvent();

	if(mState == REPAIRING) { //only restore if nothing has changed during the run
		mState = mLastState;
	}
	emit stateChanged();
}

void PlanExecutor::discardRepairNotification() {
	if(mRepairNotification) {
		mRepairNotification->deleteLater();
		mRepairNotification = nullptr;
	}
}

void PlanExecutor::startSleepInhibit() {
	if(mSleepCookie != 0) {
		return;
	}
	QDBusMessage lMsg = QDBusMessage::createMethodCall(sPwrMgmtServiceName,
	                                                   sPwrMgmtPath,
	                                                   sPwrMgmtInhibitInterface,
	                                                   QStringLiteral("Inhibit"));
	lMsg << i18n("Kup Backup System");
	lMsg << currentActivityTitle();
	QDBusReply<uint> lReply = QDBusConnection::sessionBus().call(lMsg);
	mSleepCookie = lReply.value();
}

void PlanExecutor::endSleepInhibit() {
	if(mSleepCookie == 0) {
		return;
	}
	QDBusMessage lMsg = QDBusMessage::createMethodCall(sPwrMgmtServiceName,
	                                                  sPwrMgmtPath,
	                                                  sPwrMgmtInhibitInterface,
	                                                  QStringLiteral("UnInhibit"));
	lMsg << mSleepCookie;
	QDBusConnection::sessionBus().asyncCall(lMsg);
	mSleepCookie = 0;
}

void PlanExecutor::exitBackupRunningState(bool pWasSuccessful) {
	endSleepInhibit();
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

void PlanExecutor::showBackupFiles() {
	if(mState == NOT_AVAILABLE)
		return;
	if(mPlan->mBackupType == BackupPlan::BupType) {
		QStringList lArgs;
		lArgs << QStringLiteral("--title") << mPlan->mDescription;
		lArgs << mDestinationPath;
		KProcess::startDetached(QStringLiteral("kup-filedigger"), lArgs);
	} else if(mPlan->mBackupType == BackupPlan::RsyncType) {
		KRun::runUrl(QUrl::fromLocalFile(mDestinationPath), QStringLiteral("inode/directory"), nullptr);
	}
}

BackupJob *PlanExecutor::createBackupJob() {
	if(mPlan->mBackupType == BackupPlan::BupType) {
		return new BupJob(*mPlan, mDestinationPath, mLogFilePath, mKupDaemon);
	} else if(mPlan->mBackupType == BackupPlan::RsyncType) {
		return new RsyncJob(*mPlan, mDestinationPath, mLogFilePath, mKupDaemon);
	}
	qCWarning(KUPDAEMON) << "Invalid backup type in configuration!";
	return nullptr;
}

bool PlanExecutor::powerSaveActive() {
	QDBusMessage lMsg = QDBusMessage::createMethodCall(sPwrMgmtServiceName,
	                                                   sPwrMgmtPath,
	                                                   sPwrMgmtInterface,
	                                                   QStringLiteral("GetPowerSaveStatus"));
	QDBusReply<bool> lReply = QDBusConnection::sessionBus().call(lMsg);
	return lReply.value();
}

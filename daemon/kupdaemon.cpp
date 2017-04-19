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

#include "kupdaemon.h"
#include "kupsettings.h"
#include "backupplan.h"
#include "edexecutor.h"
#include "fsexecutor.h"

#include <QApplication>
#include <QDBusConnection>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QPushButton>
#include <QSessionManager>
#include <QTimer>

#include <KIdleTime>
#include <KLocalizedString>
#include <KUiServerJobTracker>

KupDaemon::KupDaemon() {
	mWaitingToReloadConfig = false;
	mConfig = KSharedConfig::openConfig(QStringLiteral("kuprc"));
	mSettings = new KupSettings(mConfig, this);
	mJobTracker = new KUiServerJobTracker(this);
	mLocalServer = new QLocalServer(this);
}

KupDaemon::~KupDaemon() {
	while(!mExecutors.isEmpty()) {
		delete mExecutors.takeFirst();
	}
	KIdleTime::instance()->removeAllIdleTimeouts();
}

bool KupDaemon::shouldStart() {
	return mSettings->mBackupsEnabled;
}

void KupDaemon::setupGuiStuff() {
	// timer to update logged time and also trigger warning if too long
	// time has now passed since last backup
	mUsageAccTimer = new QTimer(this);
	mUsageAccTimer->setInterval(KUP_USAGE_MONITOR_INTERVAL_S * 1000);
	mUsageAccTimer->start();
	KIdleTime *lIdleTime = KIdleTime::instance();
	lIdleTime->addIdleTimeout(KUP_IDLE_TIMEOUT_S * 1000);
	connect(lIdleTime, SIGNAL(timeoutReached(int)), mUsageAccTimer, SLOT(stop()));
	connect(lIdleTime, SIGNAL(timeoutReached(int)), lIdleTime, SLOT(catchNextResumeEvent()));
	connect(lIdleTime, SIGNAL(resumingFromIdle()), mUsageAccTimer, SLOT(start()));

	mStatusUpdateTimer = new QTimer(this);
	// delay status update to avoid sending a status to plasma applet
	// that will be changed again just a microsecond later anyway
	mStatusUpdateTimer->setInterval(500);
	mStatusUpdateTimer->setSingleShot(true);
	connect(mStatusUpdateTimer, &QTimer::timeout, [this]{
		foreach(QLocalSocket *lSocket, mSockets) {
			sendStatus(lSocket);
		}

		if(mWaitingToReloadConfig) {
			// quite likely the config can be reloaded now, give it a try.
			QTimer::singleShot(0, this, SLOT(reloadConfig()));
		}
	});

	QDBusConnection lDBus = QDBusConnection::sessionBus();
	if(lDBus.isConnected()) {
		if(lDBus.registerService(KUP_DBUS_SERVICE_NAME)) {
			lDBus.registerObject(KUP_DBUS_OBJECT_PATH, this, QDBusConnection::ExportAllSlots);
		}
	}
	QString lSocketName = QStringLiteral("kup-daemon-");
	lSocketName += QString::fromLocal8Bit(qgetenv("USER"));

	connect(mLocalServer, &QLocalServer::newConnection, [this]{
		QLocalSocket *lSocket = mLocalServer->nextPendingConnection();
		if(lSocket == nullptr) {
			return;
		}
		sendStatus(lSocket);
		mSockets.append(lSocket);
		connect(lSocket, &QLocalSocket::readyRead, [this,lSocket]{handleRequests(lSocket);});
		connect(lSocket, &QLocalSocket::disconnected, [this,lSocket]{
			mSockets.removeAll(lSocket);
			lSocket->deleteLater();
		});
	});
	// remove old socket first in case it's still there, otherwise listen() fails.
	QLocalServer::removeServer(lSocketName);
	mLocalServer->listen(lSocketName);

	reloadConfig();
}

void KupDaemon::reloadConfig() {
	foreach(PlanExecutor *lExecutor, mExecutors) {
		if(lExecutor->busy()) {
			mWaitingToReloadConfig = true;
			return;
		}
	}
	mWaitingToReloadConfig = false;

	mSettings->load();
	while(!mExecutors.isEmpty()) {
		delete mExecutors.takeFirst();
	}
	if(!mSettings->mBackupsEnabled)
		qApp->quit();

	setupExecutors();
	// Juuuust in case all those executors for some reason never
	// triggered an updated status... Doesn't hurt anyway.
	mStatusUpdateTimer->start();
}

// This method is exposed over DBus so that filedigger can call it
void KupDaemon::runIntegrityCheck(QString pPath) {
	foreach(PlanExecutor *lExecutor, mExecutors) {
		// if caller passes in an empty path, startsWith will return true and we will try to check
		// all backup plans.
		if(lExecutor->mDestinationPath.startsWith(pPath)) {
			lExecutor->startIntegrityCheck();
		}
	}
}

void KupDaemon::registerJob(KJob *pJob) {
	mJobTracker->registerJob(pJob);
}

void KupDaemon::unregisterJob(KJob *pJob) {
	mJobTracker->unregisterJob(pJob);
}

void KupDaemon::slotShutdownRequest(QSessionManager &pManager) {
	// this will make session management not try (and fail because of KDBusService starting only
	// one instance) to start this daemon. We have autostart for the purpose of launching this
	// daemon instead.
	pManager.setRestartHint(QSessionManager::RestartNever);


	foreach(PlanExecutor *lExecutor, mExecutors) {
		if(lExecutor->busy() && pManager.allowsErrorInteraction()) {
			QMessageBox lMessageBox;
			QPushButton *lContinueButton = lMessageBox.addButton(i18n("Continue"),
			                                                     QMessageBox::RejectRole);
			lMessageBox.addButton(i18n("Stop"), QMessageBox::AcceptRole);
			lMessageBox.setText(i18nc("%1 is a text explaining the current activity",
			                          "Currently busy: %1", lExecutor->currentActivityTitle()));
			lMessageBox.setInformativeText(i18n("Do you really want to stop?"));
			lMessageBox.setIcon(QMessageBox::Warning);
			lMessageBox.setWindowIcon(QIcon::fromTheme(QStringLiteral("kup")));
			lMessageBox.setWindowTitle(i18n("User Backups"));
			lMessageBox.exec();
			if(lMessageBox.clickedButton() == lContinueButton) {
				pManager.cancel();
			}
			return; //only ask for one active executor.
		}
	}
}

void KupDaemon::setupExecutors() {
	for(int i = 0; i < mSettings->mNumberOfPlans; ++i) {
		PlanExecutor *lExecutor;
		BackupPlan *lPlan = new BackupPlan(i+1, mConfig, this);
		if(lPlan->mPathsIncluded.isEmpty()) {
			delete lPlan;
			continue;
		}
		if(lPlan->mDestinationType == 0) {
			lExecutor = new FSExecutor(lPlan, this);
		} else if(lPlan->mDestinationType == 1) {
			lExecutor = new EDExecutor(lPlan, this);
		} else {
			delete lPlan;
			continue;
		}
		connect(lExecutor, &PlanExecutor::stateChanged, [&]{mStatusUpdateTimer->start();});
		connect(lExecutor, &PlanExecutor::backupStatusChanged, [&]{mStatusUpdateTimer->start();});
		connect(mUsageAccTimer, &QTimer::timeout,
		        lExecutor, &PlanExecutor::updateAccumulatedUsageTime);
		lExecutor->checkStatus();
		mExecutors.append(lExecutor);
	}
}

void KupDaemon::handleRequests(QLocalSocket *pSocket) {
	if(pSocket->bytesAvailable() <= 0) {
		return;
	}
	QJsonDocument lDoc = QJsonDocument::fromBinaryData(pSocket->readAll());
	if(!lDoc.isObject()) {
		return;
	}
	QJsonObject lCommand = lDoc.object();
	QString lOperation = lCommand["operation name"].toString();
	if(lOperation == QStringLiteral("get status")) {
		sendStatus(pSocket);
		return;
	}

	int lPlanNumber = lCommand["plan number"].toInt(-1);
	if(lPlanNumber < 0 || lPlanNumber >= mExecutors.count()) {
		return;
	}
	if(lOperation == QStringLiteral("save backup")) {
		mExecutors.at(lPlanNumber)->startBackupSaveJob();
	}
	if(lOperation == QStringLiteral("show log file")) {
		mExecutors.at(lPlanNumber)->showLog();
	}
	if(lOperation == QStringLiteral("show backup files")) {
		mExecutors.at(lPlanNumber)->showBackupFiles();
	}
}

void KupDaemon::sendStatus(QLocalSocket *pSocket) {
	bool lTrayIconActive = false;
	bool lAnyPlanBusy = false;
	// If all backup plans have status == NO_STATUS then tooltip title will be empty
	QString lToolTipTitle;
	QString lToolTipSubTitle = i18nc("status in tooltip", "Backup destination not available");
	QString lToolTipIconName = QStringLiteral("kup");

	if(mExecutors.isEmpty()) {
		lToolTipTitle = i18n("No backup plans configured");
		lToolTipSubTitle.clear();
	}

	foreach(PlanExecutor *lExec, mExecutors) {
		if(lExec->destinationAvailable()) {
			lToolTipSubTitle = i18nc("status in tooltip", "Backup destination available");
			if(lExec->scheduleType() == BackupPlan::MANUAL) {
				lTrayIconActive = true;
			}
		}
	}
	foreach(PlanExecutor *lExec, mExecutors) {
		if(lExec->mPlan->backupStatus() == BackupPlan::GOOD) {
			lToolTipIconName = BackupPlan::iconName(BackupPlan::GOOD);
			lToolTipTitle = i18nc("status in tooltip", "Backup status OK");
		}
	}

	foreach(PlanExecutor *lExec, mExecutors) {
		if(lExec->mPlan->backupStatus() == BackupPlan::MEDIUM) {
			lToolTipIconName = BackupPlan::iconName(BackupPlan::MEDIUM);
			lToolTipTitle = i18nc("status in tooltip", "New backup suggested");
		}
	}

	foreach(PlanExecutor *lExec, mExecutors) {
		if(lExec->mPlan->backupStatus() == BackupPlan::BAD) {
			lToolTipIconName = BackupPlan::iconName(BackupPlan::BAD);
			lToolTipTitle = i18nc("status in tooltip", "New backup neeeded");
			lTrayIconActive = true;
		}
	}
	foreach(PlanExecutor *lExecutor, mExecutors) {
		if(lExecutor->busy()) {
			lToolTipIconName = QStringLiteral("kup");
			lToolTipTitle = lExecutor->currentActivityTitle();
			lToolTipSubTitle = lExecutor->mPlan->mDescription;
			lAnyPlanBusy = true;
		}
	}

	if(lToolTipTitle.isEmpty() && !lToolTipSubTitle.isEmpty()) {
		lToolTipTitle = lToolTipSubTitle;
		lToolTipSubTitle.clear();
	}

	QJsonObject lStatus;
	lStatus["event"] = QStringLiteral("status update");
	lStatus["tray icon active"] = lTrayIconActive;
	lStatus["tooltip icon name"] = lToolTipIconName;
	lStatus["tooltip title"] = lToolTipTitle;
	lStatus["tooltip subtitle"] = lToolTipSubTitle;
	lStatus["any plan busy"] = lAnyPlanBusy;
	lStatus["no plan reason"] = mExecutors.isEmpty()
	      ? i18n("No backup plans configured")
	       : QStringLiteral("");
	QJsonArray lPlans;
	foreach(PlanExecutor *lExecutor, mExecutors) {
		QJsonObject lPlan;
		lPlan[QStringLiteral("description")] = lExecutor->mPlan->mDescription;
		lPlan[QStringLiteral("destination available")] = lExecutor->destinationAvailable();
		lPlan[QStringLiteral("status heading")] = lExecutor->currentActivityTitle();
		lPlan[QStringLiteral("status details")] = lExecutor->mPlan->statusText();
		lPlan[QStringLiteral("icon name")] = BackupPlan::iconName(lExecutor->mPlan->backupStatus());
		lPlan[QStringLiteral("log file exists")] = QFileInfo(lExecutor->mLogFilePath).exists();
		lPlan[QStringLiteral("busy")] = lExecutor->busy();
		lPlans.append(lPlan);
	}
	lStatus["plans"] = lPlans;
	QJsonDocument lDoc(lStatus);
	pSocket->write(lDoc.toBinaryData());
}

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
#include <QMenu>
#include <QSessionManager>
#include <QTimer>

#include <KIdleTime>
#include <KLocalizedString>
#include <KRun>
#include <KServiceTypeTrader>
#include <KStandardAction>
#include <KStatusNotifierItem>

KupDaemon::KupDaemon() {
	mWaitingToReloadConfig = false;
	mConfig = KSharedConfig::openConfig(QLatin1String("kuprc"));
	mSettings = new KupSettings(mConfig, this);
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
	mUsageAccumulatorTimer = new QTimer(this);
	mUsageAccumulatorTimer->setInterval(KUP_USAGE_MONITOR_INTERVAL_S * 1000);
	mUsageAccumulatorTimer->start();
	KIdleTime::instance()->addIdleTimeout(KUP_IDLE_TIMEOUT_S * 1000);
	connect(KIdleTime::instance(), SIGNAL(timeoutReached(int)), mUsageAccumulatorTimer, SLOT(stop()));
	connect(KIdleTime::instance(), SIGNAL(timeoutReached(int)), KIdleTime::instance(), SLOT(catchNextResumeEvent()));
	connect(KIdleTime::instance(), SIGNAL(resumingFromIdle()), mUsageAccumulatorTimer, SLOT(start()));

	setupTrayIcon();
	setupExecutors();
	setupContextMenu();
	updateTrayIcon();

	QDBusConnection lDBus = QDBusConnection::sessionBus();
	if(lDBus.isConnected()) {
		if(lDBus.registerService(KUP_DBUS_SERVICE_NAME)) {
			lDBus.registerObject(KUP_DBUS_OBJECT_PATH, this, QDBusConnection::ExportAllSlots);
		}
	}
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
	setupContextMenu();
	updateTrayIcon();
}

void KupDaemon::showConfig() {
	KService::List lServices = KServiceTypeTrader::self()->query(QLatin1String("KCModule"), QLatin1String("Library == 'kcm_kup'"));
	if (!lServices.isEmpty()) {
		KService::Ptr lService = lServices.first();
		KRun::runService(*lService, QList<QUrl>(), 0);
	}
}

void KupDaemon::updateTrayIcon() {
	KStatusNotifierItem::ItemStatus lStatus = KStatusNotifierItem::Passive;
	QString lIconName = QLatin1String("kup");
	QString lToolTipTitle = i18nc("@info:tooltip", "Backup destination unavailable");
	QString lToolTipSubTitle = i18nc("@info:tooltip", "Backup status OK");
	QString lToolTipIconName = BackupPlan::iconName(BackupPlan::GOOD);

	foreach(PlanExecutor *lExec, mExecutors) {
		if(lExec->mState != PlanExecutor::NOT_AVAILABLE) {
			lStatus = KStatusNotifierItem::Active;
			lToolTipTitle = i18nc("@info:tooltip", "Backup destination available");
		}
	}

	foreach(PlanExecutor *lExec, mExecutors) {
		if(lExec->mPlan->backupStatus() == BackupPlan::MEDIUM) {
			lToolTipIconName = BackupPlan::iconName(BackupPlan::MEDIUM);
			lToolTipSubTitle = i18nc("@info:tooltip", "New backup suggested");
		}
	}

	foreach(PlanExecutor *lExec, mExecutors) {
		if(lExec->mPlan->backupStatus() == BackupPlan::BAD) {
			if(lExec->scheduleType() != BackupPlan::MANUAL) {
				lStatus = KStatusNotifierItem::Active;
			}
			lIconName = BackupPlan::iconName(BackupPlan::BAD);
			lToolTipIconName = BackupPlan::iconName(BackupPlan::BAD);
			lToolTipSubTitle = i18nc("@info:tooltip", "New backup neeeded");
		}
	}
	foreach(PlanExecutor *lExecutor, mExecutors) {
		if(lExecutor->busy()) {
			lStatus = KStatusNotifierItem::NeedsAttention;
			lToolTipIconName = QLatin1String("kup");
			lToolTipTitle = lExecutor->currentActivityTitle();
			lToolTipSubTitle = lExecutor->mPlan->mDescription; // TODO: show percentage etc.
		}
	}
	mStatusNotifier->setStatus(lStatus);
	mStatusNotifier->setIconByName(lIconName);
	mStatusNotifier->setToolTipIconByName(lToolTipIconName);
	mStatusNotifier->setToolTipTitle(lToolTipTitle);
	mStatusNotifier->setToolTipSubTitle(lToolTipSubTitle);

	if(mWaitingToReloadConfig) {
		// quite likely the config can be reloaded now, give it a try.
		QTimer::singleShot(0, this, SLOT(reloadConfig()));
	}
}

void KupDaemon::runIntegrityCheck(QString pPath) {
	foreach(PlanExecutor *lExecutor, mExecutors) {
		// if caller passes in an empty path, startsWith will return true and we will try to check all backup plans.
		if(lExecutor->mDestinationPath.startsWith(pPath)) {
			lExecutor->startIntegrityCheck();
		}
	}
}

void KupDaemon::disableSessionManagement(QSessionManager &pManager) {
	pManager.setRestartHint(QSessionManager::RestartNever);
}

void KupDaemon::setupExecutors() {
	for(int i = 0; i < mSettings->mNumberOfPlans; ++i) {
		PlanExecutor *lExecutor;
		BackupPlan *lPlan = new BackupPlan(i+1, mConfig, this);
		if(lPlan->mDestinationType == 0) {
			lExecutor = new FSExecutor(lPlan, this);
		} else if(lPlan->mDestinationType == 1) {
			lExecutor = new EDExecutor(lPlan, this);
		} else {
			delete lPlan;
			continue;
		}
		//... add other types here
		mExecutors.append(lExecutor);
	}
	foreach(PlanExecutor *lExecutor, mExecutors) {
		lExecutor->checkStatus(); //connect after to trigger less updates here, do one check after instead.
		connect(lExecutor, SIGNAL(stateChanged()), SLOT(updateTrayIcon()));
		connect(lExecutor, SIGNAL(backupStatusChanged()), SLOT(updateTrayIcon()));
		connect(mUsageAccumulatorTimer, SIGNAL(timeout()), lExecutor, SLOT(updateAccumulatedUsageTime()));
	}
}

void KupDaemon::setupTrayIcon() {
	mStatusNotifier = new KStatusNotifierItem(this);
	mStatusNotifier->setCategory(KStatusNotifierItem::SystemServices);
	mStatusNotifier->setStandardActionsEnabled(false);
	mStatusNotifier->setTitle(i18nc("@title:window", "Backups"));
	mStatusNotifier->setAttentionMovieByName(QLatin1String("kuprunning"));
}

void KupDaemon::setupContextMenu() {
	mContextMenu = new QMenu(i18nc("@title:menu", "Backups"));
	mContextMenu->addAction(i18nc("@action:inmenu", "Configure Backups"), this, SLOT(showConfig()));
	foreach(PlanExecutor *lExec, mExecutors) {
		mContextMenu->addMenu(lExec->mActionMenu);
	}
	mStatusNotifier->setContextMenu(mContextMenu);
	mStatusNotifier->setAssociatedWidget(mContextMenu);
}


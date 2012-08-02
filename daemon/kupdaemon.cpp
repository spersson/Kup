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

#include <QDBusConnection>
#include <QTimer>

#include <KIdleTime>
#include <KLocale>
#include <KMenu>
#include <KRun>
#include <KServiceTypeTrader>
#include <KStandardAction>
#include <KStatusNotifierItem>
#include <KUniqueApplication>

KupDaemon::KupDaemon() {
	mConfig = KSharedConfig::openConfig("kuprc");
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
	mSettings->readConfig();
	while(!mExecutors.isEmpty()) {
		delete mExecutors.takeFirst();
	}
	if(!mSettings->mBackupsEnabled)
		kapp->quit();

	setupExecutors();
	setupContextMenu();
	updateTrayIcon();
}

void KupDaemon::showConfig() {
	KService::List lServices = KServiceTypeTrader::self()->query("KCModule", "Library == 'kcm_kup'");
	if (!lServices.isEmpty()) {
		KService::Ptr lService = lServices.first();
		KRun::run(*lService, KUrl::List(), 0);
	}
}

void KupDaemon::updateTrayIcon() {
	KStatusNotifierItem::ItemStatus lStatus = KStatusNotifierItem::Passive;
	QString lIconName = QLatin1String("kup");
	QString lToolTipTitle = i18n("Backup destination unavailable");
	QString lToolTipSubTitle = i18n("Backup status OK");
	QString lToolTipIconName = BackupPlan::iconName(BackupPlan::GOOD);

	foreach(PlanExecutor *lExec, mExecutors) {
		if(lExec->destinationAvailable()) {
			lStatus = KStatusNotifierItem::Active;
			lToolTipTitle = i18n("Backup destination available");
		}
	}

	foreach(PlanExecutor *lExec, mExecutors) {
		if(lExec->planStatus() == BackupPlan::MEDIUM) {
			lToolTipIconName = BackupPlan::iconName(BackupPlan::MEDIUM);
			lToolTipSubTitle = i18n("New backup suggested");
		}
	}

	foreach(PlanExecutor *lExec, mExecutors) {
		if(lExec->planStatus() == BackupPlan::BAD) {
			if(lExec->scheduleType() != BackupPlan::MANUAL) {
				lStatus = KStatusNotifierItem::Active;
			}
			lIconName = BackupPlan::iconName(BackupPlan::BAD);
			lToolTipIconName = BackupPlan::iconName(BackupPlan::BAD);
			lToolTipSubTitle = i18n("New backup neeeded");
		}
	}
	foreach(PlanExecutor *lExec, mExecutors) {
		if(lExec->running()) {
			lStatus = KStatusNotifierItem::NeedsAttention;
			lToolTipIconName = QLatin1String("kup");
			lToolTipTitle = i18n("Taking new backup");
			lToolTipSubTitle = lExec->description(); // TODO: show percentage etc.
		}
	}
	mStatusNotifier->setStatus(lStatus);
	mStatusNotifier->setIconByName(lIconName);
	mStatusNotifier->setToolTipIconByName(lToolTipIconName);
	mStatusNotifier->setToolTipTitle(lToolTipTitle);
	mStatusNotifier->setToolTipSubTitle(lToolTipSubTitle);
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
	mStatusNotifier->setTitle(i18n("Backups"));
	mStatusNotifier->setAttentionMovieByName(QLatin1String("kuprunning"));
}

void KupDaemon::setupContextMenu() {
	mContextMenu = new KMenu(i18n("Backups"));
	mContextMenu->addAction(i18n("Configure Backups"), this, SLOT(showConfig()));
	foreach(PlanExecutor *lExec, mExecutors) {
		mContextMenu->addMenu(lExec->planActions());
	}
	mStatusNotifier->setContextMenu(mContextMenu);
	mStatusNotifier->setAssociatedWidget(mContextMenu);
}


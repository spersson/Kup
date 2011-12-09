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

#include "kupdaemon.h"
#include "kupsettings.h"
#include "backupplan.h"
#include "edexecutor.h"
#include "fsexecutor.h"

#include <QDBusConnection>

#include <KLocale>
#include <KMenu>
#include <KRun>
#include <KServiceTypeTrader>
#include <KStandardAction>
#include <KStatusNotifierItem>
#include <KUniqueApplication>

KupDaemon::KupDaemon()
{
	mConfig = KSharedConfig::openConfig("kuprc");
	mSettings = new KupSettings(mConfig, this);
}

KupDaemon::~KupDaemon() {
	QDBusConnection lDBus = QDBusConnection::sessionBus();
	lDBus.disconnect(QString(), KUP_DBUS_OBJECT_PATH, KUP_DBUS_INTERFACE_NAME,
	                 KUP_DBUS_RELOAD_CONFIG_MESSAGE, this, SLOT(reloadConfig()));
	while(!mExecutors.isEmpty()) {
		delete mExecutors.takeFirst();
	}
}

bool KupDaemon::shouldStart() {
	return mSettings->mBackupsEnabled;
}

void KupDaemon::setupGuiStuff() {
	setupTrayIcon();
	setupExecutors();
	setupContextMenu();
	updateStatusNotifier();

	QDBusConnection lDBus = QDBusConnection::sessionBus();
	lDBus.connect(QString(), KUP_DBUS_OBJECT_PATH, KUP_DBUS_INTERFACE_NAME,
	             KUP_DBUS_RELOAD_CONFIG_MESSAGE, this, SLOT(reloadConfig()));
}

void KupDaemon::requestQuit() {
	kapp->quit();
}

void KupDaemon::reloadConfig() {
	mSettings->readConfig();
//	delete mStatusNotifier;
//	delete mContextMenu;
	while(!mExecutors.isEmpty()) {
		delete mExecutors.takeFirst();
	}
	if(!mSettings->mBackupsEnabled)
		kapp->quit();

//	setupTrayIcon();
	setupExecutors();
	setupContextMenu();
	updateStatusNotifier();
}

void KupDaemon::showConfig() {
	KService::List lServices = KServiceTypeTrader::self()->query("KCModule", "Library == 'kcm_kup'");
	if (!lServices.isEmpty()) {
		KService::Ptr lService = lServices.first();
		KRun::run(*lService, KUrl::List(), 0);
	}
}

void KupDaemon::updateStatusNotifier() {
	mStatusNotifier->setStatus(KStatusNotifierItem::Passive);
	foreach(PlanExecutor *lExec, mExecutors) {
		if(lExec->destinationAvailable()) {
			mStatusNotifier->setStatus(KStatusNotifierItem::Active);
		}
	}
//	foreach(PlanExecutor *lExec, mExecutors) {
//		if(lExec->running())
//			mStatusNotifier->setStatus(KStatusNotifierItem::NeedsAttention);
//	}
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
		connect(lExecutor, SIGNAL(statusUpdated()), this, SLOT(updateStatusNotifier()));
	}
}

void KupDaemon::setupTrayIcon() {
	mStatusNotifier = new KStatusNotifierItem(this);
	mStatusNotifier->setTitle(i18n("Backups"));
	mStatusNotifier->setIconByName(QLatin1String("drive-removable-media"));
	mStatusNotifier->setCategory(KStatusNotifierItem::SystemServices);
	mStatusNotifier->setToolTipIconByName(QLatin1String("drive-removable-media")); //TODO: show status emblem, how recently run
	mStatusNotifier->setToolTipTitle(i18n("Backups"));
	mStatusNotifier->setStatus(KStatusNotifierItem::Passive);
	mStatusNotifier->setStandardActionsEnabled(false);
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


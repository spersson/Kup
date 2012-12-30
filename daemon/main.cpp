
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

#include <KStartupInfo>
#include <KUniqueApplication>
#include <KAboutData>
#include <KCmdLineArgs>

#include <QDebug>

static const char description[] = I18N_NOOP("Kup is a flexible backup solution using the backup storage system 'bup'. "
                                            "This allows it to quickly perform incrementel backups, only saving the "
                                            "parts of files that has actually changed since last backup was taken.");

static const char version[] = "0.1";

int main(int argc, char **argv) {
	KupDaemon *lDaemon = new KupDaemon();
	if(!lDaemon->shouldStart()) {
		qWarning() <<ki18n("Kup is not enabled, enable it from the system settings module.").toString();
		return 0;
	}
	KAboutData lAbout("kupdaemon", "kup", ki18nc("@title", "Kup Daemon"), version, ki18n(description),
	                  KAboutData::License_GPL, ki18n("Copyright (C) 2011 Simon Persson"),
	                  KLocalizedString(), 0, "simonpersson1@gmail.com");
	lAbout.addAuthor(ki18n("Simon Persson"), KLocalizedString(), "simonpersson1@gmail.com");
	KCmdLineArgs::init(argc, argv, &lAbout);

	KUniqueApplication::addCmdLineOptions();
	if (!KUniqueApplication::start()) {
		qWarning() <<ki18n("Kup is already running!").toString();
		return 0;
	}
	KUniqueApplication lApp;

	// Use for debugging...
	//		KApplication lApp;

	lApp.setQuitOnLastWindowClosed(false);
	lApp.disableSessionManagement();

	KStartupInfo::appStarted(); //make startup notification go away.

	lDaemon->setupGuiStuff();

	return lApp.exec();
}

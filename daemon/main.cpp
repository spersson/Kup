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

#include <KAboutData>
#include <KDBusService>
#include <KLocalizedString>
#include <KStartupInfo>

#include <QApplication>
#include <QDebug>
#include <QCommandLineParser>

extern "C" int Q_DECL_EXPORT kdemain(int argc, char *argv[]) {
	QApplication lApp(argc, argv);
	lApp.setQuitOnLastWindowClosed(false);
	lApp.setAttribute(Qt::AA_UseHighDpiPixmaps, true);

	KLocalizedString::setApplicationDomain("kup");

	KupDaemon *lDaemon = new KupDaemon();
	if(!lDaemon->shouldStart()) {
		qCritical("%s", xi18nc("@info:shell Error message at startup",
		                       "Kup is not enabled, enable it from the "
		                       "system settings module. You can do that by running "
		                       "<command>kcmshell5 kup</command>").toLocal8Bit().constData());
		return 0;
	}

	KAboutData lAbout(QStringLiteral("kupdaemon"), xi18nc("@title", "Kup Daemon"), QStringLiteral("0.5.1"),
	                  i18n("Kup is a flexible backup solution using the backup storage system 'bup'. "
	                       "This allows it to quickly perform incremental backups, only saving the "
	                       "parts of files that has actually changed since last backup was taken."),
	                  KAboutLicense::GPL, i18n("Copyright (C) 2011-2015 Simon Persson"),
	                  QString(), QString(), "simonpersson1@gmail.com");
	lAbout.addAuthor(i18n("Simon Persson"), QString(), "simonpersson1@gmail.com");
	lAbout.setTranslator(xi18nc("NAME OF TRANSLATORS", "Your names"), xi18nc("EMAIL OF TRANSLATORS", "Your emails"));
	KAboutData::setApplicationData(lAbout);

	QCommandLineParser lParser;
	lParser.addVersionOption();
	lParser.addHelpOption();
	lAbout.setupCommandLine(&lParser);
	lParser.process(lApp);
	lAbout.processCommandLine(&lParser);

	// This call will exit() if an instance is already running
	KDBusService lService(KDBusService::Unique);

	KStartupInfo::appStarted(); //make startup notification go away.

	lDaemon->setupGuiStuff();

	// these calls will make session management not try (and fail because of KDBusService) to start
	// this daemon. We have autostart for the purpose of launching this daemon instead.
	lDaemon->connect(&lApp, SIGNAL(commitDataRequest(QSessionManager&)), lDaemon,
	                 SLOT(disableSessionManagement(QSessionManager&)));
	lDaemon->connect(&lApp, SIGNAL(saveStateRequest(QSessionManager&)), lDaemon,
	                 SLOT(disableSessionManagement(QSessionManager&)));

	return lApp.exec();
}

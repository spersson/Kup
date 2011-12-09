
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

#include "kuprunner.h"
#include "bupjob.h"

#include <KApplication>
#include <KAboutData>
#include <KCmdLineArgs>

static const char description[] = I18N_NOOP("Kup is a flexible backup solution using the backup storage system 'bup'. "
                                            "This allows it to quickly perform incrementel backups, only saving the "
                                            "parts of files that has actually changed since last backup was taken.");

static const char version[] = "0.1";

int main(int argc, char **argv) {
	KAboutData lAbout("kuprunner", "kup", ki18n("Kup Runner"), version, ki18n(description),
	                  KAboutData::License_GPL, ki18n("Copyright (C) 2011 Simon Persson"),
	                  KLocalizedString(), 0, "simonop@spray.se");
	lAbout.addAuthor( ki18n("Simon Persson"), KLocalizedString(), "simonop@spray.se" );
	KCmdLineArgs::init(argc, argv, &lAbout);

	KApplication lApp;

	lApp.setQuitOnLastWindowClosed(false);
	lApp.disableSessionManagement();

//	new KupRunner();


	return lApp.exec();
}

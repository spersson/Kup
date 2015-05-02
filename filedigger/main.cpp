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

#include "filedigger.h"
#include "mergedvfs.h"

#include <git2/threads.h>

#include <KAboutData>
#include <KLocalizedString>
#include <KMessageBox>

#include <QApplication>
#include <QDebug>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFile>
#include <QTextStream>

int main(int pArgCount, char **pArgArray) {
	QApplication lApp(pArgCount, pArgArray);
	lApp.setAttribute(Qt::AA_UseHighDpiPixmaps, true);

	KLocalizedString::setApplicationDomain("kup");

	KAboutData lAbout(QStringLiteral("kupfiledigger"), i18nc("@title", "File Digger"), QStringLiteral("0.5.1"),
	                  i18n("Browser for bup archives."),
	                  KAboutLicense::GPL, i18n("Copyright (C) 2013-2015 Simon Persson"),
	                  QString(), QString(), "simonpersson1@gmail.com");
	lAbout.addAuthor(i18n("Simon Persson"), QString(), "simonpersson1@gmail.com");
	lAbout.setTranslator(i18nc("NAME OF TRANSLATORS", "Your names"), i18nc("EMAIL OF TRANSLATORS", "Your emails"));
	KAboutData::setApplicationData(lAbout); //this calls qApp.setApplicationName, setVersion, etc.

	QCommandLineParser lParser;
	lParser.addVersionOption();
	lParser.addHelpOption();
	lParser.addOption(QCommandLineOption(QStringList() << QStringLiteral("b") << QStringLiteral("branch"),
	                                     i18n("Name of the branch to be opened."),
	                                     QStringLiteral("branch name"), QStringLiteral("kup")));
	lParser.addPositionalArgument(QStringLiteral("<repository path>"), i18n("Path to the bup repository to be opened."));

	lAbout.setupCommandLine(&lParser);
	lParser.process(lApp);
	lAbout.processCommandLine(&lParser);

	if(lParser.positionalArguments().count() != 1) {
		qCritical() << i18nc("Error message at startup",
		                     "You must supply the path to a bup or git repository that "
		                     "you wish to open for viewing.");
		return -1;
	}

	// This needs to be called first thing, before any other calls to libgit2.
	git_threads_init();
	MergedRepository *lRepository = new MergedRepository(NULL, lParser.positionalArguments().first(),
	                                                     lParser.value("branch"));
	if(!lRepository->open()) {
		KMessageBox::sorry(NULL, i18nc("@info:label messagebox, %1 is a folder path",
		                               "The backup archive \"%1\" could not be opened. Check if the backups really are located there.",
		                               lParser.positionalArguments().first()));
		return 1;
	}
	if(!lRepository->readBranch()) {
		if(!lRepository->permissionsOk()) {
			KMessageBox::sorry(NULL, i18nc("@info:label messagebox",
			                               "You do not have permission needed to read this backup archive."));
			return 2;
		} else {
			lRepository->askForIntegrityCheck();
			return 3;
		}
	}

	FileDigger *lFileDigger = new FileDigger(lRepository);
	lFileDigger->show();
	int lRetVal = lApp.exec();
	delete lRepository;
	git_threads_shutdown();
	return lRetVal;
}

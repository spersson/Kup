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
#include "kupfiledigger_debug.h"

#if LIBGIT2_VER_MAJOR == 0 && LIBGIT2_VER_MINOR >= 24
#include <git2/global.h>
#else
#include <git2/threads.h>
#endif

#include <KAboutData>
#include <KLocalizedString>

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

	KAboutData lAbout(QStringLiteral("kupfiledigger"), xi18nc("@title", "File Digger"), QStringLiteral("0.7.3"),
	                  i18n("Browser for bup archives."),
	                  KAboutLicense::GPL, i18n("Copyright (C) 2013-2015 Simon Persson"),
	                  QString(), QString(), "simonpersson1@gmail.com");
	lAbout.addAuthor(i18n("Simon Persson"), QString(), "simonpersson1@gmail.com");
	lAbout.setTranslator(xi18nc("NAME OF TRANSLATORS", "Your names"), xi18nc("EMAIL OF TRANSLATORS", "Your emails"));
	KAboutData::setApplicationData(lAbout); //this calls qApp.setApplicationName, setVersion, etc.

	QCommandLineParser lParser;
	lParser.addVersionOption();
	lParser.addHelpOption();
	lParser.addOption(QCommandLineOption(QStringList() << QStringLiteral("b") << QStringLiteral("branch"),
	                                     i18n("Name of the branch to be opened."),
	                                     QStringLiteral("branch name"), QStringLiteral("kup")));
	lParser.addOption(QCommandLineOption(QStringList() << QStringLiteral("t"),
										 i18n("Type of backup (bup, restic)."),
										 QStringLiteral("type of backup"), QStringLiteral("kup")));
	lParser.addPositionalArgument(QStringLiteral("<repository path>"), i18n("Path to the bup repository to be opened."));

	lAbout.setupCommandLine(&lParser);
	lParser.process(lApp);
	lAbout.processCommandLine(&lParser);

	QString lRepoPath;
	if(!lParser.positionalArguments().isEmpty()) {
		lRepoPath = lParser.positionalArguments().first();
	}

	// This needs to be called first thing, before any other calls to libgit2.
	#if LIBGIT2_VER_MAJOR == 0 && LIBGIT2_VER_MINOR >= 24
	git_libgit2_init();
	#else
	git_threads_init();
	#endif

	BackupType type;
	QString typeString = lParser.value(QStringLiteral("t"));
	if (typeString == QStringLiteral("restic"))
		type = BackupType::B_T_RESTIC;
	else if (typeString == QStringLiteral("bup"))
		type = BackupType::B_T_BUP;
	else {
		qCWarning(KUPFILEDIGGER) << "Please specify a known backup type";
		return 1;
	}

	FileDigger *lFileDigger = new FileDigger(type, lRepoPath, lParser.value(QStringLiteral("branch")));
	lFileDigger->show();
	int lRetVal = lApp.exec();
	#if LIBGIT2_VER_MAJOR == 0 && LIBGIT2_VER_MINOR >= 24
	git_libgit2_shutdown();
	#else
	git_threads_shutdown();
	#endif
	return lRetVal;
}

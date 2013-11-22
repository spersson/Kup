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

#include <QFile>
#include <QTextStream>
#include <KAboutData>
#include <KApplication>
#include <KCmdLineArgs>
#include <KMessageBox>

static const char version[] = "0.4";
static const char description[] = I18N_NOOP("Browser for bup archives.");

int main(int pArgCount, char **pArgArray) {
	KAboutData lAbout("filedigger", 0, ki18n("File Digger"), version, ki18n(description),
	                  KAboutData::License_GPL, ki18n("Copyright (C) 2013 Simon Persson"),
	                  KLocalizedString(), 0, "simonpersson1@gmail.com");
	lAbout.addAuthor(ki18n("Simon Persson"), KLocalizedString(), "simonpersson1@gmail.com");
	lAbout.setTranslator(ki18nc("NAME OF TRANSLATORS", "Your names"), ki18nc("EMAIL OF TRANSLATORS", "Your emails"));
	KCmdLineArgs::init(pArgCount, pArgArray, &lAbout);

	KCmdLineOptions lOptions;
	lOptions.add("b").add("branch <branch name>", ki18n("Name of the branch to be opened."), "kup");
	lOptions.add("+<repository path>", ki18n("Path to the bup repository to be opened."));
	KCmdLineArgs::addCmdLineOptions(lOptions);

	KApplication lApp;
	KCmdLineArgs *lParsedArguments = KCmdLineArgs::parsedArgs();
	if(lParsedArguments->count() != 1) {
		KCmdLineArgs::usageError(ki18nc("Error message at startup",
		                                "You must supply the path to a bup or git repository that "
		                                "you wish to open for viewing.").toString());
	}

	// This needs to be called first thing, before any other calls to libgit2.
	git_threads_init();
	MergedRepository *lRepository = new MergedRepository(NULL, lParsedArguments->arg(0),
	                                                     lParsedArguments->getOption("branch"));
	if(!lRepository->openedSuccessfully()) {
		KMessageBox::sorry(NULL, i18nc("@info:label messagebox", "The specified bup repository could not be opened."));
		return -1;
	}

	FileDigger *lFileDigger = new FileDigger(lRepository);
	lFileDigger->show();
	int lRetVal = lApp.exec();
	delete lRepository;
	git_threads_shutdown();
	return lRetVal;
}

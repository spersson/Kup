
#include "filedigger.h"

#include <QFile>
#include <QTextStream>
#include <KAboutData>
#include <KApplication>
#include <KCmdLineArgs>

static const char version[] = "0.4";
static const char description[] = I18N_NOOP("Browser for bup archives.");

int main(int pArgCount, char **pArgArray) {
	KAboutData lAbout("filedigger", 0, ki18n("File Digger"), version, ki18n(description),
	                  KAboutData::License_GPL, ki18n("(C) 2013 Simon Persson"),
	                  KLocalizedString(), 0, "simonpersson1@gmail.com");
	lAbout.addAuthor( ki18n("Simon Persson"), KLocalizedString(), "simonpersson1@gmail.com" );
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
	FileDigger *lFileDigger = new FileDigger(lParsedArguments->arg(0),
	                                         lParsedArguments->getOption("branch"));
	lFileDigger->show();
	return lApp.exec();
}

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
#include "kupsettings.h"
#include "backupplan.h"

#include <KLocale>
#include <KStandardAction>
#include <KUniqueApplication>


KupRunner::KupRunner()
{
	readSettings();
}

void KupRunner::readSettings() {
	mConfig = KSharedConfig::openConfig("kuprc");
	mSettings = new KupSettings(mConfig, this);
	if(!mSettings->mBackupsEnabled)
		kapp->quit();

	for(int i = 0; i < mSettings->mNumberOfPlans; ++i) {
//		PlanExecutor *lExecutor;
//		BackupPlan *lPlan = new BackupPlan(i+1, mConfig, this);
//		if(lPlan->mDestinationType == 2)
//			lExecutor = new EDExecutor(lPlan, this);
//		//... add other types here
//		mExecutors.append(lExecutor);
	}

}

void KupRunner::requestQuit() {
	kapp->quit();
}



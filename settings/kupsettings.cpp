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

#include "kupsettings.h"
#include "backupplan.h"

KupSettings::KupSettings(KSharedConfigPtr pConfig, QObject *pParent)
   : KCoreConfigSkeleton(pConfig, pParent)
{
	setCurrentGroup(QLatin1String("Kup settings"));
	addItemBool(QLatin1String("Backups enabled"), mBackupsEnabled);
	addItemInt(QLatin1String("Number of backups"), mNumberOfPlans, 0);
}



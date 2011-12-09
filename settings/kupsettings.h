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

#ifndef KUPSETTINGS_H
#define KUPSETTINGS_H

#include <KConfigSkeleton>
#include <KSharedConfig>

class KupSettings : public KCoreConfigSkeleton
{
	Q_OBJECT
public:
	explicit KupSettings(KSharedConfigPtr pConfig, QObject *pParent = 0);

	// Common enable of backup plans
	bool mBackupsEnabled;

	// Number of backup plans configured
	int mNumberOfPlans;
};

#endif // KUPSETTINGS_H

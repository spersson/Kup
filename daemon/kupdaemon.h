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

#ifndef KUPDAEMON_H
#define KUPDAEMON_H

#include <KSharedConfig>

#define KUP_DBUS_INTERFACE_NAME QLatin1String("org.kde.kupdaemon")
#define KUP_DBUS_OBJECT_PATH QLatin1String("/Configuration")
#define KUP_DBUS_RELOAD_CONFIG_MESSAGE QLatin1String("reloadConfig")


class KupSettings;
class PlanExecutor;

class KMenu;
class KStatusNotifierItem;

class KupDaemon : public QObject
{
	Q_OBJECT

public:
	KupDaemon();
	virtual ~KupDaemon();
	bool shouldStart();
	void setupGuiStuff();

public slots:
	void requestQuit();
	void reloadConfig();
	void showConfig();
	void updateStatusNotifier();

private:
	void setupExecutors();
	void setupTrayIcon();
	void setupContextMenu();

	KSharedConfigPtr mConfig;
	KupSettings *mSettings;
	QList<PlanExecutor *> mExecutors;
	KStatusNotifierItem *mStatusNotifier;
	KMenu *mContextMenu;
};

#endif /*KUPDAEMON_H*/

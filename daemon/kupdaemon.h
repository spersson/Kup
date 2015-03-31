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

#ifndef KUPDAEMON_H
#define KUPDAEMON_H

#include <KSharedConfig>

#define KUP_DBUS_SERVICE_NAME QLatin1String("org.kde.kup-daemon")
#define KUP_DBUS_OBJECT_PATH QLatin1String("/DaemonControl")


class KupSettings;
class PlanExecutor;

class QMenu;
class KStatusNotifierItem;

class QTimer;

class KupDaemon : public QObject
{
	Q_OBJECT
	// interface names are not allowed to have hyphens. set name here without hyphen, otherwise the
	// name gets taken from KAboutData combined with the QMetaData of this class.
	Q_CLASSINFO("D-Bus Interface", "org.kde.kupdaemon")

public:
	KupDaemon();
	virtual ~KupDaemon();
	bool shouldStart();
	void setupGuiStuff();

public slots:
	void reloadConfig();
	void showConfig();
	void updateTrayIcon();
	void runIntegrityCheck(QString pPath);

private:
	void setupExecutors();
	void setupTrayIcon();
	void setupContextMenu();

	KSharedConfigPtr mConfig;
	KupSettings *mSettings;
	QList<PlanExecutor *> mExecutors;
	KStatusNotifierItem *mStatusNotifier;
	QMenu *mContextMenu;
	QTimer *mUsageAccumulatorTimer;
	bool mWaitingToReloadConfig;
};

#endif /*KUPDAEMON_H*/

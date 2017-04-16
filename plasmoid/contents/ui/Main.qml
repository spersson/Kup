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

import QtQuick 2.0
import org.kde.plasma.plasmoid 2.0
import org.kde.plasma.core 2.0 as PlasmaCore

Item {
	Plasmoid.switchWidth: units.gridUnit * 10
	Plasmoid.switchHeight: units.gridUnit * 10
	Plasmoid.toolTipMainText: getCommonStatus("tooltip title", "Error")
	Plasmoid.toolTipSubText: getCommonStatus("tooltip subtitle", "No connection")
	Plasmoid.icon: getCommonStatus("tooltip icon name", "kup")
	Plasmoid.status: getCommonStatus("tray icon active", false)
						  ? PlasmaCore.Types.ActiveStatus
						  : PlasmaCore.Types.PassiveStatus

	PlasmaCore.DataSource {
		id: backupPlans
		engine: "backups"
		connectedSources: sources

		onSourceAdded: {
			disconnectSource(source);
			connectSource(source);
		}
		onSourceRemoved: {
			disconnectSource(source);
		}
	}


	function getCommonStatus(key, def){
		var result = backupPlans.data["common"][key];
		if(result === undefined) {
			result = def;
		}
		return result;
	}

	property int planCount: backupPlans.data["common"]["plan count"]

	Plasmoid.fullRepresentation: FullRepresentation {}
	Plasmoid.compactRepresentation: PlasmaCore.IconItem {
		source: "kup"
		width: units.iconSizes.medium;
		height: units.iconSizes.medium;

		MouseArea {
			anchors.fill: parent
			onClicked: plasmoid.expanded = !plasmoid.expanded
		}
	}
}

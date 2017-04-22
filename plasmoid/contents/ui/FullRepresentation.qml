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

import QtQuick 2.2
import QtQuick.Layouts 1.1

import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.components 2.0 as PlasmaComponents
import org.kde.plasma.extras 2.0 as PlasmaExtras

Item {
	Layout.minimumWidth: units.gridUnit * 12
	Layout.minimumHeight: units.gridUnit * 12

	PlasmaExtras.Heading {
		width: parent.width
		level: 3
		opacity: 0.6
		text: getCommonStatus("no plan reason", "")
		visible: planCount == 0
	}

	ColumnLayout {
		anchors.fill: parent

		PlasmaExtras.ScrollArea {
			Layout.fillWidth: true
			Layout.fillHeight: true

			ListView {
				model: planCount
				delegate: planDelegate
				boundsBehavior: Flickable.StopAtBounds
			}
		}
	}

	Component {
		id: planDelegate

		Column {
			width: parent.width
			spacing: theme.defaultFont.pointSize
			RowLayout {
				width: parent.width
				Column {
					Layout.fillWidth: true
					PlasmaExtras.Heading {
						level: 3
						text: getPlanStatus(index, "description")
					}
					PlasmaExtras.Heading {
						level: 4
						text: getPlanStatus(index, "status heading")
					}
					PlasmaExtras.Paragraph {
						text: getPlanStatus(index, "status details")
					}
				}
				PlasmaCore.IconItem {
					source: getPlanStatus(index, "icon name")
					Layout.alignment: Qt.AlignRight | Qt.AlignTop
					Layout.preferredWidth: units.iconSizes.huge
					Layout.preferredHeight: units.iconSizes.huge
				}
			}
			Flow {
				width: parent.width
				spacing: theme.defaultFont.pointSize
				PlasmaComponents.Button {
					text: i18nd("kup", "Save new backup")
					visible: getPlanStatus(index, "destination available") &&
								!getPlanStatus(index, "busy")
					onClicked: startOperation(index, "save backup")
				}
				PlasmaComponents.Button {
					text: i18nd("kup", "Show files")
					visible: getPlanStatus(index, "destination available")
					onClicked: startOperation(index, "show backup files")
				}
				PlasmaComponents.Button {
					text: i18nd("kup", "Show log file")
					visible: getPlanStatus(index, "log file exists")
					onClicked: startOperation(index, "show log file")
				}
			}
		}
	}

	function getPlanStatus(planNumber, key){
		return backupPlans.data[planNumber.toString()][key];
	}

	function startOperation(i, name) {
		var service = backupPlans.serviceForSource(i.toString());
		var operation = service.operationDescription(name);
		service.startOperationCall(operation);
	}
}

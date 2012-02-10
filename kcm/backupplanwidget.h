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

#ifndef BACKUPPLANWIDGET_H
#define BACKUPPLANWIDGET_H

#include <QWidget>

class BackupPlan;
class FolderSelectionModel;

class KLineEdit;
class KPageWidget;
class KPageWidgetItem;
class KPushButton;

class QTreeView;

class ConfigIncludeDummy : public QWidget {
	Q_OBJECT
signals:
	void includeListChanged();
public:
	Q_PROPERTY(QStringList includeList READ includeList WRITE setIncludeList USER true)
	ConfigIncludeDummy(FolderSelectionModel *pModel, QTreeView *pParent);
	QStringList includeList();
	void setIncludeList(QStringList pIncludeList);
	FolderSelectionModel *mModel;
	QTreeView *mTreeView;
};

class ConfigExcludeDummy : public QWidget {
	Q_OBJECT
signals:
	void excludeListChanged();
public:
	Q_PROPERTY(QStringList excludeList READ excludeList WRITE setExcludeList USER true)
	ConfigExcludeDummy(FolderSelectionModel *pModel, QTreeView *pParent);
	QStringList excludeList();
	void setExcludeList(QStringList pExcludeList);
	FolderSelectionModel *mModel;
	QTreeView *mTreeView;
};

class BackupPlanWidget : public QWidget
{
	Q_OBJECT
public:
	explicit BackupPlanWidget(bool pCreatePages = true, QWidget *pParent = 0);

	static KPageWidgetItem *createSourcePage(QWidget *pParent = NULL);
	static KPageWidgetItem *createDestinationPage(QWidget *pParent = NULL);
	static KPageWidgetItem *createSchedulePage(QWidget *pParent = NULL);

	KLineEdit *mDescriptionEdit;
	KPushButton *mConfigureButton;
	KPageWidget *mConfigPages;

signals:
	void requestOverviewReturn();
};

#endif // BACKUPPLANWIDGET_H

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

#ifndef KUPKCM_H
#define KUPKCM_H

#include <KCModule>
#include <KSharedConfig>

#include <QList>

class BackupPlan;
class BackupPlanWidget;
class KupSettings;
class KupSettingsWidget;
class PlanStatusWidget;

class KAssistantDialog;
class KPageWidgetItem;
class QPushButton;

class QCheckBox;
class QStackedLayout;
class QVBoxLayout;

class KupKcm : public KCModule
{
	Q_OBJECT
public:
	KupKcm(QWidget *pParent, const QVariantList &pArgs);

public slots:
	virtual void load();
	virtual void save();

	void addPlan();
	void updateChangedStatus();
	void showFrontPage();
	void removePlan();
	void configurePlan();

private:
	void createSettingsFrontPage();
	void createPlanWidgets(int pIndex);
	void completelyRemovePlan(int pIndex);
	void partiallyRemovePlan(int pIndex);

	KSharedConfigPtr mConfig;
	KupSettings *mSettings;
	QWidget *mFrontPage;
	QList<BackupPlan *> mPlans;
	QList<BackupPlanWidget *> mPlanWidgets;
	QList<PlanStatusWidget *> mStatusWidgets;
	QList<KConfigDialogManager *> mConfigManagers;
	QStackedLayout *mStackedLayout;
	QVBoxLayout *mVerticalLayout;
	QCheckBox *mEnableCheckBox;
	QPushButton *mAddPlanButton;
	QString mBupVersion;
	QString mRsyncVersion;
	bool mPar2Available;
};

#endif // KUPKCM_H

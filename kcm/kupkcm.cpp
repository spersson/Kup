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

#include "backupplan.h"
#include "backupplanwidget.h"
#include "kupdaemon.h"
#include "kupkcm.h"
#include "kupsettings.h"
#include "planstatuswidget.h"

#include <QCheckBox>
#include <QDBusInterface>
#include <QLabel>
#include <QScrollArea>
#include <QStackedLayout>

#include <KAboutData>
#include <KAssistantDialog>
#include <KConfigDialogManager>
#include <KLineEdit>
#include <KPluginFactory>
#include <KProcess>
#include <KPushButton>

K_PLUGIN_FACTORY(KupKcmFactory, registerPlugin<KupKcm>();)
K_EXPORT_PLUGIN(KupKcmFactory("kcm_kup", "kup"))

static const char version[] = "0.3";

KupKcm::KupKcm(QWidget *pParent, const QList<QVariant> &pArgs)
   : KCModule(KupKcmFactory::componentData(), pParent, pArgs)
{
	KAboutData *lAboutData = new KAboutData("kcm_kup", 0, ki18n("Kup Configuration Module"),
	                                        version, ki18n("Configuration of backup plans for the Kup backup system"),
	                                        KAboutData::License_GPL, ki18n("Copyright 2011 Simon Persson"));
	lAboutData->addAuthor(ki18n("Simon Persson"), ki18n("Maintainer"), "simonpersson1@gmail.com");
	setAboutData(lAboutData);
	setObjectName("kcm_kup"); //needed for the kconfigdialogmanager magic
	setButtons((Apply | buttons()) & ~Default);

	KProcess lBupProcess;
	lBupProcess << QLatin1String("bup") << QLatin1String("version");
	lBupProcess.setOutputChannelMode(KProcess::SeparateChannels);
	int lExitCode = lBupProcess.execute();
	if(lExitCode >= 0) {
		mBupVersion = lBupProcess.readAllStandardOutput();
	}

	if(mBupVersion.isEmpty()) {
		QLabel *lSorryIcon = new QLabel;
		lSorryIcon->setPixmap(KIconLoader::global()->loadIcon(QLatin1String("dialog-error"),
		                                                      KIconLoader::Dialog, KIconLoader::SizeHuge));
		QLabel *lSorryText = new QLabel(i18nc("@info:label", "You need to install the program "
		                                      "called \"bup\" before you can activate any backup plans."));
		QHBoxLayout *lHLayout = new QHBoxLayout;
		lHLayout->addWidget(lSorryIcon);
		lHLayout->addWidget(lSorryText);
		lHLayout->addStretch();
		setLayout(lHLayout);
	} else {
		mConfig = KSharedConfig::openConfig("kuprc");
		mSettings = new KupSettings(mConfig, this);
		for(int i = 0; i < mSettings->mNumberOfPlans; ++i) {
			mPlans.append(new BackupPlan(i+1, mConfig, this));
			mConfigManagers.append(NULL);
			mPlanWidgets.append(NULL);
			mStatusWidgets.append(NULL);
		}
		createSettingsFrontPage();
		addConfig(mSettings, mFrontPage);
		mStackedLayout = new QStackedLayout;
		mStackedLayout->addWidget(mFrontPage);
		setLayout(mStackedLayout);
	}
}


void KupKcm::load() {
	if(mBupVersion.isEmpty()) {
		return;
	}
	// status will be set correctly after construction, set to checked here to
	// match the enabled status of other widgets
	mEnableCheckBox->setChecked(true);
	for(int i = 0; i < mSettings->mNumberOfPlans; ++i) {
		if(!mConfigManagers.at(i))
			createPlanWidgets(i);
		mConfigManagers.at(i)->updateWidgets();
	}
	for(int i = mSettings->mNumberOfPlans; i < mPlans.count();) {
		completelyRemovePlan(i);
	}
	KCModule::load();
}

void KupKcm::save() {
	KConfigDialogManager *lManager;
	BackupPlan *lPlan;
	int lPlansRemoved = 0;
	for(int i=0; i < mPlans.count(); ++i) {
		lPlan = mPlans.at(i);
		lManager = mConfigManagers.at(i);
		if(lManager != NULL) {
			if(lPlansRemoved != 0) {
				lPlan->removePlanFromConfig();
				lPlan->setPlanNumber(i + 1);
				// config manager does not detect a changed group name of the config items.
				// To work around, read default settings - config manager will then notice
				// changed values and save current widget status into the config using the
				// new group name. If all settings for the plan already was default then
				// nothing was saved anyway, either under old or new group name.
				lPlan->setDefaults();
			}
			mPlanWidgets.at(i)->saveExtraData();
			lManager->updateSettings();
			mStatusWidgets.at(i)->updateIcon();
		}
		else {
			lPlan->removePlanFromConfig();
			delete mPlans.takeAt(i);
			mConfigManagers.removeAt(i);
			mStatusWidgets.removeAt(i);
			mPlanWidgets.removeAt(i);
			++lPlansRemoved;
			--i;
		}
	}
	mSettings->mNumberOfPlans = mPlans.count();
	mSettings->writeConfig();

	KCModule::save();

	QDBusInterface lInterface(KUP_DBUS_SERVICE_NAME, KUP_DBUS_OBJECT_PATH);
	if(lInterface.isValid()) {
		lInterface.call(QLatin1String("reloadConfig"));
	} else {
		KProcess::execute(QLatin1String("kupdaemon")); // kuniqueapplication, should exit very quickly.
	}
}

void KupKcm::updateChangedStatus() {
	bool lHasUnmanagedChanged = false;
	foreach(KConfigDialogManager *lConfigManager, mConfigManagers) {
		if(!lConfigManager || lConfigManager->hasChanged()) {
			lHasUnmanagedChanged = true;
			break;
		}
	}
	if(mPlanWidgets.count() != mSettings->mNumberOfPlans)
		lHasUnmanagedChanged = true;
	unmanagedWidgetChangeState(lHasUnmanagedChanged);
}

void KupKcm::showFrontPage() {
	mStackedLayout->setCurrentIndex(0);
}

void KupKcm::removePlan() {
	for(int i = 0; i < mStatusWidgets.count(); ++i) {
		PlanStatusWidget *lStatusWidget = mStatusWidgets.at(i);
		if(QObject::sender() == lStatusWidget) {
			if(i < mSettings->mNumberOfPlans)
				partiallyRemovePlan(i);
			else
				completelyRemovePlan(i);
			updateChangedStatus();
			break;
		}
	}
}

void KupKcm::configurePlan() {
	for(int i = 0; i < mStatusWidgets.count(); ++i) {
		PlanStatusWidget *lPlanWidget = mStatusWidgets.at(i);
		if(QObject::sender() == lPlanWidget) {
			mStackedLayout->setCurrentWidget(mPlanWidgets.at(i));
			break;
		}
	}
}

void KupKcm::addPlan() {
	mPlans.append(new BackupPlan(mPlans.count() + 1, mConfig, this));
	mConfigManagers.append(NULL);
	mPlanWidgets.append(NULL);
	mStatusWidgets.append(NULL);
	createPlanWidgets(mPlans.count() - 1);
	updateChangedStatus();
	mStatusWidgets.at(mPlans.count() - 1)->mConfigureButton->click();
}

void KupKcm::createSettingsFrontPage() {
	mFrontPage = new QWidget;
	QHBoxLayout *lHLayout = new QHBoxLayout;
	QVBoxLayout *lVLayout = new QVBoxLayout;
	QScrollArea *lScrollArea = new QScrollArea;
	QWidget *lCentralWidget = new QWidget(lScrollArea);
	mVerticalLayout = new QVBoxLayout;
	lScrollArea->setWidget(lCentralWidget);
	lScrollArea->setWidgetResizable(true);
	lScrollArea->setFrameStyle(QFrame::NoFrame);

	mAddPlanButton = new KPushButton(KIcon("list-add"), i18nc("@action:button", "Add New Plan"));
	connect(mAddPlanButton, SIGNAL(clicked()), this, SLOT(addPlan()));

	mEnableCheckBox = new QCheckBox(i18nc("@option:check", "Backups Enabled"));
	mEnableCheckBox->setObjectName("kcfg_Backups enabled");
	connect(mEnableCheckBox, SIGNAL(toggled(bool)), mAddPlanButton, SLOT(setEnabled(bool)));

	lHLayout->addWidget(mEnableCheckBox);
	lHLayout->addStretch();
	lHLayout->addWidget(mAddPlanButton);
	lVLayout->addLayout(lHLayout);
	lVLayout->addWidget(lScrollArea);
	mFrontPage->setLayout(lVLayout);

	mVerticalLayout->addStretch(1);
	lCentralWidget->setLayout(mVerticalLayout);
}

void KupKcm::createPlanWidgets(int pIndex) {
	BackupPlanWidget *lPlanWidget = new BackupPlanWidget(mPlans.at(pIndex), mBupVersion);
	connect(lPlanWidget, SIGNAL(requestOverviewReturn()), this, SLOT(showFrontPage()));
	KConfigDialogManager *lConfigManager = new KConfigDialogManager(lPlanWidget, mPlans.at(pIndex));
	lConfigManager->setObjectName(objectName());
	connect(lConfigManager, SIGNAL(widgetModified()), this, SLOT(updateChangedStatus()));
	PlanStatusWidget *lStatusWidget = new PlanStatusWidget(mPlans.at(pIndex));
	connect(lStatusWidget, SIGNAL(removeMe()), this, SLOT(removePlan()));
	connect(lStatusWidget, SIGNAL(configureMe()), this, SLOT(configurePlan()));
	connect(mEnableCheckBox, SIGNAL(toggled(bool)), lStatusWidget, SLOT(setEnabled(bool)));
	connect(lPlanWidget->mDescriptionEdit, SIGNAL(textChanged(QString)),
	        lStatusWidget->mDescriptionLabel, SLOT(setText(QString)));

	mConfigManagers[pIndex] = lConfigManager;
	mPlanWidgets[pIndex] = lPlanWidget;
	mStackedLayout->insertWidget(pIndex + 1, lPlanWidget);
	mStatusWidgets[pIndex] = lStatusWidget;
	mVerticalLayout->insertWidget(pIndex, lStatusWidget);
}

void KupKcm::completelyRemovePlan(int pIndex) {
	mVerticalLayout->removeWidget(mStatusWidgets.at(pIndex));
	mStackedLayout->removeWidget(mPlanWidgets.at(pIndex));
	delete mConfigManagers.takeAt(pIndex);
	delete mStatusWidgets.takeAt(pIndex);
	delete mPlanWidgets.takeAt(pIndex);
	delete mPlans.takeAt(pIndex);
}

void KupKcm::partiallyRemovePlan(int pIndex) {
	mVerticalLayout->removeWidget(mStatusWidgets.at(pIndex));
	mStackedLayout->removeWidget(mPlanWidgets.at(pIndex));
	mConfigManagers.at(pIndex)->deleteLater();
	mConfigManagers[pIndex] = NULL;
	mStatusWidgets.at(pIndex)->deleteLater();
	mStatusWidgets[pIndex] = NULL;
	mPlanWidgets.at(pIndex)->deleteLater();
	mPlanWidgets[pIndex] = NULL;
}


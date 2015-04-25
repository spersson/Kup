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

#include "planstatuswidget.h"
#include "kupsettings.h"
#include "backupplan.h"

#include <KGlobal>

#include <QBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <KLocalizedString>

PlanStatusWidget::PlanStatusWidget(BackupPlan *pPlan, QWidget *pParent)
   : QGroupBox(pParent), mPlan(pPlan)
{
	QVBoxLayout *lVLayout1 = new QVBoxLayout;
	QHBoxLayout *lHLayout1 = new QHBoxLayout;
	QHBoxLayout *lHLayout2 = new QHBoxLayout;

	mDescriptionLabel = new QLabel(mPlan->mDescription);
	QFont lDescriptionFont = mDescriptionLabel->font();
	lDescriptionFont.setPointSizeF(lDescriptionFont.pointSizeF() + 2.0);
	lDescriptionFont.setBold(true);
	mDescriptionLabel->setFont(lDescriptionFont);
	mStatusIconLabel = new QLabel();
	mStatusTextLabel = new QLabel(statusText()); //TODO: add dbus interface to be notified from daemon when this is updated.
	mConfigureButton = new QPushButton(QIcon::fromTheme(QLatin1String("configure")), i18nc("@action:button", "Configure"));
	connect(mConfigureButton, SIGNAL(clicked()), this, SIGNAL(configureMe()));
	mRemoveButton = new QPushButton(QIcon::fromTheme(QLatin1String("list-remove")), i18nc("@action:button", "Remove"));
	connect(mRemoveButton, SIGNAL(clicked()), this, SIGNAL(removeMe()));

	lVLayout1->addWidget(mDescriptionLabel);
	lHLayout1->addWidget(mStatusIconLabel);
	lHLayout1->addWidget(mStatusTextLabel);
	lHLayout1->addStretch();
	lVLayout1->addLayout(lHLayout1);
	lHLayout2->addStretch();
	lHLayout2->addWidget(mConfigureButton);
	lHLayout2->addWidget(mRemoveButton);
	lVLayout1->addLayout(lHLayout2);
	setLayout(lVLayout1);

	updateIcon();
}

QString PlanStatusWidget::statusText() {
	KLocale *lLocale = KGlobal::locale();
	QString lStatus;
	if(mPlan->mLastCompleteBackup.isValid()) {
		QDateTime lLocalTime = mPlan->mLastCompleteBackup.toLocalTime();

		lStatus += i18nc("@label %1 is fancy formatted date, %2 is time of day", "Last backup was taken %1 at %2.\n",
		                lLocale->formatDate(lLocalTime.date(), KLocale::FancyLongDate),
		                lLocale->formatLocaleTime(lLocalTime.time()));
		if(mPlan->mLastBackupSize > 0.0)
			lStatus += i18nc("@label %1 is storage size of archive" ,
			                 "The size of the backup archive was %1.\n",
			                 lLocale->formatByteSize(mPlan->mLastBackupSize, 1));
		if(mPlan->mLastAvailableSpace > 0.0)
			lStatus += i18nc("@label %1 is free storage space",
			                 "The destination still had %1 available.\n",
			                 lLocale->formatByteSize(mPlan->mLastAvailableSpace, 1));
	} else {
		lStatus = i18nc("@label", "This backup plan has never been run.");
	}
	return lStatus;
}

void PlanStatusWidget::updateIcon() {
	mStatusIconLabel->setPixmap(QIcon::fromTheme(mPlan->iconName(mPlan->backupStatus())).pixmap(64,64));
}


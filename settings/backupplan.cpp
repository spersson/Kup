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

#include <QDir>
#include <QString>

#include <KGlobalSettings>
#include <KIcon>
#include <KLocale>

BackupPlan::BackupPlan(int pPlanNumber, KSharedConfigPtr pConfig, QObject *pParent)
   :KConfigSkeleton(pConfig, pParent), mPlanNumber(pPlanNumber)
{
	setCurrentGroup(QString::fromLatin1("Plan/%1").arg(mPlanNumber));

	addItemString(QLatin1String("Description"), mDescription,
	              i18nc("@label Default name for a new backup plan, %1 is the number of the plan in order",
	                    "Backup plan %1", pPlanNumber));
	QStringList lDefaultIncludeList;
	lDefaultIncludeList << QDir::homePath();
	addItemStringList(QLatin1String("Paths included"), mPathsIncluded, lDefaultIncludeList);
	QStringList lDefaultExcludeList;
	lDefaultExcludeList << KGlobalSettings::musicPath();
	lDefaultExcludeList << KGlobalSettings::videosPath();
	lDefaultExcludeList << QDir::homePath() + QDir::separator() + QLatin1String(".cache");
	lDefaultExcludeList << QDir::homePath() + QDir::separator() + QLatin1String(".bup");
	lDefaultExcludeList << QDir::homePath() + QDir::separator() + QLatin1String(".thumbnails");
	QMutableListIterator<QString> i(lDefaultExcludeList);
	while(i.hasNext()) {
		QString &lPath = i.next();
		if(lPath.endsWith(QLatin1Char('/')))
			lPath.chop(1);
	}

	addItemStringList(QLatin1String("Paths excluded"), mPathsExcluded, lDefaultExcludeList);
	addItemInt(QLatin1String("Backup type"), mBackupType);

	addItemInt(QLatin1String("Schedule type"), mScheduleType, 2);
	addItemInt(QLatin1String("Schedule interval"), mScheduleInterval, 1);
	addItemInt(QLatin1String("Schedule interval unit"), mScheduleIntervalUnit, 3);
	addItemInt(QLatin1String("Usage limit"), mUsageLimit, 25);
	addItemBool(QLatin1String("Ask first"), mAskBeforeTakingBackup, true);

	addItemInt(QLatin1String("Destination type"), mDestinationType, 1);
	addItem(new KCoreConfigSkeleton::ItemUrl(currentGroup(),
	                                         QLatin1String("Filesystem destination path"),
	                                         mFilesystemDestinationPath,
	                                         QDir::homePath() + QLatin1String("/.bup")));
	addItemString(QLatin1String("External drive UUID"), mExternalUUID);
	addItemPath(QLatin1String("External drive destination path"), mExternalDestinationPath, i18n("Backups"));
	addItemString(QLatin1String("External volume label"), mExternalVolumeLabel);
	addItemULongLong(QLatin1String("External volume capacity"), mExternalVolumeCapacity);
	addItemString(QLatin1String("External device description"), mExternalDeviceDescription);
	addItemInt(QLatin1String("External partition number"), mExternalPartitionNumber);
	addItemInt(QLatin1String("External partitions count"), mExternalPartitionsOnDrive);

	addItemBool(QLatin1String("Show hidden folders"), mShowHiddenFolders);
	addItemBool(QLatin1String("Generate recovery info"), mGenerateRecoveryInfo);

	addItemDateTime(QLatin1String("Last complete backup"), mLastCompleteBackup);
	addItemDouble(QLatin1String("Last backup size"), mLastBackupSize);
	addItemDouble(QLatin1String("Last available space"), mLastAvailableSpace);
	addItemUInt(QLatin1String("Accumulated usage time"), mAccumulatedUsageTime);
	readConfig();
}

void BackupPlan::setPlanNumber(int pPlanNumber) {
	mPlanNumber = pPlanNumber;
	QString lGroupName = QString::fromLatin1("Plan/%1").arg(mPlanNumber);
	foreach(KConfigSkeletonItem *lItem, items()) {
		lItem->setGroup(lGroupName);
	}
}

void BackupPlan::removePlanFromConfig() {
	config()->deleteGroup(QString::fromLatin1("Plan/%1").arg(mPlanNumber));
}

QDateTime BackupPlan::nextScheduledTime() {
	Q_ASSERT(mScheduleType == 1);
	if(!mLastCompleteBackup.isValid())
		return QDateTime(); //plan has never been run

	return mLastCompleteBackup.addSecs(scheduleIntervalInSeconds());
}

int BackupPlan::scheduleIntervalInSeconds() {
	Q_ASSERT(mScheduleType == 1);

	switch(mScheduleIntervalUnit) {
	case 0:
		return mScheduleInterval * 60;
	case 1:
		return mScheduleInterval * 60 * 60;
	case 2:
		return mScheduleInterval * 60 * 60 * 24;
	case 3:
		return mScheduleInterval * 60 * 60 * 24 * 7;
	default:
		return 0;
	}
}

BackupPlan::Status BackupPlan::backupStatus() {
	if(!mLastCompleteBackup.isValid())
		return BAD;

	int lStatus = 5; //trigger BAD status if schedule type is something strange
	int lInterval = 1;

	switch(mScheduleType) {
	case MANUAL:
		lStatus = mLastCompleteBackup.secsTo(QDateTime::currentDateTime().toUTC());
		lInterval = 60*60*24*7; //assume seven days is safe interval
		break;
	case INTERVAL:
		lStatus = mLastCompleteBackup.secsTo(QDateTime::currentDateTime().toUTC());
		lInterval = scheduleIntervalInSeconds();
		break;
	case USAGE:
		lStatus = mAccumulatedUsageTime;
		lInterval = mUsageLimit * 3600;
		break;
	}

	if(lStatus < lInterval)
		return GOOD;
	else if(lStatus < lInterval * 3)
		return MEDIUM;
	else
		return BAD;
}

QString BackupPlan::iconName(Status pStatus) {
	switch(pStatus) {
	case GOOD:
		return QLatin1String("security-high");
	case MEDIUM:
		return QLatin1String("security-medium");
	case BAD:
		return QLatin1String("security-low");
	}
	return QLatin1String("unknown");
}

void BackupPlan::usrReadConfig() {
	//correct the time spec after default read routines.
	mLastCompleteBackup.setTimeSpec(Qt::UTC);
}


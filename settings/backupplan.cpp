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
	setCurrentGroup(QString("Plan/%1").arg(mPlanNumber));

	addItemString("Description", mDescription,
	              i18nc("@label Default name for a new backup plan, %1 is the number of the plan in order",
	                    "Backup plan %1", pPlanNumber));
	QStringList lDefaultIncludeList;
	lDefaultIncludeList << QDir::homePath();
	addItemStringList("Paths included", mPathsIncluded, lDefaultIncludeList);
	QStringList lDefaultExcludeList;
	lDefaultExcludeList << KGlobalSettings::musicPath();
	lDefaultExcludeList << KGlobalSettings::videosPath();
	lDefaultExcludeList << QDir::homePath() + QDir::separator() + QLatin1String(".cache");
	lDefaultExcludeList << QDir::homePath() + QDir::separator() + QLatin1String(".bup");
	lDefaultExcludeList << QDir::homePath() + QDir::separator() + QLatin1String(".thumbnails");
	QMutableListIterator<QString> i(lDefaultExcludeList);
	while(i.hasNext()) {
		QString &lPath = i.next();
		if(lPath.endsWith('/'))
			lPath.chop(1);
	}

	addItemStringList("Paths excluded", mPathsExcluded, lDefaultExcludeList);
	addItemStringList("Patterns excluded", mPatternsExcluded);
	addItemBool("Use system exclude list", mUseSystemExcludeList, true);
	addItemBool("Use user exclude list", mUseUserExcludeList, true);

	addItemInt("Schedule type", mScheduleType, 2);
	addItemInt("Schedule interval", mScheduleInterval, 1);
	addItemInt("Schedule interval unit", mScheduleIntervalUnit, 3);
	addItemInt("Usage limit", mUsageLimit, 25);
	addItemBool("Ask first", mAskBeforeTakingBackup, true);

	addItemInt("Destination type", mDestinationType, 1);
	addItem(new KCoreConfigSkeleton::ItemUrl(currentGroup(),
	                                         "Filesystem destination path",
	                                         mFilesystemDestinationPath,
	                                         QDir::homePath() + QDir::separator() + ".bup"));
	addItemString("External drive UUID", mExternalUUID);
	addItemPath("External drive destination path", mExternalDestinationPath, i18n("Backups"));
	addItemString("External volume label", mExternalVolumeLabel);
	addItemULongLong("External volume capacity", mExternalVolumeCapacity);
	addItemString("External device description", mExternalDeviceDescription);
	addItemInt("External partition number", mExternalPartitionNumber);
	addItemInt("External partitions count", mExternalPartitionsOnDrive);

//	addItemString("SSH server name", mSshServerName);
//	addItemString("SSH login name", mSshLoginName);
//	addItemPassword("SSH login password", mSshLoginPassword);
//	addItemPath("SSH destination path", mSshDestinationPath);

	addItemBool("Show hidden folders", mShowHiddenFolders);
	addItemInt("Compression level", mCompressionLevel, 1);

	addItemDateTime("Last complete backup", mLastCompleteBackup);
	addItemDouble("Last backup size", mLastBackupSize);
	addItemDouble("Last available space", mLastAvailableSpace);
	addItemUInt("Accumulated usage time", mAccumulatedUsageTime);
	readConfig();
}

BackupPlan::~BackupPlan() {
}

void BackupPlan::setPlanNumber(int pPlanNumber) {
	mPlanNumber = pPlanNumber;
	QString lGroupName = QString("Plan/%1").arg(mPlanNumber);
	foreach(KConfigSkeletonItem *lItem, items()) {
		lItem->setGroup(lGroupName);
	}
}

void BackupPlan::removePlanFromConfig() {
	config()->deleteGroup(QString("Plan/%1").arg(mPlanNumber));
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


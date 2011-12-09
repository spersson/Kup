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


#ifndef BACKUPPLAN_H
#define BACKUPPLAN_H

#include <kcoreconfigskeleton.h>

class Schedule;

class BackupPlan : public KCoreConfigSkeleton
{
public:
	BackupPlan(int pPlanNumber, KSharedConfigPtr pConfig, QObject *pParent = 0);

	virtual ~BackupPlan();

	int planNumber() const {return mPlanNumber;}
	virtual void setPlanNumber(int pPlanNumber);

	void removePlanFromConfig();

	QString mDescription;
	QStringList mPathsIncluded;
	QStringList mPathsExcluded;
	QStringList mPatternsExcluded;
	// True if files from the system exclude list shall be excluded.
	bool mUseSystemExcludeList;
	// True if files from the user-specific exclude list shall be excluded.
	bool mUseUserExcludeList;

	qint32 mScheduleType;
	qint32 mScheduleInterval;
	qint32 mScheduleIntervalUnit;

	qint32 mDestinationType;

	KUrl mFilesystemDestinationPath;

	QString mExternalUUID;
	QString mExternalDestinationPath;

//	QString mSshServerName;
//	QString mSshLoginName;
//	QString mSshLoginPassword; //TODO: move to kwallet
//	QString mSshDestinationPath;

	QDateTime mLastCompleteBackup;
	// Size of the last backup in bytes.
	double mLastBackupSize;
	// Last known available space on destination
	double mLastAvailableSpace;

	virtual QDateTime nextScheduledTime();
	virtual int scheduleIntervalInSeconds();

protected:
	virtual void usrReadConfig();
	int mPlanNumber;
};

#endif // BACKUPPLAN_H

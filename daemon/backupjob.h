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

#ifndef BACKUPJOB_H
#define BACKUPJOB_H

#include "backupplan.h"

#include <KJob>

#include <QFile>
#include <QStringList>
#include <QTextStream>

class KupDaemon;

class BackupJob : public KJob
{
	Q_OBJECT
public:
	enum ErrorCodes {
		ErrorWithLog = UserDefinedError,
		ErrorWithoutLog,
		ErrorSuggestRepair
	};

	void start() Q_DECL_OVERRIDE;

protected slots:
	virtual void performJob() = 0;

protected:
	BackupJob(const BackupPlan &pBackupPlan, const QString &pDestinationPath, const QString &pLogFilePath, KupDaemon *pKupDaemon);
	static void makeNice(int pPid);
	QString quoteArgs(const QStringList &pCommand);
	void jobFinishedSuccess();
	void jobFinishedError(ErrorCodes pErrorCode, QString pErrorText);
	const BackupPlan &mBackupPlan;
	QString mDestinationPath;
	QString mLogFilePath;
	QFile mLogFile;
	QTextStream mLogStream;
	KupDaemon *mKupDaemon;
};

#endif // BACKUPJOB_H

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

#ifndef BUPJOB_H
#define BUPJOB_H

#include "backupjob.h"

#include <KProcess>

class BupJob : public BackupJob
{
	Q_OBJECT

public:
	BupJob(const QStringList &pPathsIncluded, const QStringList &pPathsExcluded,
	       const QString &pDestinationPath, const QString &pLogFilePath, bool pGenerateRecoveryInfo);
	virtual void start();

protected slots:
	void startIndexing();
	void slotIndexingStarted();
	void slotIndexingDone(int pExitCode, QProcess::ExitStatus pExitStatus);
	void slotSavingStarted();
	void slotSavingDone(int pExitCode, QProcess::ExitStatus pExitStatus);
	void slotRecoveryInfoStarted();
	void slotRecoveryInfoDone(int pExitCode, QProcess::ExitStatus pExitStatus);

protected:
	KProcess mIndexProcess;
	KProcess mSaveProcess;
	KProcess mFsckProcess;
	QString mBupVersion;
	bool mGenerateRecoveryInfo;
};

#endif /*BUPJOB_H*/

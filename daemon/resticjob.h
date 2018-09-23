/***************************************************************************
 *   Copyright Luca Carlon                                                 *
 *   carlon.luca@gmail.com                                                 *
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

#ifndef RESTICJOB_H
#define RESTICJOB_H

#include <QSharedPointer>

#include "backupjob.h"
#include "restichelper.h"

class ResticJob : public BackupJob
{
    Q_OBJECT
public:
    explicit ResticJob(const BackupPlan &pBackupPlan,
                       const QString &pDestinationPath,
                       const QString &pLogFilePath,
                       KupDaemon *pKupDaemon);

protected:
	virtual bool doKill();
	virtual bool doSuspend();
	virtual bool doResume();

protected slots:
    void performJob() Q_DECL_OVERRIDE;

private slots:
	void onBackupStep(ResticProgressState state);
	void onBackupCompleted();
	void onBackupFailed();

	void onForgetCompleted();
	void onForgetFailed();

private:
	bool isCommandAvailable();
	bool sendSignal(int signal);
	QList<ResticForgetSwitch> buildSwitchList();

private:
	bool mProcessing;
	ResticHelper mResticHelper;
	QSharedPointer<ResticBackupProcess> mBackupProcess;
	QSharedPointer<ResticForgetProcess> mForgetProcess;
};

#endif // RESTICJOB_H

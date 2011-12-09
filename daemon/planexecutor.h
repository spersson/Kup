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

#ifndef PLANEXECUTOR_H
#define PLANEXECUTOR_H

#include "backupplan.h"

#include <QObject>
#include <KProcess>

class KRun;
class KUiServerJobTracker;
//class KTempDir;

class QAction;
class QMenu;

class PlanExecutor : public QObject
{
	Q_OBJECT
public:
	PlanExecutor(BackupPlan *pPlan, QObject *pParent);

	virtual bool destinationAvailable() {
		return mDestinationAvailable;
	}

	virtual bool running() {
		return mRunning;
	}

	virtual QMenu *planActions() {
		return mActionMenu;
	}

	virtual QString description() {
		return mPlan->mDescription;
	}

public slots:
	virtual void checkStatus();
	virtual void startBackup();
	void showFiles();

signals:
	void statusUpdated();

protected slots:
	void bupFuseFinished(int pExitCode, QProcess::ExitStatus pExitStatus);
	void unmountBupFuse();

protected:
	bool mDestinationAvailable;
	bool mRunning;
	QString mDestinationPath;
	BackupPlan *mPlan;
	KUiServerJobTracker *mJobTracker;
	QMenu *mActionMenu;
	QAction *mShowFilesAction;
	QAction *mRunBackupAction;
	KProcess *mBupFuseProcess;
	QString mTempDir;
};

#endif // PLANEXECUTOR_H

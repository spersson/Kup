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

#ifndef RESTOREJOB_H
#define RESTOREJOB_H

#include "versionlistmodel.h"

#include <KJob>
#include <KProcess>

class RestoreJob : public KJob
{
	Q_OBJECT
public:
	explicit RestoreJob(const QString &pRepositoryPath, const QString &pSourcePath, const QString &pRestorationPath,
	                    int pTotalDirCount, quint64 pTotalFileSize, const QHash<QString, quint64> &pFileSizes);
	void start() override;

protected slots:
	void slotRestoringStarted();
	void slotRestoringDone(int pExitCode, QProcess::ExitStatus pExitStatus);

protected:
	void timerEvent(QTimerEvent *pTimerEvent) override;
	void makeNice(int pPid);
	void moveFolder();

	KProcess mRestoreProcess;
	QString mRepositoryPath;
	QString mSourcePath;
	QString mRestorationPath;
	QString mSourceFileName;
	int mTotalDirCount;
	quint64 mTotalFileSize;
	const QHash<QString, quint64> &mFileSizes;
	int mTimerId;
};

#endif // RESTOREJOB_H

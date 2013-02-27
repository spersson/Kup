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

#include "backupjob.h"
#include "bupjob.h"
#include "rsyncjob.h"

#include <KLocale>

#include <unistd.h>
#include <sys/resource.h>
#ifdef Q_OS_LINUX
#include <sys/syscall.h>
#endif

BackupJob::BackupJob(const QStringList &pPathsIncluded, const QStringList &pPathsExcluded,
                     const QString &pDestinationPath, bool pRunAsRoot)
   :KJob(), mPathsIncluded(pPathsIncluded), mPathsExcluded(pPathsExcluded),
     mDestinationPath(pDestinationPath), mRunAsRoot(pRunAsRoot)
{
}

void BackupJob::startRootHelper(QVariantMap pArguments, int pBackupType) {
	pArguments[QLatin1String("type")] = pBackupType;
	pArguments[QLatin1String("pathsIncluded")] = mPathsIncluded;
	pArguments[QLatin1String("pathsExcluded")] = mPathsExcluded;
	pArguments[QLatin1String("destinationPath")] = mDestinationPath;
	pArguments[QLatin1String("uid")] = (uint)geteuid();
	pArguments[QLatin1String("gid")] = (uint)getegid();

	Action lAction(QLatin1String("org.kde.kup.runner.takebackup"));
	lAction.setHelperID(QLatin1String("org.kde.kup.runner"));
	lAction.setArguments(pArguments);
	connect(lAction.watcher(), SIGNAL(actionPerformed(ActionReply)), SLOT(slotHelperDone(ActionReply)));
	ActionReply lReply = lAction.execute();
	if(checkForError(lReply)) {
		emitResult();
	}
}

void BackupJob::slotHelperDone(ActionReply pReply) {
	checkForError(pReply);
	emitResult();
}

bool BackupJob::checkForError(ActionReply pReply) {
	if(pReply.failed()) {
		setError(1);
		if(pReply.type() == ActionReply::KAuthError) {
			if(pReply.errorCode() != ActionReply::UserCancelled) {
				setErrorText(i18nc("@info", "Failed to take backup as root: %1", pReply.errorDescription()));
			}
		} else {
			setErrorText(pReply.errorDescription());
		}
		return true;
	}
	return false;
}

void BackupJob::makeNice(int pPid) {
#ifdef Q_OS_LINUX
	// See linux documentation Documentation/block/ioprio.txt for details of the syscall
	syscall(SYS_ioprio_set, 1, pPid, 3 << 13 | 7);
#endif
	setpriority(PRIO_PROCESS, pPid, 19);
}


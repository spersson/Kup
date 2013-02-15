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

#include "buphelper.h"
#include "bupjob.h"

#include <QDir>

#include <stdlib.h>
#include <unistd.h>

static void ensureOwner(const QString &pPath, uid_t pUid, gid_t pGid) {
	QFileInfo lThisPathInfo(pPath);
	if(lThisPathInfo.ownerId() != pUid || lThisPathInfo.groupId() != pGid) {
		chown(QFile::encodeName(pPath), pUid, pGid);
	}
	if(!lThisPathInfo.isDir()) {
		return;
	}
	QDir lDir(pPath);
	QFileInfoList lInfoList = lDir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
	foreach(QFileInfo lFileInfo, lInfoList) {
		if(lFileInfo.isDir()) {
			ensureOwner(lFileInfo.filePath(), pUid, pGid);
		} else {
			if(lFileInfo.ownerId() != pUid || lFileInfo.groupId() != pGid) {
				chown(QFile::encodeName(lFileInfo.absoluteFilePath()), pUid, pGid);
			}
		}
	}
}

BupHelper::BupHelper(QObject *parent) :
   QObject(parent)
{
}

ActionReply BupHelper::takebackup(const QVariantMap &pArguments) {
	// PATH is not set initially, we need to set it to allow
	// this process to find system-installed "bup" command, but nothing else.
	setenv("PATH", "/usr/local/bin:/usr/bin:/bin", 1);

	QString lDestinationPath = pArguments[QLatin1String("destinationPath")].toString();
	QString lBupPath = pArguments[QLatin1String("bupPath")].toString();

	BupJob *lJob = new BupJob(pArguments[QLatin1String("pathsIncluded")].toStringList(),
	                          pArguments[QLatin1String("pathsExcluded")].toStringList(),
	                          lDestinationPath,
	                          pArguments[QLatin1String("compressionLevel")].toInt(), false, this);
	lJob->setBupPath(lBupPath);
	if(!lJob->exec()) {
		ActionReply lReply(ActionReply::HelperErrorReply);
		lReply.setErrorCode(1);
		lReply.setErrorDescription(lJob->errorText());
		return lReply;
	}

	uid_t lUid = pArguments[QLatin1String("uid")].toUInt();
	gid_t lGid = pArguments[QLatin1String("gid")].toUInt();

	ensureOwner(lDestinationPath, lUid, lGid);
	if(lBupPath != lDestinationPath) {
		ensureOwner(lBupPath, lUid, lGid);
	}

	return ActionReply::SuccessReply;
}

KDE4_AUTH_HELPER_MAIN("org.kde.kup.runner", BupHelper)

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

#include <QDir>
#include <QDateTime>

#include "mergedvfsrestic.h"
#include "vfshelpers.h"
#include "kuputils.h"

MergedNodeRestic::MergedNodeRestic(QObject *parent, const QString &repoPath, const QString &relPath, const QString &name, uint mode) :
	MergedNode(parent, name, mode)
  , mSnapthosPath(repoPath)
  , mRelPath(relPath)
  , mRegexPath("([\\/][^\\/]+)+\\/snapshots\\/([^\\/]+)\\/(.*)$")
{
}

void MergedNodeRestic::getUrl(int pVersionIndex, QUrl *pComplete, QString *pRepoPath, QString *pBranchName, quint64 *pCommitTime, QString *pPathInRepo) const
{
	if (mVersionList.size() <= pVersionIndex) {
		*pComplete = QUrl();
		return;
	}

	VersionDataRestic* version = static_cast<VersionDataRestic*>(mVersionList[pVersionIndex]);

	if (pComplete)
		*pComplete = QUrl::fromLocalFile(version->absPath());
	if (pRepoPath)
		*pRepoPath = "repo"; // TODO
	if (pBranchName)
		*pBranchName = QString();
	if (pCommitTime)
		*pCommitTime = version->mCommitTime;
	if (pPathInRepo)
		*pPathInRepo = QString();
}

void MergedNodeRestic::generateSubNodes()
{
	QHash<QString, MergedNode*> nodes;
	QDir snapshots(mSnapthosPath);
	QFileInfoList infoList = snapshots.entryInfoList(QStringList(),
													 QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
													 QDir::Name | QDir::Reversed);
	foreach (const QFileInfo &info, infoList) {
		QString pathToSearch = QString("%1%2%3")
				.arg(info.absoluteFilePath()).arg(QDir::separator()).arg(mRelPath);
		QFileInfoList itemList = QDir(pathToSearch).entryInfoList(QStringList(),
																  QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
		foreach (const QFileInfo &item, itemList) {
			QRegularExpressionMatch match = mRegexPath.match(item.absoluteFilePath());
			if (!match.hasMatch())
				continue;
			QDateTime dateTime = QDateTime::fromString(match.captured(2), Qt::ISODate);
			QString relPath = match.captured(3);
			MergedNode* node = nodes.value(relPath);
			if (!node) {
				uint mode = item.isDir() ? DEFAULT_MODE_DIRECTORY : DEFAULT_MODE_FILE;
				node = new MergedNodeRestic(this, mSnapthosPath, relPath, item.fileName(), mode);
				nodes.insert(relPath, node);
			}

			// Compare.
			if (!node->mVersionList.isEmpty()) {
				VersionDataRestic *last = static_cast<VersionDataRestic*>(node->mVersionList.last());
				QString absPath1 = last->absPath();
				QString absPath2 = item.absoluteFilePath();
				if (!fastDiff(absPath1, absPath2))
					continue;
			}

			node->mVersionList.append(new VersionDataRestic(
										  dateTime.toSecsSinceEpoch(),
										  item.lastModified().toSecsSinceEpoch(),
										  static_cast<quint64>(item.size()),
										  item.absoluteFilePath()
										  ));
		}
	}

	if (!mSubNodes)
		mSubNodes = new QList<MergedNode*>(nodes.values());
	else
		mSubNodes->append(nodes.values());
}

MergedRepositoryResitc::MergedRepositoryResitc(QObject *pParent, const QString &pRepoMountPath) :
	MergedRepository(pParent)
  , mRoot(new MergedNodeRestic(pParent, QString("%1%2snapshots").arg(pRepoMountPath).arg(QDir::separator()), QStringLiteral("/"), QStringLiteral("/"), DEFAULT_MODE_DIRECTORY))
{
}

MergedRepositoryResitc::~MergedRepositoryResitc()
{

}

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

#ifndef MERGEDVFS_H
#define MERGEDVFS_H

#include <QHash>
#include <QObject>
#include <QUrl>
#include <QMapIterator>

#include <sys/stat.h>

class MergedNode;
typedef QMap<QString, MergedNode *> NameMap;
typedef QMapIterator<QString, MergedNode *> NameMapIterator;

struct VersionData {
	VersionData(quint64 pCommitTime, quint64 pModifiedDate)
	   :mCommitTime(pCommitTime), mModifiedDate(pModifiedDate)
	{
		mSizeIsValid = false;
	}

	VersionData(quint64 pCommitTime, quint64 pModifiedDate, quint64 pSize)
	   :mCommitTime(pCommitTime), mModifiedDate(pModifiedDate), mSize(pSize)
	{
		mSizeIsValid = true;
	}

	virtual ~VersionData() {}

	virtual quint64 size() = 0;

	bool mSizeIsValid;

	quint64 mCommitTime;
	quint64 mModifiedDate;

protected:
	quint64 mSize;
};

typedef QList<MergedNode*> MergedNodeList;
typedef QListIterator<MergedNode*> MergedNodeListIterator;
typedef QList<VersionData *> VersionList;
typedef QListIterator<VersionData *> VersionListIterator;

class MergedNode: public QObject {
	Q_OBJECT
	friend class VersionData;
public:
	MergedNode(QObject *pParent, const QString &pName, uint pMode);
	virtual ~MergedNode() {
		if(mSubNodes != nullptr) {
			qDeleteAll(*mSubNodes);
			delete mSubNodes;
		}
	}
	bool isDirectory() const { return S_ISDIR(mMode); }
	virtual MergedNodeList &subNodes();
	const VersionList *versionList() const { return &mVersionList; }

	virtual void getUrl(int pVersionIndex, QUrl *pComplete, QString *pRepoPath = nullptr, QString *pBranchName = nullptr,
				   quint64 *pCommitTime = nullptr, QString *pPathInRepo = nullptr) const = 0;
	virtual void askForIntegrityCheck() = 0;

public:
	VersionList mVersionList;
	MergedNodeList *mSubNodes;
	uint mMode;

protected:
	virtual void generateSubNodes() = 0;
};

class MergedRepository : public QObject
{
	Q_OBJECT
public:
	MergedRepository(QObject* parent = nullptr) : QObject(parent) {}
	virtual ~MergedRepository() {}
	virtual bool open() = 0;
	virtual bool readBranch() = 0;
	virtual bool permissionsOk() = 0;
	virtual MergedNode *rootNode() const = 0;
};

#endif // MERGEDVFS_H

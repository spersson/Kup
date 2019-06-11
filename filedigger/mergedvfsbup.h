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

#ifndef MERGEDVFSBUP_H
#define MERGEDVFSBUP_H

#include <git2.h>
uint qHash(git_oid pOid);
bool operator ==(const git_oid &pOidA, const git_oid &pOidB);
#include "mergedvfs.h"

struct VersionDataBup : public VersionData
{
    VersionDataBup(bool pChunkedFile, const git_oid *pOid, quint64 pCommitTime, quint64 pModifiedDate) :
		VersionData(pCommitTime, pModifiedDate), mOid(*pOid), mChunkedFile(pChunkedFile) {}
    VersionDataBup(const git_oid *pOid, quint64 pCommitTime, quint64 pModifiedDate, quint64 pSize) :
        VersionData(pCommitTime, pModifiedDate, pSize), mOid(*pOid) {}

    virtual quint64 size();

    git_oid mOid;
	bool mChunkedFile;
};

class MergedNodeBup : public MergedNode
{
    Q_OBJECT
    friend class VersionDataBup;
public:
    MergedNodeBup(QObject *pParent, const QString &pName, uint pMode);
	virtual void getUrl(int pVersionIndex, QUrl *pComplete, QString *pRepoPath = nullptr, QString *pBranchName = nullptr,
						quint64 *pCommitTime = nullptr, QString *pPathInRepo = nullptr) const;
	virtual void askForIntegrityCheck();

protected:
    virtual void generateSubNodes();

public:
    static git_repository *mRepository;

private:
	uint mMode;
};

class MergedRepositoryBup: public MergedRepository
{
    Q_OBJECT
public:
    MergedRepositoryBup(QObject *pParent, const QString &pRepositoryPath, const QString &pBranchName);
    virtual ~MergedRepositoryBup();

	virtual bool open();
	virtual bool readBranch();
	virtual bool permissionsOk();
	virtual MergedNode *rootNode() const;

    QString mBranchName;
	MergedNodeBup *mRoot;
};

#endif // MERGEDVFSBUP_H

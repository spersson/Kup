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

#ifndef MERGEDVFSRESTIC_H
#define MERGEDVFSRESTIC_H

#include <QObject>
#include <QRegularExpression>

#include "mergedvfs.h"

class VersionDataRestic : public VersionData
{
public:
	VersionDataRestic(quint64 pCommitTime, quint64 pModifiedDate, quint64 size, const QString &absPath) :
		VersionData(pCommitTime, pModifiedDate), mSize(size), mAbsPath(absPath) {}
    quint64 size() { return mSize; }
	QString absPath() { return mAbsPath; }

private:
    quint64 mSize;
	QString mAbsPath;
};

class MergedNodeRestic : public MergedNode
{
public:
    MergedNodeRestic(QObject *parent,
                     const QString &repoPath,
                     const QString &relPath,
                     const QString &name,
                     uint mode);
	virtual void getUrl(int pVersionIndex, QUrl *pComplete, QString *pRepoPath = nullptr, QString *pBranchName = nullptr,
						quint64 *pCommitTime = nullptr, QString *pPathInRepo = nullptr) const;
    virtual void askForIntegrityCheck() {}

protected:
    void generateSubNodes();

private:
    QString mSnapthosPath;
    QString mRelPath;
    QRegularExpression mRegexPath;
};

class MergedRepositoryResitc : public MergedRepository
{
    Q_OBJECT
public:
    MergedRepositoryResitc(QObject *pParent, const QString &pRepositoryPath);
    virtual ~MergedRepositoryResitc();

    virtual bool open() { return true; }
    virtual bool readBranch() { return true; }
    virtual bool permissionsOk() { return true; }
	virtual MergedNode *rootNode() const { return mRoot; }

    MergedNodeRestic *mRoot;
};

#endif // MERGEDVFSRESTIC_H

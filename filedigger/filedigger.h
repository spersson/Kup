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

#ifndef FILEDIGGER_H
#define FILEDIGGER_H

#include <KMainWindow>
#include <QUrl>

#include <QSharedPointer>
#include <QAbstractListModel>

#include <restichelper.h>
#include "fsminer.h"

#include "vfshelpers.h"

class KDirOperator;
class MergedVfsModel;
class MergedRepository;
class VersionListModel;
class QListView;
class QModelIndex;
class QTreeView;
class QLabel;
class FsMiner;

class BupMountLock : public MountLock
{
    Q_OBJECT
public:
    BupMountLock(const QString& repoPath, QObject* parent = nullptr);
    bool isLocked() override;

public slots:
    bool unlock() override;

private:
    QProcess mProcess;
};

class SnapshotListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    SnapshotListModel(QObject *parent = nullptr);
    void setSnapshots(const QList<BackupSnapshot> &snapshots);
    QVariant data(const QModelIndex &index, int role) const override;
    int rowCount(const QModelIndex &parent) const override;

private:
    QList<BackupSnapshot> mSnapshots;
};

class FileDigger : public KMainWindow
{
	Q_OBJECT
public:
	explicit FileDigger(BackupType type,
						const QString &pRepoPath,
						const QString &pBranchName,
						QWidget *pParent = nullptr);

protected slots:
	void updateVersionModel(const QModelIndex &pCurrent, const QModelIndex &pPrevious);
	void open(const QModelIndex &pIndex);
	void restore(const QModelIndex &pIndex);
	void copy(const QModelIndex &pIndex);
	void repoPathAvailable();
	void checkFileWidgetPath();
	void enterUrl(QUrl pUrl);
    void setInfo(QString info);
    void onBackupDescriptorReceived(BackupDescriptor desc);

signals:
    void mountSucceeded();

protected:
    void requestMount();
    virtual void closeEvent(QCloseEvent* event) override;

protected:
	MergedRepository *createRepo();
    void createRepoView();
	void createSelectionView();
	MergedVfsModel *mMergedVfsModel;
	QTreeView *mMergedVfsView;

	VersionListModel *mVersionModel;
	QListView *mVersionView;
    QListView *mSnapshotView;
	QString mRepoPath;
	QString mBranchName;
    QLabel* mInfoLabel;
	KDirOperator *mDirOperator;
	BackupType mBackupType;
    QSharedPointer<MountLock> mMountLock;
    QSharedPointer<FsMiner> mFsMiner;
    BackupDescriptor mDescriptor;
    SnapshotListModel mSnapshotModel;
    QTabWidget* m_tab;
};

#endif // FILEDIGGER_H

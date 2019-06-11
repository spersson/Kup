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

#ifndef RESTICHELPER_H
#define RESTICHELPER_H

#include <KProcess>
#include <KPtyProcess>

#include <QString>
#include <QStringList>
#include <QSharedPointer>
#include <QRegularExpression>
#include <QDir>
#include <QVersionNumber>

#include "resticforgetswitch.h"

#define QSL QStringLiteral
#define RESTIC_BIN QSL("restic")

struct ResticProcess : public KPtyProcess
{
    ResticProcess(const QString &repoPath);
    virtual ~ResticProcess() {}
};

class MountLock : public QObject
{
    Q_OBJECT
public:
    MountLock(const QString& mountPath, QObject* parent = nullptr) :
        QObject(parent), mMountPath(mountPath) {}

    virtual bool isLocked() = 0;
    virtual bool unlock() = 0;

    QString mountPath() { return mMountPath; }

signals:
    void servingData();

protected:
    bool umountMountPoint();
    bool removeMountPoint();

protected:
    QString mMountPath;
};

class ResticMountLock : public MountLock
{
	Q_OBJECT
public:
    ResticMountLock(const QString &repoPath, const QString& mountPath);
    bool isLocked() override;

public slots:
    bool unlock() override;

private slots:
	void onStdoutData();

private:
    ResticProcess mProcess;
    QString mOutput;
};

struct ResticProgressState
{
	enum State {
		S_SCANNING,
		S_TRANSFERING,
        S_BUILDING_NEW_INDEX,
        S_FIND_DATA_IN_USE,
        S_COUNTING,
        S_REMOVING
	};

	int mPerc;
	qlonglong speedBps;
	qint64 processedItems;
	qint64 processedBytes;
	qint64 totalItems;
	qint64 totalBytes;
	State state;
};

class ResticProgressProcess : public ResticProcess
{
	Q_OBJECT
public:
	ResticProgressProcess(const QString &repoPath);

protected:
	virtual bool processStepLine(const QString &line, ResticProgressState &state) = 0;
	virtual void emitStepSignal(ResticProgressState &state) = 0;
	virtual void emitCompletedSignal() = 0;
	virtual void emitFailedSignal() = 0;

private slots:
	void onStdoutData();
	void onFinished(int errorCode, QProcess::ExitStatus exitStatus);
};

class ResticForgetProcess : public ResticProgressProcess
{
	Q_OBJECT
public:
	ResticForgetProcess(const QString &repoPath, const QList<ResticForgetSwitch> &switches);

protected:
	virtual bool processStepLine(const QString &line, ResticProgressState &state);
	virtual void emitStepSignal(ResticProgressState &state) { emit forgetStep(state); }
	virtual void emitCompletedSignal() { emit forgetSucceeded(); }
	virtual void emitFailedSignal() { emit forgetFailed(); }

signals:
	void forgetStep(ResticProgressState &state);
	void forgetSucceeded();
	void forgetFailed();

private:
    QRegularExpression m_regex;
    ResticProgressState::State m_state;
};

class ResticBackupProcess : public ResticProgressProcess
{
	Q_OBJECT
public:
	ResticBackupProcess(const QString &repoPath,
						const QStringList &includedPaths,
                        const QStringList &excludedPaths,
                        const QString &regex);

protected:
    virtual bool processStepLine(const QString &line, ResticProgressState &state) = 0;

	void emitStepSignal(ResticProgressState &state) { emit backupStep(state); }
	void emitCompletedSignal() { emit backupCompleted(); }
	void emitFailedSignal() { emit backupFailed(); }

signals:
	void backupStep(ResticProgressState state);
	void backupCompleted();
	void backupFailed();

protected slots:
	qint64 processMeasureToBytes(const QString &number, const QString &unit);

protected:
	QRegularExpression mRegexTransfer;
};

class ResticBackupProcess08 : public ResticBackupProcess
{
    Q_OBJECT
public:
    ResticBackupProcess08(const QString &repoPath,
                          const QStringList &includedPaths,
                          const QStringList &excludedPaths) :
        ResticBackupProcess(repoPath, includedPaths, excludedPaths,
                            "\\[.*\\]\\s*([0-9\\.]*)%\\s*([0-9\\.]*)\\s*(GiB|KiB|MiB)"""
                            "\\s*\\/\\s*([0-9\\.]*)\\s*(GiB|KiB|MiB)\\s*(\\d*)\\s*\\/\\s*"
                            "(\\d*)[^\\d]*(\\d)*\\s*error") {}

protected:
    virtual bool processStepLine(const QString &line, ResticProgressState &state);
};

class ResticBackupProcess09 : public ResticBackupProcess
{
    Q_OBJECT
public:
    ResticBackupProcess09(const QString &repoPath,
                          const QStringList &includedPaths,
                          const QStringList &excludedPaths) :
        ResticBackupProcess(repoPath, includedPaths, excludedPaths,
                            "\\[\\d+:\\d+\\]\\s+([\\d+\\.]+)%\\s+(\\d+)\\s+files\\s+([\\d\\.]+)"
                            "\\s+(GiB|MiB|KiB|TiB),\\s+total\\s+\\d+\\s+files\\s+([\\d\\.]+)"
                            "\\s+(GiB|MiB|KiB|TiB),\\s+(\\d)+\\s+error") {}

protected:
    virtual bool processStepLine(const QString &line, ResticProgressState &state);
};

class ResticHelper
{
public:
    ResticHelper();

    static bool repoExists(const QString &repo);
    static bool repoInit(const QString &repo);
    static bool repoCheck(const QString &repo, QString &output);
    static ResticBackupProcess* repoBackup(const QString &repo,
                                           const QStringList &includedPaths,
                                           const QStringList &excludedPaths);
    static ResticMountLock *repoMount(const QString &repo,
                                      const QString &mountPath);
    static ResticForgetProcess *repoForget(const QString &repo,
                                           const QList<ResticForgetSwitch> &switches);

    static QStringList resticMountPaths();
    static QList<QDir> resticMountDirs();
    static QVersionNumber version();

private:
    static bool isRepoMounted(const QString& repo);
};

class ResticMountManager
{
public:
	static ResticMountManager& instance() {
		static ResticMountManager _instance;
		return _instance;
	}

    ResticMountLock *mount(const QString &repo);

    static QString generateMountPrivateDir();

private:
	ResticMountManager();
    static QString generateDirName();

private:
	ResticHelper mResticHelper;
};

#endif // RESTICHELPER_H

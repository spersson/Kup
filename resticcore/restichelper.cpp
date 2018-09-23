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

#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QUuid>
#include <QPair>

#include <KPtyDevice>
#include <KI18n/KLocalizedString>

#include <sys/mount.h>
#include <signal.h>

#include "restichelper.h"
#include "kupresticcore_debug.h"

inline double to_double_or_null(const QString &value)
{
	bool convOk = false;
	double val = value.toDouble(&convOk);
	return convOk ? val : 0;
}

inline qint64 to_longlong_or_null(const QString &value)
{
	bool convOk = false;
	qint64 val = value.toLongLong(&convOk);
	return convOk ? val : 0;
}

ResticProcess::ResticProcess(const QString &repoPath) : KPtyProcess()
{
    setOutputChannelMode(KProcess::MergedChannels);
    setPtyChannels(KPtyProcess::StdoutChannel);

    if (pty()->masterFd() == -1)
        qCWarning(KUPRESTICCORE) << "Master fd not set";

    setEnv("RESTIC_PASSWORD", "kup");
    *this << RESTIC_BIN << QSL("-r") << repoPath;
}

bool MountLock::umountMountPoint()
{
    // Try to unmount. If it fails, then report failure and do nothing else.
    return QProcess::execute(QSL("fusermount"), QStringList() << "-u" << mMountPath) == 0;
}

bool MountLock::removeMountPoint()
{
    // Remove mountpoint.
    if (!QDir().rmdir(mMountPath)) {
        qCWarning(KUPRESTICCORE) << "Failed to remove mountpoint " << mMountPath;
        return false;
    }

    return true;
}

ResticMountLock::ResticMountLock(const QString &repoPath, const QString &mountPath)
    : MountLock(mountPath)
    , mProcess(repoPath)
{
    connect(mProcess.pty(), SIGNAL(readyRead()),
            this, SLOT(onStdoutData()));

	qCDebug(KUPRESTICCORE) << "Mouting to " << mountPath;
    mProcess << QSL("mount") << mountPath;
    mProcess.start();
}

bool ResticMountLock::isLocked()
{
    return mProcess.state() != KPtyProcess::NotRunning;
}

bool ResticMountLock::unlock()
{
    umountMountPoint();

	// At this point this process should already be dead, but terminate
	// anyway for safety. SIGINT seems to be needed to also include unmount.
    ::kill(mProcess.pid(), SIGINT);
    if (!mProcess.waitForFinished()) {
		qCWarning(KUPRESTICCORE) << "The restic process seems to be stuck";

		// I don't return false here as the process seems to be stuck, but umount
		// reported success.
	}

    removeMountPoint();

	return true;
}

void ResticMountLock::onStdoutData()
{
    QByteArray data = mProcess.pty()->readAll();
	QString string = QString::fromUtf8(data);
	mOutput.append(string);

    qCWarning(KUPRESTICCORE) << "Data: " << mOutput;

	if (mOutput.contains("serving the repository at")) {
		emit servingData();
		mOutput.clear();
	}
}

ResticProgressProcess::ResticProgressProcess(const QString &repoPath) :
    ResticProcess(repoPath)
{
    connect(pty(), SIGNAL(readyRead()),
			this, SLOT(onStdoutData()));
	connect(this, SIGNAL(finished(int,QProcess::ExitStatus)),
            this, SLOT(onFinished(int,QProcess::ExitStatus)));
}

void ResticProgressProcess::onStdoutData()
{
	QTextStream stream(pty()->readAll());
	while (!stream.atEnd()) {
		QString line = stream.readLine();
		ResticProgressState state;
		if (!processStepLine(line, state))
			continue;
		emitStepSignal(state);
	}
}

void ResticProgressProcess::onFinished(int errorCode, QProcess::ExitStatus exitStatus)
{
	if (exitStatus == QProcess::CrashExit) {
		qCWarning(KUPRESTICCORE) << "Restic crashed";
		emitFailedSignal();
		return;
	}

	if (!errorCode) {
		qCInfo(KUPRESTICCORE) << "Restic completed process successfully";
		emitCompletedSignal();
		return;
	}

	// TODO: Send error info.
	qCWarning(KUPRESTICCORE) << "Restic failed procedure";
	emit emitFailedSignal();
}

ResticForgetProcess::ResticForgetProcess(const QString &repoPath,
										 const QList<ResticForgetSwitch> &switches) :
	ResticProgressProcess(repoPath)
  , m_regex("\\[\\d*:\\d*\\]\\s*([\\d\\.]+)%\\s+(\\d+)\\s*\\/\\s*(\\d+)\\s+packs")
  , m_state(ResticProgressState::S_BUILDING_NEW_INDEX)
{
	(*this) << QSL("forget");
	for (int i = 0; i < switches.size(); i++) {
		const ResticForgetSwitch &forgetSwitch = switches[i];
		(*this) << forgetSwitch.param << forgetSwitch.value;
	}

	(*this) << QSL("--prune");

	start();

    ResticProgressState state;
    state.mPerc = 0;
    state.speedBps = 0; // TODO: compute speed.
    state.processedItems = 0;
    state.processedBytes = 0;
    state.totalItems = 0;
    state.totalBytes = 0;
    state.state = ResticProgressState::S_BUILDING_NEW_INDEX;

    emit forgetStep(state);
}

bool ResticForgetProcess::processStepLine(const QString &line, ResticProgressState &state)
{
	if (line.contains(QStringLiteral("running prune"))) {
		state.mPerc = 0;
		state.speedBps = 0; // TODO: compute speed.
		state.processedItems = 0;
		state.processedBytes = 0;
		state.totalItems = 0;
		state.totalBytes = 0;
        state.state = ResticProgressState::S_BUILDING_NEW_INDEX;
		return true;
	}

    if (line.contains(QStringLiteral("building new index for repo"))) {
        m_state = ResticProgressState::S_BUILDING_NEW_INDEX;
        return true;
    }
    else if (line.contains("find data that is still in use")) {
        m_state = ResticProgressState::S_FIND_DATA_IN_USE;
        return true;
    }
    else if (line.contains("counting files in repo")) {
        m_state = ResticProgressState::S_COUNTING;
        return true;
    }
    else if (line.contains(QRegularExpression("remove [\\d]+ old index file"))) {
        m_state = ResticProgressState::S_REMOVING;
        return true;
    }

	QRegularExpressionMatch match = m_regex.match(line);
	if (!match.hasMatch())
		return false;

	qCDebug(KUPRESTICCORE) << "Step: " << match.captured(0);
    if (match.lastCapturedIndex() != 3)
        return false;

    QString sPerc = match.captured(1);
    QString sProcessedItems = match.captured(2);
    QString sTotalItems = match.captured(3);

    double perc = to_double_or_null(sPerc);
    qCWarning(KUPRESTICCORE) << "Done: " << perc << sProcessedItems;

    state.mPerc = qRound(perc);
    state.speedBps = 0; // TODO
    state.processedItems = to_longlong_or_null(sProcessedItems);
    state.processedBytes = 0;
    state.totalItems = to_longlong_or_null(sTotalItems);
    state.totalBytes = 0;
    state.state = m_state;

    return true;
}

ResticBackupProcess::ResticBackupProcess(const QString &repoPath,
										 const QStringList &includedPaths,
                                         const QStringList &excludedPaths,
                                         const QString &regex) :
	ResticProgressProcess(repoPath)
  , mRegexTransfer(regex)
{
	(*this) << QSL("backup") << includedPaths;

	foreach (QString excludedPath, excludedPaths)
		(*this) << QSL("-e") << excludedPath;

	start();
}

qint64 ResticBackupProcess::processMeasureToBytes(const QString &number, const QString &unit)
{
	bool ok = false;
	double value = number.toDouble(&ok);
	if (!ok)
		return -1;

#define KIB2B(b) qRound64(1024*b)
#define MIB2B(m) KIB2B(1024*m)
#define GIB2B(g) MIB2B(1024*g)

	if (unit == "GiB")
		return GIB2B(value);
	if (unit == "MiB")
		return MIB2B(value);
	if (unit == "KiB")
		return KIB2B(value);

	qCWarning(KUPRESTICCORE) << "Unknown unit " << unit;
	return 0;
}

bool ResticBackupProcess08::processStepLine(const QString &line, ResticProgressState &state)
{
    if (line.startsWith("scan", Qt::CaseInsensitive)) {
        state.mPerc = 0;
        state.speedBps = 0;
        state.processedItems = 0;
        state.processedBytes = 0;
        state.totalItems = 0;
        state.totalBytes = 0;
        state.state = ResticProgressState::S_SCANNING;
        return true;
    }

    QRegularExpressionMatch match = mRegexTransfer.match(line);
    if (!match.hasMatch())
        return false;

    qCDebug(KUPRESTICCORE) << "Step: " << match.captured(0);
    if (match.lastCapturedIndex() != 8)
        return false;

    QString sPerc = match.captured(1);
    QString sProcessed = match.captured(2);
    QString sProcessedUnit = match.captured(3);
    QString sTotal = match.captured(4);
    QString sTotalUnit = match.captured(5);
    QString sProcessedItems = match.captured(6);
    QString sTotalItems = match.captured(7);
    QString sErrors = match.captured(8);

    qint64 processed = processMeasureToBytes(sProcessed, sProcessedUnit);
    qint64 total = processMeasureToBytes(sTotal, sTotalUnit);
    double perc = to_double_or_null(sPerc);

    // TODO: Compute speed.
    state.mPerc = qMin(100, qMax(0, qRound(perc)));
    state.speedBps = 0;
    state.processedItems = to_longlong_or_null(sProcessedItems);
    state.processedBytes = processed;
    state.totalItems = to_longlong_or_null(sTotalItems);
    state.totalBytes = total;
    state.state = ResticProgressState::S_TRANSFERING;

    return true;
}

bool ResticBackupProcess09::processStepLine(const QString &line, ResticProgressState &state)
{
    if (line.startsWith("scan", Qt::CaseInsensitive)) {
        state.mPerc = 0;
        state.speedBps = 0;
        state.processedItems = 0;
        state.processedBytes = 0;
        state.totalItems = 0;
        state.totalBytes = 0;
        state.state = ResticProgressState::S_SCANNING;
        return true;
    }

    QRegularExpressionMatch match = mRegexTransfer.match(line);
    if (!match.hasMatch())
        return false;

    qCDebug(KUPRESTICCORE) << "Step: " << match.captured(0);
    if (match.lastCapturedIndex() != 8)
        return false;

    QString sPerc = match.captured(1);
    QString sProcessed = match.captured(3);
    QString sProcessedUnit = match.captured(4);
    QString sTotal = match.captured(6);
    QString sTotalUnit = match.captured(7);
    QString sProcessedItems = match.captured(2);
    QString sTotalItems = match.captured(5);
    QString sErrors = match.captured(8);

    qint64 processed = processMeasureToBytes(sProcessed, sProcessedUnit);
    qint64 total = processMeasureToBytes(sTotal, sTotalUnit);
    double perc = to_double_or_null(sPerc);

    // TODO: Compute speed.
    state.mPerc = qMin(100, qMax(0, qRound(perc)));
    state.speedBps = 0;
    state.processedItems = to_longlong_or_null(sProcessedItems);
    state.processedBytes = processed;
    state.totalItems = to_longlong_or_null(sTotalItems);
    state.totalBytes = total;
    state.state = ResticProgressState::S_TRANSFERING;

    return true;
}

ResticHelper::ResticHelper()
{

}

bool ResticHelper::repoExists(const QString &repo)
{
	ResticProcess proc(repo);
	proc << QSL("snapshots");

	return proc.execute() == 0;
}

bool ResticHelper::repoInit(const QString &repo)
{
	ResticProcess proc(repo);
	proc << QSL("init");

	return proc.execute() == 0;
}

bool ResticHelper::repoCheck(const QString &repo, QString &output)
{
	ResticProcess proc(repo);
	proc.setOutputChannelMode(ResticProcess::MergedChannels);
	proc << QSL("check");

	int ret = (proc.execute() == 0);
	output.clear();
	output.append(QString::fromUtf8(proc.readAllStandardOutput()));

	return ret;
}

ResticBackupProcess *ResticHelper::repoBackup(const QString &repo,
											  const QStringList &includedPaths,
											  const QStringList &excludedPaths)
{
    return new ResticBackupProcess08(repo, includedPaths, excludedPaths);
}

ResticMountLock *ResticHelper::repoMount(const QString &repo, const QString &mountPath)
{
	if (isRepoMounted(repo))
		return nullptr;

	return new ResticMountLock(repo, mountPath);
}

ResticForgetProcess *ResticHelper::repoForget(const QString &repo,
											  const QList<ResticForgetSwitch> &switches)
{
	return new ResticForgetProcess(repo, switches);
}

QStringList ResticHelper::resticMountPaths()
{
	QFile mounts("/proc/mounts");
	if (!mounts.exists()) {
		qCWarning(KUPRESTICCORE) << "Cannot find /proc/mounts table";
		return QStringList();
	}

	if (!mounts.open(QIODevice::ReadOnly)) {
		qCWarning(KUPRESTICCORE) << "Cannot open /proc/mounts for reading";
		return QStringList();
	}

	QStringList mountPaths;
	QTextStream stream(&mounts);
	QString line;
	while (stream.readLineInto(&line)) {
		QString line = stream.readLine();
		QStringList tokens = line.split(" ");
		if (tokens.size() > 2)
			if (tokens[0] == QSL("restic"))
				mountPaths.append(tokens[1]);
	}

	return mountPaths;
}

QList<QDir> ResticHelper::resticMountDirs()
{
	QList<QDir> dirs;
	QStringList paths = resticMountPaths();
	foreach (QString path, paths)
		dirs.append(QDir(path));

    return dirs;
}

QVersionNumber ResticHelper::version()
{
    KProcess lResticProcess;
    lResticProcess << QSL("restic") << QSL("version");
    lResticProcess.setOutputChannelMode(KProcess::MergedChannels);
    int code = lResticProcess.execute();
    if (code >= 0) {
        QString lOutput = QString::fromLocal8Bit(lResticProcess.readLine());
        QStringList tokens = lOutput.split(QLatin1Char(' '), QString::SkipEmptyParts);
        return QVersionNumber::fromString(tokens[1]);
    }

    return QVersionNumber();
}

bool ResticHelper::isRepoMounted(const QString &repo)
{
	QStringList mountPaths = resticMountPaths();
	for (int i = 0; i < mountPaths.size(); i++)
		if (QDir(repo) == QDir(mountPaths[i]))
			return true;

	return false;
}

ResticMountLock *ResticMountManager::mount(const QString &repo)
{
    qCDebug(KUPRESTICCORE) << "Mounting...";

    QString dataDirString = generateMountPrivateDir();
    if (dataDirString.isEmpty())
        return nullptr;

    QDir dataDir(dataDirString);

	qCDebug(KUPRESTICCORE) << "Created temporary mountpoint " << dataDir.absolutePath();
    return mResticHelper.repoMount(repo, dataDir.absolutePath());
}

QString ResticMountManager::generateMountPrivateDir()
{
    // Never empty according to Qt docs.
    QStringList paths = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    QDir dataDir = QDir(paths[0]);
    QString dirName = generateDirName();
    if (!dataDir.exists()) {
        if (!QDir().mkpath(dataDir.absolutePath())) {
            //*error = i18n("Could not write temporary data into the app private directory");
            qCWarning(KUPRESTICCORE) << "Could not create app temp dir " << dataDir.absolutePath();
            return QString();
        }
    }

    if (!dataDir.cd(dirName)) {
        qCDebug(KUPRESTICCORE) << "Trying to create mountpoint "
                           << dataDir.absolutePath() << QDir::separator() << dirName;
        if (!dataDir.mkdir(dirName) || !dataDir.cd(dirName)) {
            //*error = i18n("Failed to write temporary data into %1", dataDir.absolutePath());
            qCWarning(KUPRESTICCORE) << "Failed to create mountpoint in " << dataDir.absolutePath();
            return QString();
        }
    }

    return dataDir.absolutePath();
}

ResticMountManager::ResticMountManager()
{
	// Cleanup if possible.
	QStringList paths = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
	QDir dataDir = QDir(paths[0]);
	QFileInfoList infoList = dataDir.entryInfoList(
				QStringList() << "mount_*", QDir::Dirs | QDir::NoDotAndDotDot);
	foreach (QFileInfo info, infoList) {
		QDir mountDir(info.absoluteFilePath());
		if (mountDir.entryInfoList(QStringList(), QDir::NoDotAndDotDot).isEmpty()) {
			// Try to clean.
			if (!QDir().rmdir(info.absoluteFilePath()))
				qCWarning(KUPRESTICCORE) << "Cannot cleanup mount path " << info.absoluteFilePath();
		}
	}
}

QString ResticMountManager::generateDirName()
{
	QUuid uuid = QUuid::createUuid();
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
	return QString("mount_%1").arg(uuid.toString(QUuid::Id128));
#else
    return QString("mount_%1").arg(QString::fromLocal8Bit(uuid.toByteArray().toHex()));
#endif
}

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

#include <KProcess>
#include <KLocalizedString>

#include <QtMath>
#include <QTimer>

#include <signal.h>

#include "resticjob.h"
#include "kupdaemon_debug.h"

ResticJob::ResticJob(const BackupPlan &pBackupPlan,
					 const QString &pDestinationPath,
					 const QString &pLogFilePath,
					 KupDaemon *pKupDaemon) :
	BackupJob(pBackupPlan, pDestinationPath, pLogFilePath, pKupDaemon)
  , mProcessing(false)
{
	qputenv("RESTIC_PASSWORD", "kup");
	setCapabilities(KJob::Suspendable | KJob::Killable);
}

bool ResticJob::doKill()
{
    if (mBackupProcess && mBackupProcess->state() != KPtyProcess::NotRunning)
        ::kill(mBackupProcess->pid(), SIGINT);
    if (mForgetProcess && mForgetProcess->state() != KPtyProcess::NotRunning)
        ::kill(mForgetProcess->pid(), SIGINT);

#define TIMEOUT 10000

    if (mBackupProcess && !mBackupProcess->waitForFinished(TIMEOUT))
        return false;
    if (mForgetProcess && !mForgetProcess->waitForFinished(TIMEOUT))
        return false;

    return true;
}

bool ResticJob::doSuspend()
{
	return sendSignal(SIGSTOP);
}

bool ResticJob::doResume()
{
	return sendSignal(SIGCONT);
}

void ResticJob::performJob()
{
	if (mProcessing)
		return;

	mProcessing = true;
	mLogStream << QStringLiteral("Kup is starting restic backup job at ")
			   << QLocale().toString(QDateTime::currentDateTime())
			   << endl << endl;

	// Determine if command exists.
	if (!isCommandAvailable()) {
		jobFinishedError(ErrorWithoutLog, xi18nc("@info notification",
												 "The <application>restic</application> program is "
												 "needed but could not be found, maybe it is not installed?"));
		return;
	}

	// Determine if repo exists.
	if (!mResticHelper.repoExists(mDestinationPath)) {
		qCDebug(KUPDAEMON) << "Repo does not exist: init";
		if (!mResticHelper.repoInit(mDestinationPath)) {
			jobFinishedError(ErrorWithLog, xi18nc("@info notification", "Backup could not be initialized."));
			return;
		}

		// Repo initilized.
	}

	if(mBackupPlan.mCheckBackups) {
		emit description(this, i18n("Checking backup integrity"));

		QString output;
		if (!mResticHelper.repoCheck(mDestinationPath, output)) {
			mLogStream << output;
			jobFinishedError(ErrorWithLog, xi18nc("@info notification",
												  "Failed backup integrity check. Your backups could be corrupted! "
												  "See log file for more details."));
			return;
		}

		// Repo is not corrupted.
	}

	mBackupProcess.reset(mResticHelper.repoBackup(mDestinationPath,
												  mBackupPlan.mPathsIncluded,
												  mBackupPlan.mPathsExcluded));
	if (!mBackupProcess) {
		jobFinishedError(ErrorWithLog, xi18nc("@info notification",
											  "Failed to start backup process. "
											  "See log file for more details."));
		return;
	}

	connect(mBackupProcess.data(), SIGNAL(backupStep(ResticProgressState)),
			this, SLOT(onBackupStep(ResticProgressState)));
    connect(mBackupProcess.data(), SIGNAL(backupFailed()),
			this, SLOT(onBackupFailed()));
    connect(mBackupProcess.data(), SIGNAL(backupCompleted()),
			this, SLOT(onBackupCompleted()));
}

void ResticJob::onBackupStep(ResticProgressState state)
{
	if (!mProcessing)
		return;

	QString info = i18nc("notification", "Kup procedure is running");
	QString descriptionString;
	switch (state.state) {
	case ResticProgressState::S_SCANNING:
		descriptionString = i18n("Scanning directories...");
		break;
	case ResticProgressState::S_TRANSFERING:
		descriptionString = i18n("Transfering data...");
		break;
    case ResticProgressState::S_BUILDING_NEW_INDEX:
        descriptionString = i18n("Building new index...");
		break;
    case ResticProgressState::S_COUNTING:
        descriptionString = i18n("Counting files...");
        break;
    case ResticProgressState::S_FIND_DATA_IN_USE:
        descriptionString = i18n("Finding data in use...");
        break;
    case ResticProgressState::S_REMOVING:
        descriptionString = i18n("Removing old files...");
        break;
	}

    if (state.totalItems) {
        setTotalAmount(KJob::Files, static_cast<qulonglong>(state.totalItems));
        setProcessedAmount(KJob::Files, static_cast<qulonglong>(state.processedItems));
    }

    if (state.totalBytes) {
        setTotalAmount(KJob::Bytes, static_cast<qulonglong>(state.totalBytes));
        setProcessedAmount(KJob::Bytes, static_cast<qulonglong>(state.processedBytes));
    }

    setPercent(static_cast<unsigned long>(qMin(100, qMax(0, state.mPerc))));
    emitSpeed(static_cast<unsigned long>(state.speedBps));
	emit description(this, descriptionString,
					 QPair<QString, QString>(i18n("Executing plan"), mBackupPlan.mDescription),
					 QPair<QString, QString>(i18n("Action"), descriptionString));
	emit infoMessage(this, info, info);
}

void ResticJob::onBackupCompleted()
{
	mBackupProcess.reset(nullptr);

	mLogStream << endl << QSL("Kup successfully completed the bup backup job at ")
			   << QLocale().toString(QDateTime::currentDateTime()) << endl;

	QList<ResticForgetSwitch> switches = buildSwitchList();
	if (switches.isEmpty()) {
		onForgetCompleted();
		return;
	}

	// TODO: I should probably consider a case where the backup succeeds and the
	// forget fails.
	mForgetProcess.reset(mResticHelper.repoForget(mDestinationPath, switches));
	connect(mForgetProcess.data(), &ResticForgetProcess::forgetFailed,
			this, &ResticJob::onForgetFailed);
	connect(mForgetProcess.data(), &ResticForgetProcess::forgetSucceeded,
			this, &ResticJob::onForgetCompleted);
    connect(mForgetProcess.data(), &ResticForgetProcess::forgetStep,
            this, &ResticJob::onBackupStep);
}

void ResticJob::onBackupFailed()
{
	// TODO: Provide more info.
	mBackupProcess.reset(nullptr);
	mLogStream << endl << QSL("Kup did not successfully complete the bup backup job: "
							  "failed to save everything.") << endl;
	jobFinishedError(ErrorWithLog, xi18nc("@info notification", "Failed to save backup. "
																"See log file for more details."));
	mProcessing = false;
}

void ResticJob::onForgetCompleted()
{
	mForgetProcess.reset(nullptr);
	mLogStream << endl << QSL("Kup successfully dropped backups accoding to the policy at ")
			   << QLocale().toString(QDateTime::currentDateTime()) << endl;

	jobFinishedSuccess();
	mProcessing = false;
}

void ResticJob::onForgetFailed()
{
	// TODO: provide more info.
	mForgetProcess.reset(nullptr);
	mLogStream << endl << QSL("Kup could not drop older backups according to the selected policy");

	jobFinishedError(ErrorWithLog, xi18nc("@info notification",
										 "Failed to drop older backups"));
	mProcessing = false;
}

bool ResticJob::isCommandAvailable()
{
	QStringList args = QStringList()
			<< QSL("-c")
			<< QSL("which restic &> /dev/null");
	return QProcess::execute(QSL("bash"), args) == 0;
}

bool ResticJob::sendSignal(int signal)
{
    if (mBackupProcess && mBackupProcess->state() != KPtyProcess::NotRunning) {
        if (::kill(mBackupProcess->pid(), signal)) {
            qCWarning(KUPDAEMON) << "Failed to "
                                 << (signal == SIGSTOP ? "suspend" : "resume")
                                 << " restic backup: " << strerror(errno);
            return false;
        }
    }

    if (mForgetProcess && mForgetProcess->state() != KPtyProcess::NotRunning) {
        if (::kill(mForgetProcess->pid(), signal)) {
            qCWarning(KUPDAEMON) << "Failed to "
                                 << (signal == SIGSTOP ? "suspend" : "resume")
                                 << " restic forget: " << strerror(errno);
            return false;
        }
    }

    return true;
}

QList<ResticForgetSwitch> ResticJob::buildSwitchList()
{
	QList<ResticForgetSwitch> switches;
	if (mBackupPlan.mKeepLastN && mBackupPlan.mKeepLastNValue > 0)
		switches.append(ResticForgetKeepLastN(mBackupPlan.mKeepLastNValue));
	if (mBackupPlan.mKeepHourly && mBackupPlan.mKeepHourlyValue > 0)
		switches.append(ResticForgetKeepHourly(mBackupPlan.mKeepHourlyValue));
	if (mBackupPlan.mKeepDaily && mBackupPlan.mKeepDailyValue > 0)
		switches.append(ResticForgetKeepDaily(mBackupPlan.mKeepDailyValue));
	if (mBackupPlan.mKeepMonthly && mBackupPlan.mKeepDailyValue > 0)
		switches.append(ResticForgetKeepMonthly(mBackupPlan.mKeepMonthlyValue));
	if (mBackupPlan.mKeepYearly && mBackupPlan.mKeepYearlyValue > 0)
		switches.append(ResticForgetKeepYearly(mBackupPlan.mKeepYearlyValue));
	if (mBackupPlan.mKeepWithinDuration
			&& mBackupPlan.mKeepWithinDurationDays > 0
			&& mBackupPlan.mKeepWithinDurationMonths > 0
			&& mBackupPlan.mKeepWithinDurationYears > 0)
		switches.append(ResticForgetKeepWithinDuration(
							mBackupPlan.mKeepWithinDurationYears,
							mBackupPlan.mKeepWithinDurationMonths,
							mBackupPlan.mKeepWithinDurationDays));

	return switches;
}

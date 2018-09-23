#include <QDir>

#include "fsminerbup.h"
#include "kupfiledigger_debug.h"

FsMinerBup::FsMinerBup(const QString &mountPath, QObject *parent) :
    FsMiner(mountPath, parent)
{

}

QString FsMinerBup::pathToLatest()
{
    return QString("%1%2%3%2latest")
            .arg(mMountPath).arg(QDir::separator()).arg("kup");
}

void FsMinerBup::doProcess()
{
#define EMIT_AND_RETURN(snapshots) {                          \
        emit backupProcessed(BackupDescriptor { snapshots }); \
        return;                                               \
    }

#define EMIT_AND_RETURN_EMPTY EMIT_AND_RETURN(QList<BackupSnapshot>())

    QString snapshotsPath = QString("%1%2kup")
            .arg(mMountPath).arg(QDir::separator());
    QDir snapshotsDir(snapshotsPath);
    if (!snapshotsDir.exists()) {
        qCWarning(KUPFILEDIGGER) << "Cannot find snapshots dir";
        EMIT_AND_RETURN_EMPTY;
    }

    QFileInfoList snapshotListDir = snapshotsDir
            .entryInfoList(QStringList(), QDir::Dirs | QDir::NoDotAndDotDot);
    BackupDescriptor descriptor;
    foreach (QFileInfo fileInfo, snapshotListDir) {
        QDateTime snapshotDateTime =
                QDateTime::fromString(fileInfo.fileName(), QStringLiteral("yyyy-MM-dd-hhmmss"));
        if (snapshotDateTime.isNull() || !snapshotDateTime.isValid())
            continue;
        BackupSnapshot snaphot {
            snapshotDateTime,
            fileInfo.absoluteFilePath()
        };
        descriptor.mSnapshots.append(snaphot);
    }

    emit backupProcessed(descriptor);
}

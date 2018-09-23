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

#ifndef FSMINER_H
#define FSMINER_H

#include <QObject>
#include <QDateTime>

struct BackupSnapshot
{
    QDateTime mDateTime;
    QString mAbsPath;
};

struct BackupDescriptor
{
    QList<BackupSnapshot> mSnapshots;
};

class FsMiner : public QObject
{
    Q_OBJECT
public:
    explicit FsMiner(const QString &mountPath, QObject *parent = nullptr);

public slots:
    void process();

	virtual QString pathToLatest() = 0;

signals:
    void backupProcessed(BackupDescriptor descriptor);

protected:
    virtual void doProcess() = 0;

protected:
    QString mMountPath;
};

#endif // FSMINER_H

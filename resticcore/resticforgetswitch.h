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

#ifndef RESTICFORGETSWITCH_H
#define RESTICFORGETSWITCH_H

#include <QString>

class ResticForgetSwitch
{
public:
    ResticForgetSwitch(const QString &param, const QString &value);

    QString param;
    QString value;
};

#define QSL QStringLiteral

class ResticForgetKeepLastN : public ResticForgetSwitch
{
public:
    ResticForgetKeepLastN(int value) :
        ResticForgetSwitch(QSL("--keep-last"), QString::number(value)) {}
};

class ResticForgetKeepHourly : public ResticForgetSwitch
{
public:
    ResticForgetKeepHourly(int value) :
        ResticForgetSwitch(QSL("--keep-hourly"), QString::number(value)) {}
};

class ResticForgetKeepDaily : public ResticForgetSwitch
{
public:
    ResticForgetKeepDaily(int value) :
        ResticForgetSwitch(QSL("--keep-daily"), QString::number(value)) {}
};

class ResticForgetKeepWeekly : public ResticForgetSwitch
{
public:
    ResticForgetKeepWeekly(int value) :
        ResticForgetSwitch(QSL("--keep-weekly"), QString::number(value)) {}
};

class ResticForgetKeepMonthly : public ResticForgetSwitch
{
public:
    ResticForgetKeepMonthly(int value) :
        ResticForgetSwitch(QSL("--keep-monthly"), QString::number(value)) {}
};

class ResticForgetKeepYearly : public ResticForgetSwitch
{
public:
    ResticForgetKeepYearly(int value) :
        ResticForgetSwitch(QSL("--keep-yearly"), QString::number(value)) {}
};

class ResticForgetKeepWithinDuration : public ResticForgetSwitch
{
public:
    ResticForgetKeepWithinDuration(int years, int months, int days) :
        ResticForgetSwitch(QSL("--keep-within"),
                           QString("%1y%2m%3d").arg(years).arg(months).arg(days)) {}
};

#undef QSL

#endif // RESTICFORGETSWITCH_H

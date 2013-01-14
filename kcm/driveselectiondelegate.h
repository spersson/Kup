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

#ifndef DRIVESELECTIONDELEGATE_H
#define DRIVESELECTIONDELEGATE_H

#include <QStyledItemDelegate>

class KCapacityBar;

class DriveSelectionDelegate : public QStyledItemDelegate
{
public:
	DriveSelectionDelegate(QObject *pParent = 0);
	virtual void paint(QPainter* pPainter, const QStyleOptionViewItem& pOption, const QModelIndex& pIndex) const;
	virtual QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const;
private:
	KCapacityBar *mCapacityBar;
};

#endif // DRIVESELECTIONDELEGATE_H

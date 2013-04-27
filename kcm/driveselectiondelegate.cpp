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

#include "backupplan.h"
#include "driveselectiondelegate.h"
#include "driveselection.h"

#include <QApplication>
#include <QPainter>
#include <QStyle>

#include <kcapacitybar.h>
#include <KLocale>
#include <kio/global.h>

static const int cMargin = 6;

DriveSelectionDelegate::DriveSelectionDelegate(QObject *pParent)
   : QStyledItemDelegate(pParent)
{
	mCapacityBar = new KCapacityBar(KCapacityBar::DrawTextInline);
}

void DriveSelectionDelegate::paint(QPainter* pPainter, const QStyleOptionViewItem& pOption,
                                   const QModelIndex& pIndex) const {
	pPainter->save();
	pPainter->setRenderHint(QPainter::Antialiasing);

	QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &pOption, pPainter);

	KIO::filesize_t lTotalSize = pIndex.data(DriveSelection::TotalSpace).value<KIO::filesize_t>();
	KIO::filesize_t lUsedSize = pIndex.data(DriveSelection::UsedSpace).value<KIO::filesize_t>();

	bool lIsDisconnected = pIndex.data(DriveSelection::UDI).toString().isEmpty();

	if(lTotalSize == 0 || lIsDisconnected) {
		mCapacityBar->setValue(0);
	} else {
		mCapacityBar->setValue((lUsedSize * 100) / lTotalSize);
	}
	mCapacityBar->drawCapacityBar(pPainter, pOption.rect.adjusted(cMargin,
	                                                              cMargin+QApplication::fontMetrics().height()+cMargin,
	                                                              -cMargin, -cMargin));

	if (pOption.state & QStyle::State_Selected)
		pPainter->setPen(pOption.palette.color(QPalette::HighlightedText));
	else
		pPainter->setPen(pOption.palette.color(QPalette::Text));

	KLocale *lLocale = KGlobal::locale();
	QString lDisplayLabel, lPartitionLabel, lDisconnectedLabel;
	if(lIsDisconnected) {
		lDisconnectedLabel = i18nc("@item:inlistbox this text is added if selected drive is disconnected", " (disconnected)");
	} else {
		lDisconnectedLabel = QString();
		QString lFreeSpace = i18nc("@label %1 is amount of free storage space of hard drive","%1 free",
		                           lLocale->formatByteSize(lTotalSize - lUsedSize, 1));
		pPainter->drawText(pOption.rect.topRight() + QPoint(-(cMargin+QApplication::fontMetrics().width(lFreeSpace)),
		                                                    cMargin+QApplication::fontMetrics().height()), lFreeSpace);
	}

	QString lDeviceDescription = pIndex.data(DriveSelection::DeviceDescription).toString();
	QString lLabel = pIndex.data(DriveSelection::Label).toString();
	int lPartitionNumber = pIndex.data(DriveSelection::PartitionNumber).toInt();
	if(lLabel.isEmpty() || lLabel == lDeviceDescription) {
		if(pIndex.data(DriveSelection::PartitionsOnDrive).toInt() > 1) {
			lPartitionLabel = i18nc("@item:inlistbox used for unnamed filesystems, more than one filesystem on device. %1 is partition number, %2 is device description, %3 is either empty or the \" (disconnected)\" text",
			                        "Partition %1 on %2%3", lPartitionNumber, lDeviceDescription, lDisconnectedLabel);
		} else {
			lPartitionLabel = i18nc("@item:inlistbox used when there is only one unnamed filesystem on device. %1 is device description, %2 is either empty or the \" (disconnected)\" text",
			                        "%1%2", lDeviceDescription, lDisconnectedLabel);
		}
	} else {
		lPartitionLabel = i18nc("@item:inlistbox %1 is filesystem label, %2 is the device description, %3 is either empty or the \" (disconnected)\" text",
		                        "%1 on %2%3", lLabel, lDeviceDescription, lDisconnectedLabel);
	}

	if(lTotalSize == 0) {
		lDisplayLabel = lPartitionLabel;
	} else {
		lDisplayLabel = i18nc("@item:inlistbox %1 is drive(partition) label, %2 is storage capacity",
		                      "%1: %2 total capacity", lPartitionLabel, lLocale->formatByteSize(lTotalSize, 1));
	}
	pPainter->drawText(pOption.rect.topLeft() + QPoint(cMargin, cMargin+QApplication::fontMetrics().height()), lDisplayLabel);

	pPainter->restore();
}

QSize DriveSelectionDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
	Q_UNUSED(option)
	QSize size;
	size.setHeight(cMargin*5 + QApplication::fontMetrics().height());
	size.setWidth(cMargin*2 + QApplication::fontMetrics().width(index.data().toString()));
	return size;
}

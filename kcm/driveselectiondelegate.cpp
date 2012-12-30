/*
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "backupplan.h"
#include "driveselectiondelegate.h"

#include <QApplication>
#include <QPainter>
#include <QStyle>

#include <KDiskFreeSpaceInfo>
#include <kcapacitybar.h>
#include <KGlobal>
#include <KLocale>
#include <Solid/Device>
#include <Solid/StorageDrive>
#include <Solid/StorageVolume>
#include <Solid/StorageAccess>

static const int cMargin = 6;

void DriveSelectionDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
	painter->save();
	painter->setRenderHint(QPainter::Antialiasing);

	QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter);

	KLocale *lLocale = KGlobal::locale();

	QString udi = index.data(Qt::UserRole+1).toString();
	QString uuid = index.data(Qt::UserRole+2).toString();
	QString label = index.data().toString();
	QString freeSpace;

	KIO::filesize_t lTotalSize = index.data(Qt::UserRole + 3).value<KIO::filesize_t>();
	KIO::filesize_t lUsedSize = index.data(Qt::UserRole + 4).value<KIO::filesize_t>();

	if(lTotalSize == 0) {
		QList<Solid::Device> storageVolumes = Solid::Device::listFromType(Solid::DeviceInterface::StorageVolume, udi);
		foreach(Solid::Device drive, storageVolumes) {
			Solid::StorageVolume *volume = drive.as<Solid::StorageVolume>();
			if (volume->uuid() == uuid) {
				Solid::StorageAccess *access = drive.as<Solid::StorageAccess>();
				if (access) {
					if(!access->isAccessible())
						access->setup();
					else {
						KDiskFreeSpaceInfo info = KDiskFreeSpaceInfo::freeSpaceInfo(access->filePath());
						if (info.isValid()) {
							label = i18nc("@label %1 is drive(partition) label, %2 is storage capacity",
							              "%1: %2 total capacity", volume->label(), lLocale->formatByteSize(info.size(), 1));
							freeSpace = i18nc("@label %1 is amount of free storage space of hard drive","%1 free",
							                  lLocale->formatByteSize(info.size() - info.used(), 1));
							KCapacityBar capacityBar(KCapacityBar::DrawTextInline);
							capacityBar.setValue((info.used() * 100) / info.size());
							capacityBar.drawCapacityBar(painter, option.rect.adjusted(cMargin, cMargin+QApplication::fontMetrics().height()+cMargin, -cMargin, -cMargin));
						}
					}
				}
				break;
			}
		}
	}
	else {
		KCapacityBar capacityBar(KCapacityBar::DrawTextInline);
		capacityBar.setValue((lUsedSize * 100) / lTotalSize);
		capacityBar.drawCapacityBar(painter, option.rect.adjusted(cMargin, cMargin+QApplication::fontMetrics().height()+cMargin, -cMargin, -cMargin));

		label = i18nc("@label %1 is drive(partition) label, %2 is storage capacity",
		         "%1: %2 total capacity", index.data().toString(), lLocale->formatByteSize(lTotalSize, 1));
		freeSpace = i18nc("@label %1 is amount of free storage space of hard drive","%1 free",
		                  lLocale->formatByteSize(lTotalSize - lUsedSize, 1));
	}
	if (option.state & QStyle::State_Selected)
		painter->setPen(option.palette.color(QPalette::HighlightedText));
	else
		painter->setPen(option.palette.color(QPalette::Text));

	painter->drawText(option.rect.topLeft() + QPoint(cMargin, cMargin+QApplication::fontMetrics().height()), label);
	painter->drawText(option.rect.topRight() + QPoint(-(cMargin+QApplication::fontMetrics().width(freeSpace)), cMargin+QApplication::fontMetrics().height()), freeSpace);
	painter->restore();
}

QSize DriveSelectionDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
	Q_UNUSED(option)
	QSize size;
	size.setHeight(cMargin*5 + QApplication::fontMetrics().height());
	size.setWidth(cMargin*2 + QApplication::fontMetrics().width(index.data().toString()));
	return size;
}


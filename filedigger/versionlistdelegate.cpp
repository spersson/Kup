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

#include "versionlistdelegate.h"
#include "versionlistmodel.h"

#include <KLocale>
#include <QAbstractItemView>
#include <QAbstractItemModel>
#include <QParallelAnimationGroup>
#include <QApplication>
#include <QPainter>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QPushButton>

#define cMargin 4

VersionItemAnimation::VersionItemAnimation(QList<QWidget *> &pWidgets, const QModelIndex pIndex, QObject *pParent)
   : QParallelAnimationGroup(pParent), mIndex(pIndex), mWidgets(pWidgets)
{
	QPropertyAnimation *lHeight = new QPropertyAnimation(this, "extraHeight", this);
	lHeight->setStartValue(0.0);
	lHeight->setEndValue(1.0);
	lHeight->setDuration(1000);
	addAnimation(lHeight);

	QParallelAnimationGroup *lOpacityAnimations = new QParallelAnimationGroup(this);
	addAnimation(lOpacityAnimations);
	foreach(QWidget *lWidget, pWidgets) {
		QGraphicsOpacityEffect *lEffect = new QGraphicsOpacityEffect(lWidget);
		lEffect->setOpacity(0.0);
		lWidget->setGraphicsEffect(lEffect);
		QPropertyAnimation *lWidgetOpacityAnimation = new QPropertyAnimation(lEffect, "opacity", this);
		lWidgetOpacityAnimation->setStartValue(0.0);
		lWidgetOpacityAnimation->setEndValue(1.0);
		lWidgetOpacityAnimation->setDuration(1000);
		lOpacityAnimations->addAnimation(lWidgetOpacityAnimation);
	}
}

void VersionItemAnimation::setExtraHeight(float pExtraHeight) {
	mCurrentHeight = pExtraHeight;
	emit sizeChanged(mIndex);
}

VersionListDelegate::VersionListDelegate(QAbstractItemView *pItemView, QObject *pParent) :
   KWidgetItemDelegate(pItemView, pParent)
{
	mData = new VersionListDelegateData;
	mView = pItemView;
	mModel = pItemView->model();
	connect(pItemView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),
	        SLOT(updateCurrent(QModelIndex,QModelIndex)));
	connect(pItemView->model(), SIGNAL(modelReset()), SLOT(reset()));
	connect(&mData->mOpenMapper, SIGNAL(mapped(int)), SIGNAL(openRequested(int)));
	connect(&mData->mRestoreMapper, SIGNAL(mapped(int)), SIGNAL(restoreRequested(int)));
}

VersionListDelegate::~VersionListDelegate() {
	delete mData;
}

void VersionListDelegate::paint(QPainter *pPainter, const QStyleOptionViewItem &pOption, const QModelIndex &pIndex) const {
	pPainter->save();
	QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &pOption, pPainter);
	pPainter->setPen(pOption.palette.color(pOption.state & QStyle::State_Selected
	                                       ? QPalette::HighlightedText: QPalette::Text));
	pPainter->drawText(pOption.rect.topLeft() + QPoint(cMargin, cMargin+QApplication::fontMetrics().height()),
	                   pIndex.data().toString());
	pPainter->restore();
}

QSize VersionListDelegate::sizeHint(const QStyleOptionViewItem &pOption, const QModelIndex &pIndex) const {
	if(!pIndex.isValid()) {
		return QSize(0,0);
	}
	VersionItemAnimation *lAnimation = mData->mAnimations.value(pIndex);
	if(lAnimation == NULL) {
		return QSize(0,0);
	}
	int lWidth = 0;
	foreach (QWidget *lWidget, lAnimation->mWidgets) {
		lWidth += lWidget->width();
	}
	return QSize(cMargin*2 + lWidth, cMargin*2 + pOption.fontMetrics.height() +
	             lAnimation->extraHeight()*(2*cMargin + lAnimation->mWidgets.at(0)->height()));
}


void VersionListDelegate::updateCurrent(const QModelIndex &pCurrent, const QModelIndex &pPrevious) {
	if(pPrevious.isValid()) {
		VersionItemAnimation *lPrevAnim = mData->mAnimations.value(pPrevious);
		if(lPrevAnim != NULL) {
			lPrevAnim->setDirection(QAbstractAnimation::Backward);
			lPrevAnim->start();
		}
	}
	if(pCurrent.isValid()) {
		VersionItemAnimation *lCurAnim = mData->mAnimations.value(pCurrent);
		if(lCurAnim != NULL) {
			lCurAnim->setDirection(QAbstractAnimation::Forward);
			lCurAnim->start();
		}
	}
}

void VersionListDelegate::updateHeight(const QModelIndex &pIndex) {
	QStyleOptionViewItemV4 lStyleOption;
	lStyleOption.initFrom(mView->viewport());
	lStyleOption.rect = mView->visualRect(pIndex);
	updateItemWidgets(mData->mAnimations.value(pIndex)->mWidgets, lStyleOption, pIndex);
	emit sizeHintChanged(pIndex);
}

void VersionListDelegate::reset() {
	qDeleteAll(mData->mAnimations);
	mData->mAnimations.clear();
}

Q_DECLARE_METATYPE(QModelIndex)

QList<QWidget *> VersionListDelegate::createItemWidgets() const {
	QList<QWidget *> lWidgetList;
	QModelIndex lIndex = property("goya:creatingWidgetForIndex").value<QModelIndex>();
	QList<QEvent::Type> lBlockedEventList;
	lBlockedEventList << QEvent::MouseButtonPress << QEvent::MouseButtonRelease
	                  << QEvent::MouseButtonDblClick << QEvent::KeyPress << QEvent::KeyRelease;

	QPushButton *lOpen = new QPushButton(i18nc("@action:button", "Open"));
	lWidgetList.append(lOpen);
	mData->mOpenMapper.setMapping(lOpen, lIndex.row());
	connect(lOpen, SIGNAL(clicked()), &mData->mOpenMapper, SLOT(map()));
	setBlockedEventTypes(lOpen, lBlockedEventList);

	QPushButton *lRestore = new QPushButton(i18nc("@action:button", "Restore"));
	lWidgetList.append(lRestore);
	mData->mRestoreMapper.setMapping(lRestore, lIndex.row());
	connect(lRestore, SIGNAL(clicked()), &mData->mRestoreMapper, SLOT(map()));
	setBlockedEventTypes(lRestore, lBlockedEventList);

	VersionItemAnimation *lAnimation = new VersionItemAnimation(lWidgetList, lIndex);
	connect(lAnimation, SIGNAL(sizeChanged(QModelIndex)),
	        this, SLOT(updateHeight(QModelIndex)));
	mData->mAnimations.insert(lIndex, lAnimation);
	return lWidgetList;
}

void VersionListDelegate::updateItemWidgets(const QList<QWidget *> pWidgets,
                                            const QStyleOptionViewItem &pOption,
                                            const QPersistentModelIndex &pIndex) const {
	QPushButton *lOpen = static_cast<QPushButton *>(pWidgets.at(0));
	QPushButton *lRestore = static_cast<QPushButton *>(pWidgets.at(1));

	if(pIndex.isValid()) {
		QSize lOpenSize = lOpen->sizeHint();
		lOpen->resize(lOpenSize);
		QSize lRestoreSize = lRestore->sizeHint();
		lRestore->resize(lRestoreSize);

		lRestore->move(pOption.rect.width() - cMargin - lRestoreSize.width(),
		               pOption.rect.height() - cMargin - lRestoreSize.height());

		lOpen->move(lRestore->x() - cMargin - lOpenSize.width(), lRestore->y());
	}
}


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

#ifndef VERSIONDELEGATE_H
#define VERSIONDELEGATE_H

#include <QAbstractItemDelegate>
#include <QParallelAnimationGroup>
#include <QSignalMapper>


struct Button {
	Button(){}
	Button(QString pText, QWidget *pParent);
	bool mPushed;
	QStyleOptionButton mStyleOption;
	QWidget *mParent;

	void setPosition(const QPoint &pTopRight);
	void paint(QPainter *pPainter, float pOpacity);
	bool event(QEvent *pEvent);
};

class VersionItemAnimation : public QParallelAnimationGroup
{
	Q_OBJECT
	Q_PROPERTY(float extraHeight READ extraHeight WRITE setExtraHeight)
	Q_PROPERTY(float opacity READ opacity WRITE setOpacity)

public:
	VersionItemAnimation(QWidget *pParent);
	float extraHeight() {return mExtraHeight;}
	float opacity() {return mOpacity;}

signals:
	void sizeChanged(const QModelIndex &pIndex);

public slots:
	void setExtraHeight(float pExtraHeight);
	void setOpacity(float pOpacity) {mOpacity = pOpacity;}

public:
	QPersistentModelIndex mIndex;
	float mExtraHeight;
	float mOpacity;
	Button mOpenButton;
	Button mRestoreButton;
};

class VersionListDelegate : public QAbstractItemDelegate
{
	Q_OBJECT
public:
	explicit VersionListDelegate(QAbstractItemView *pItemView, QObject *pParent = 0);
	~VersionListDelegate();
	virtual void paint(QPainter *pPainter, const QStyleOptionViewItem &pOption, const QModelIndex &pIndex) const;
	virtual QSize sizeHint(const QStyleOptionViewItem &pOption, const QModelIndex &pIndex) const;
	virtual bool eventFilter(QObject *pObject, QEvent *pEvent);

signals:
	void updateRequested(const QModelIndex &pIndex);
	void openRequested(const QModelIndex &pIndex);
	void restoreRequested(const QModelIndex &pIndex);

public slots:
	void updateCurrent(const QModelIndex &pCurrent, const QModelIndex &pPrevious);
	void reset();
	void reclaimAnimation();

protected:
	void initialize();
	QAbstractItemView *mView;
	QAbstractItemModel *mModel;
	QHash<QPersistentModelIndex, VersionItemAnimation *> mActiveAnimations;
	QList<VersionItemAnimation *> mInactiveAnimations;
};

#endif // VERSIONDELEGATE_H

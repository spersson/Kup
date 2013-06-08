#ifndef VERSIONDELEGATE_H
#define VERSIONDELEGATE_H

#include <KWidgetItemDelegate>
#include <QParallelAnimationGroup>
#include <QSignalMapper>


class VersionItemAnimation : public QParallelAnimationGroup
{
	Q_OBJECT
	Q_PROPERTY(float extraHeight READ extraHeight WRITE setExtraHeight)

public:
	VersionItemAnimation(QList<QWidget *> &pWidgets, const QModelIndex pIndex, QObject *pParent = 0);
	int row() {return mIndex.row();}
	QPersistentModelIndex &index() {return mIndex;}
	float extraHeight() {return mCurrentHeight;}

signals:
	void sizeChanged(const QModelIndex &pIndex);

public slots:
	void setIndex(const QModelIndex &pIndex) {mIndex = pIndex;}
	void setExtraHeight(float pExtraHeight);

public:
	QPersistentModelIndex mIndex;
	float mCurrentHeight;
	QList<QWidget *> mWidgets;
};


// that virtual method createWidgets() is const. Work around:
struct VersionListDelegateData {
	QHash<QPersistentModelIndex, VersionItemAnimation *> mAnimations;
	QSignalMapper mOpenMapper;
	QSignalMapper mRestoreMapper;
};

class VersionListDelegate : public KWidgetItemDelegate
{
	Q_OBJECT
public:
	explicit VersionListDelegate(QAbstractItemView *pItemView, QObject *pParent = 0);
	~VersionListDelegate();
	virtual void paint(QPainter *pPainter, const QStyleOptionViewItem &pOption, const QModelIndex &pIndex) const;
	virtual QSize sizeHint(const QStyleOptionViewItem &pOption, const QModelIndex &pIndex) const;

signals:
	void updateRequested(const QModelIndex &pIndex);
	void openRequested(int pRow);
	void restoreRequested(int pRow);

public slots:
	void updateCurrent(const QModelIndex &pCurrent, const QModelIndex &pPrevious);
	void updateHeight(const QModelIndex &pIndex);
	void reset();


protected:
	virtual QList<QWidget *> createItemWidgets() const;
	virtual void updateItemWidgets(const QList<QWidget *> pWidgets, const QStyleOptionViewItem &pOption,
	                               const QPersistentModelIndex &pIndex) const;
	QAbstractItemView *mView;
	QAbstractItemModel *mModel;
	VersionListDelegateData *mData;
};

#endif // VERSIONDELEGATE_H

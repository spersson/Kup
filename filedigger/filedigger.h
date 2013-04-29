#ifndef FILEDIGGER_H
#define FILEDIGGER_H

#include <QSplitter>
class MergedVfsModel;
class VersionListModel;
class QTreeView;
class QListView;
class QModelIndex;

class FileDigger : public QSplitter
{
	Q_OBJECT
public:
	explicit FileDigger(const QString &pRepositoryPath, const QString &pBranchName, QWidget *parent = 0);

protected slots:
	void updateVersionModel(const QModelIndex &pCurrent, const QModelIndex &pPrevious);
	void open(const QModelIndex &pIndex);

protected:
	MergedVfsModel *mTreeModel;
	QTreeView *mTreeView;

	VersionListModel *mVersionModel;
	QListView *mListView;
};

#endif // FILEDIGGER_H

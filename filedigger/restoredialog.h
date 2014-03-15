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

#ifndef RESTOREDIALOG_H
#define RESTOREDIALOG_H

#include "versionlistmodel.h"

#include <KIO/Job>
#include <QDialog>
#include <QFileInfo>

namespace Ui {
class RestoreDialog;
}

//class KFileTreeView;
class KDirSelectDialog;
class KFileWidget;
class KMessageWidget;
class KWidgetJobTracker;
class QSignalMapper;
class QTreeWidget;

class RestoreDialog : public QDialog
{
	Q_OBJECT

public:
	explicit RestoreDialog(const BupSourceInfo &pPathInfo, QWidget *parent = 0);
	~RestoreDialog();

protected:
	void changeEvent(QEvent *pEvent);

protected slots:
	void setOriginalDestination();
	void setCustomDestination();
	void checkDestinationSelection();
	void checkDestinationSelection2();
	void startPrechecks();
	void collectSourceListing(KIO::Job *pJob, const KIO::UDSEntryList &pEntryList);
	void sourceListingCompleted(KJob *pJob);
	void completePrechecks();
	void fileOverwriteConfirmed();
	void startRestoring();
	void restoringCompleted(KJob *pJob);
	void fileMoveCompleted(KJob *pJob);
	void folderMoveCompleted(KJob *pJob);
	void createNewFolder();
	void openDestinationFolder();

private:
	void checkForExistingFiles(const KIO::UDSEntryList &pEntryList);
	void moveFolder();
	Ui::RestoreDialog *mUI;
	KFileWidget *mFileWidget;
	KDirSelectDialog *mDirDialog;
	QFileInfo mDestination;
	QFileInfo mFolderToCreate;
	QString mRestorationPath; // not neccesarily same as destination
	BupSourceInfo mSourceInfo;
	quint64 mDestinationSize; //size of files about to be overwritten
	quint64 mSourceSize; //size of files about to be read
	KMessageWidget *mMessageWidget;
	QSignalMapper *mSignalMapper;
	QString mSavedWorkingDirectory;
	QString mSourceFileName;
	QHash<QString, quint64> mFileSizes;
	int mDirectoriesCount;
	KWidgetJobTracker *mJobTracker;
};

#endif // RESTOREDIALOG_H

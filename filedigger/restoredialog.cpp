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

#include "restoredialog.h"
#include "ui_restoredialog.h"
#include "restorejob.h"
#include "dirselector.h"
#include "kuputils.h"

#include <KIO/CopyJob>
#include <KDiskFreeSpaceInfo>
#include <KFileWidget>
#include <KLocalizedString>
#include <KMessageBox>
#include <KMessageWidget>
#include <KProcess>
#include <KRun>
#include <KWidgetJobTracker>

#include <QDir>
#include <QInputDialog>
#include <QPushButton>
#include <QSignalMapper>
#include <QTimer>

#define KUP_TMP_RESTORE_FOLDER QStringLiteral("_kup_temporary_restore_folder_")

RestoreDialog::RestoreDialog(const BupSourceInfo &pPathInfo, QWidget *parent)
   : QDialog(parent), mUI(new Ui::RestoreDialog), mSourceInfo(pPathInfo)
{
	mSourceFileName = mSourceInfo.mPathInRepo.section(QDir::separator(), -1);

	mUI->setupUi(this);

	mFileWidget = NULL;
	mDirSelector = NULL;
	mJobTracker = NULL;

	mUI->mRestoreOriginalButton->setMinimumHeight(mUI->mRestoreOriginalButton->sizeHint().height() * 2);
	mUI->mRestoreCustomButton->setMinimumHeight(mUI->mRestoreCustomButton->sizeHint().height() * 2);

	connect(mUI->mRestoreOriginalButton, SIGNAL(clicked()), SLOT(setOriginalDestination()));
	connect(mUI->mRestoreCustomButton, SIGNAL(clicked()), SLOT(setCustomDestination()));

	mMessageWidget = new KMessageWidget(this);
	mMessageWidget->setWordWrap(true);
	mUI->mTopLevelVLayout->insertWidget(0, mMessageWidget);
	connect(mUI->mDestBackButton, SIGNAL(clicked()), mMessageWidget, SLOT(hide()));
	connect(mUI->mDestNextButton, SIGNAL(clicked()), SLOT(checkDestinationSelection()));

	mSignalMapper = new QSignalMapper(this);
	connect(mSignalMapper, SIGNAL(mapped(int)), mUI->mStackedWidget, SLOT(setCurrentIndex(int)));
	mSignalMapper->setMapping(mUI->mDestBackButton, 0);
	connect(mUI->mDestBackButton, SIGNAL(clicked()), mSignalMapper, SLOT(map()));
	mSignalMapper->setMapping(mUI->mOverwriteBackButton, 0);
	connect(mUI->mOverwriteBackButton, SIGNAL(clicked()), mSignalMapper, SLOT(map()));

	connect(mUI->mConfirmButton, SIGNAL(clicked()), SLOT(fileOverwriteConfirmed()));
	connect(mUI->mOpenDestinationButton, SIGNAL(clicked()), SLOT(openDestinationFolder()));
}

RestoreDialog::~RestoreDialog() {
	delete mUI;
}

void RestoreDialog::changeEvent(QEvent *pEvent) {
	QDialog::changeEvent(pEvent);
	switch (pEvent->type()) {
	case QEvent::LanguageChange:
		mUI->retranslateUi(this);
		break;
	default:
		break;
	}
}

void RestoreDialog::setOriginalDestination() {
	if(mSourceInfo.mIsDirectory) {
		// the path in repo could have had slashes appended below, we are back here because user clicked "back"
		ensureNoTrailingSlash(mSourceInfo.mPathInRepo);
		//select parent of folder to be restored
		mDestination.setFile(mSourceInfo.mPathInRepo.section(QDir::separator(), 0, -2));
	} else {
		mDestination.setFile(mSourceInfo.mPathInRepo);
	}
	startPrechecks();
}

void RestoreDialog::setCustomDestination() {
	if(mSourceInfo.mIsDirectory && mDirSelector == NULL) {
		mDirSelector = new DirSelector(this);
		mDirSelector->setRootUrl(QUrl::fromLocalFile(QStringLiteral("/")));
		QString lDirPath = mSourceInfo.mPathInRepo.section(QDir::separator(), 0, -2);
		mDirSelector->expandToUrl(QUrl::fromLocalFile(lDirPath));
		mUI->mDestinationVLayout->insertWidget(0, mDirSelector);

		QPushButton *lNewFolderButton = new QPushButton(QIcon::fromTheme(QStringLiteral("folder-new")),
		                                                xi18nc("@action:button","New Folder..."));
		connect(lNewFolderButton, SIGNAL(clicked()), SLOT(createNewFolder()));
		mUI->mDestinationHLayout->insertWidget(0, lNewFolderButton);
	} else if(!mSourceInfo.mIsDirectory && mFileWidget == NULL) {
		QFileInfo lFileInfo(mSourceInfo.mPathInRepo);
		do {
			lFileInfo.setFile(lFileInfo.absolutePath()); // check the file's directory first, not the file.
		} while(!lFileInfo.exists());
		QUrl lStartSelection = QUrl::fromLocalFile(lFileInfo.absoluteFilePath() + '/' + mSourceFileName);
		mFileWidget = new KFileWidget(lStartSelection, this);
		mFileWidget->setOperationMode(KFileWidget::Saving);
		mFileWidget->setMode(KFile::File | KFile::LocalOnly);
		mUI->mDestinationVLayout->insertWidget(0, mFileWidget);
	}
	mUI->mDestNextButton->setFocus();
	mUI->mStackedWidget->setCurrentIndex(1);
}

void RestoreDialog::checkDestinationSelection() {
	if(mSourceInfo.mIsDirectory) {
		QUrl lUrl = mDirSelector->url();
		if(!lUrl.isEmpty()) {
			mDestination.setFile(lUrl.path());
			startPrechecks();
		} else {
			mMessageWidget->setText(xi18nc("@info message bar appearing on top",
			                              "No destination was selected, please select one."));
			mMessageWidget->setMessageType(KMessageWidget::Error);
			mMessageWidget->animatedShow();
		}
	} else {
		connect(mFileWidget, SIGNAL(accepted()), SLOT(checkDestinationSelection2()));
		mFileWidget->slotOk(); // will emit accepted() if selection is valid, continue below then
	}
}

void RestoreDialog::checkDestinationSelection2() {
	mFileWidget->accept(); // This call is needed for selectedFile() to return something.

	QString lFilePath = mFileWidget->selectedFile();
	if(!lFilePath.isEmpty()) {
		mDestination.setFile(lFilePath);
		startPrechecks();
	} else {
		mMessageWidget->setText(xi18nc("@info message bar appearing on top",
		                              "No destination was selected, please select one."));
		mMessageWidget->setMessageType(KMessageWidget::Error);
		mMessageWidget->animatedShow();
	}
}

void RestoreDialog::startPrechecks() {
	mUI->mFileConflictList->clear();
	mSourceSize = 0;
	mFileSizes.clear();

	if(mSourceInfo.mIsDirectory) {
		mDirectoriesCount = 1; // the folder being restored, rest will be added during listing.
		mRestorationPath = mDestination.absoluteFilePath();
		mFolderToCreate = QFileInfo(mDestination.absoluteFilePath() + QDir::separator() + mSourceFileName);
		mSavedWorkingDirectory.clear();
		if(mFolderToCreate.exists()) {
			if(mFolderToCreate.isDir()) {
				// destination dir exists, first restore to a subfolder, then move files up.
				mRestorationPath = mFolderToCreate.absoluteFilePath();
				QDir lDir(mFolderToCreate.absoluteFilePath());
				lDir.setFilter(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
				if(lDir.count() > 0) { // destination dir exists and is non-empty.
					mRestorationPath.append(QDir::separator());
					mRestorationPath.append(KUP_TMP_RESTORE_FOLDER);
				}
				// make bup not restore the source folder itself but instead it's contents
				mSourceInfo.mPathInRepo.append(QDir::separator());
				// folder already exists, need to check for files about to be overwritten.
				// will create QFileInfos with relative paths during listing and compare with listed source entries.
				mSavedWorkingDirectory = QDir::currentPath();
				QDir::setCurrent(mFolderToCreate.absoluteFilePath());
			} else {
				mUI->mFileConflictList->addItem(mFolderToCreate.absoluteFilePath());
				mRestorationPath.append(QDir::separator());
				mRestorationPath.append(KUP_TMP_RESTORE_FOLDER);
			}
		}
		KIO::ListJob *lListJob = KIO::listRecursive(mSourceInfo.mBupKioPath, KIO::HideProgressInfo);
		connect(lListJob, SIGNAL(entries(KIO::Job*,KIO::UDSEntryList)),
		        SLOT(collectSourceListing(KIO::Job*,KIO::UDSEntryList)));
		connect(lListJob, SIGNAL(result(KJob*)), SLOT(sourceListingCompleted(KJob*)));
		lListJob->start();
	} else {
		mDirectoriesCount = 0;
		mSourceSize = mSourceInfo.mSize;
		mFileSizes.insert(mSourceFileName, mSourceInfo.mSize);
		mRestorationPath = mDestination.absolutePath();
		if(mDestination.exists() || mDestination.fileName() != mSourceFileName) {
			mRestorationPath.append(QDir::separator());
			mRestorationPath.append(KUP_TMP_RESTORE_FOLDER);
			if(mDestination.exists()) {
				mUI->mFileConflictList->addItem(mDestination.absoluteFilePath());
			}
		}
		completePrechecks();
	}
}

void RestoreDialog::collectSourceListing(KIO::Job *pJob, const KIO::UDSEntryList &pEntryList) {
	Q_UNUSED(pJob)
	KIO::UDSEntryList::ConstIterator it = pEntryList.begin();
	const KIO::UDSEntryList::ConstIterator end = pEntryList.end();
	for(; it != end; ++it) {
		QString lEntryName = it->stringValue(KIO::UDSEntry::UDS_NAME);
		if(it->isDir()) {
			if(lEntryName != QStringLiteral(".") && lEntryName != QStringLiteral("..")) {
				mDirectoriesCount++;
			}
		} else {
			if(!it->isLink()) {
				quint64 lEntrySize = it->numberValue(KIO::UDSEntry::UDS_SIZE);
				mSourceSize += lEntrySize;
				mFileSizes.insert(mSourceFileName + QDir::separator() + lEntryName, lEntrySize);
			}
			if(!mSavedWorkingDirectory.isEmpty()) {
				QFileInfo lFileInfo(lEntryName);
				if(lFileInfo.exists()) {
					mUI->mFileConflictList->addItem(lEntryName);
				}
			}
		}
	}
}

void RestoreDialog::sourceListingCompleted(KJob *pJob) {
	if(!mSavedWorkingDirectory.isEmpty()) {
		QDir::setCurrent(mSavedWorkingDirectory);
	}
	if(pJob->error() != 0) {
		mMessageWidget->setText(xi18nc("@info message bar appearing on top",
		                              "There was a problem while getting a list of all files to restore: %1",
		                              pJob->errorString()));
		mMessageWidget->setMessageType(KMessageWidget::Error);
		mMessageWidget->animatedShow();
	} else {
		completePrechecks();
	}
}

void RestoreDialog::completePrechecks() {
	KDiskFreeSpaceInfo lSpaceInfo = KDiskFreeSpaceInfo::freeSpaceInfo(mDestination.absolutePath());
	if(lSpaceInfo.isValid() && lSpaceInfo.available() < mSourceSize) {
		mMessageWidget->setText(xi18nc("@info message bar appearing on top",
		                              "The destination does not have enough space available. "
		                              "Please choose a different destination or free some space."));
		mMessageWidget->setMessageType(KMessageWidget::Error);
		mMessageWidget->animatedShow();
	} else if(mUI->mFileConflictList->count() > 0) {
		if(mSourceInfo.mIsDirectory) {
			QString lDateString = QLocale().toString(QDateTime::fromTime_t(mSourceInfo.mCommitTime).toLocalTime());
			lDateString.replace(QLatin1Char('/'), QLatin1Char('-')); // make sure no slashes in suggested folder name
			mUI->mNewFolderNameEdit->setText(mSourceFileName +
			                                 xi18nc("added to the suggested filename when restoring, %1 is the time when backup was taken",
			                                       " - saved at %1", lDateString));
			mUI->mConflictTitleLabel->setText(xi18nc("@info", "Folder already exists, please choose a solution"));
		} else {
			mUI->mOverwriteRadioButton->setChecked(true);
			mUI->mOverwriteRadioButton->hide();
			mUI->mNewNameRadioButton->hide();
			mUI->mNewFolderNameEdit->hide();
			mUI->mConflictTitleLabel->setText(xi18nc("@info", "File already exists"));
		}
		mUI->mStackedWidget->setCurrentIndex(2);
	} else {
		startRestoring();
	}
}

void RestoreDialog::fileOverwriteConfirmed() {
	if(mSourceInfo.mIsDirectory && mUI->mNewNameRadioButton->isChecked()) {
		QFileInfo lNewFolderInfo(mDestination.absoluteFilePath() + QDir::separator() + mUI->mNewFolderNameEdit->text());
		if(lNewFolderInfo.exists()) {
			mMessageWidget->setText(xi18nc("@info message bar appearing on top",
			                              "The new name entered already exists, please enter a different one."));
			mMessageWidget->setMessageType(KMessageWidget::Error);
			mMessageWidget->animatedShow();
			return;
		} else {
			mFolderToCreate = QFileInfo(mDestination.absoluteFilePath() + QDir::separator() + mUI->mNewFolderNameEdit->text());
			mRestorationPath = mFolderToCreate.absoluteFilePath();
			if(!mSourceInfo.mPathInRepo.endsWith(QDir::separator())) {
				mSourceInfo.mPathInRepo.append(QDir::separator());
			}
		}
	}
	startRestoring();
}

void RestoreDialog::startRestoring() {
	QString lSourcePath(QDir::separator());
	lSourcePath.append(mSourceInfo.mBranchName);
	lSourcePath.append(QDir::separator());
	QDateTime lCommitTime = QDateTime::fromTime_t(mSourceInfo.mCommitTime);
	lSourcePath.append(lCommitTime.toString(QStringLiteral("yyyy-MM-dd-hhmmss")));
	lSourcePath.append(mSourceInfo.mPathInRepo);
	RestoreJob *lRestoreJob = new RestoreJob(mSourceInfo.mRepoPath, lSourcePath, mRestorationPath,
	                                         mDirectoriesCount, mSourceSize, mFileSizes);
	if(mJobTracker == NULL) {
		mJobTracker = new KWidgetJobTracker(this);
	}
	mJobTracker->registerJob(lRestoreJob);
	QWidget *lProgressWidget = mJobTracker->widget(lRestoreJob);
	mUI->mRestoreProgressLayout->insertWidget(1, lProgressWidget);
	lProgressWidget->show();
	connect(lRestoreJob, SIGNAL(result(KJob*)), SLOT(restoringCompleted(KJob*)));
	lRestoreJob->start();
	mUI->mCloseButton->hide();
	mUI->mStackedWidget->setCurrentIndex(3);
}

void RestoreDialog::restoringCompleted(KJob *pJob) {
	if(pJob->error() != 0) {
		mUI->mRestorationOutput->setPlainText(pJob->errorText());
		mUI->mRestorationStackWidget->setCurrentIndex(1);
		mUI->mCloseButton->show();
	} else {
		if(!mSourceInfo.mIsDirectory && mSourceFileName != mDestination.fileName()) {
			KIO::CopyJob *lFileMoveJob = KIO::move(QUrl::fromLocalFile(mRestorationPath + '/' + mSourceFileName),
			                                       QUrl::fromLocalFile(mRestorationPath + '/' + mDestination.fileName()),
			                                       KIO::HideProgressInfo);
			connect(lFileMoveJob, SIGNAL(result(KJob*)), SLOT(fileMoveCompleted(KJob*)));
			lFileMoveJob->start();
		} else {
			moveFolder();
		}
	}
}

void RestoreDialog::fileMoveCompleted(KJob *pJob) {
	if(pJob->error() != 0) {
		mUI->mRestorationOutput->setPlainText(pJob->errorText());
		mUI->mRestorationStackWidget->setCurrentIndex(1);
	} else {
		moveFolder();
	}
}

void RestoreDialog::folderMoveCompleted(KJob *pJob) {
	mUI->mCloseButton->show();
	if(pJob->error() != 0) {
		mUI->mRestorationOutput->setPlainText(pJob->errorText());
		mUI->mRestorationStackWidget->setCurrentIndex(1);
	} else {
		mUI->mRestorationStackWidget->setCurrentIndex(2);
	}
}

void RestoreDialog::createNewFolder() {
	bool lUserAccepted;
	QUrl lUrl = mDirSelector->url();
	QString lNameSuggestion = xi18nc("default folder name when creating a new folder", "New Folder");
	if(QFileInfo(lUrl.adjusted(QUrl::StripTrailingSlash).path() + '/' + lNameSuggestion).exists()) {
		lNameSuggestion = KIO::suggestName(lUrl, lNameSuggestion);
	}

	QString lSelectedName = QInputDialog::getText(this, xi18nc("@title:window", "New Folder" ),
	                                              xi18nc("@label:textbox", "Create new folder in:\n%1", lUrl.path()),
	                                              QLineEdit::Normal, lNameSuggestion, &lUserAccepted);

	if (!lUserAccepted)
		return;

	QUrl lPartialUrl(lUrl);
	const QStringList lDirectories = lSelectedName.split(QDir::separator(), QString::SkipEmptyParts);
	foreach(QString lSubDirectory, lDirectories) {
		QDir lDir(lPartialUrl.path());
		if(lDir.exists(lSubDirectory)) {
			lPartialUrl = lPartialUrl.adjusted(QUrl::StripTrailingSlash);
			lPartialUrl.setPath(lPartialUrl.path() + '/' + (lSubDirectory));
			KMessageBox::sorry(this, i18n("A folder named %1 already exists.", lPartialUrl.path()));
			return;
		}
		if(!lDir.mkdir(lSubDirectory)) {
			lPartialUrl = lPartialUrl.adjusted(QUrl::StripTrailingSlash);
			lPartialUrl.setPath(lPartialUrl.path() + '/' + (lSubDirectory));
			KMessageBox::sorry(this, i18n("You do not have permission to create %1.", lPartialUrl.path()));
			return;
		}
		lPartialUrl = lPartialUrl.adjusted(QUrl::StripTrailingSlash);
		lPartialUrl.setPath(lPartialUrl.path() + '/' + (lSubDirectory));
	}
	mDirSelector->expandToUrl(lPartialUrl);
}

void RestoreDialog::openDestinationFolder() {
	KRun::runUrl(QUrl::fromLocalFile(mSourceInfo.mIsDirectory ?
	                                    mFolderToCreate.absoluteFilePath() :
	                                    mDestination.absolutePath()),
	             QStringLiteral("inode/directory"), NULL);
}

void RestoreDialog::moveFolder() {
	if(!mRestorationPath.endsWith(KUP_TMP_RESTORE_FOLDER)) {
		mUI->mRestorationStackWidget->setCurrentIndex(2);
		mUI->mCloseButton->show();
		return;
	}
	KIO::CopyJob *lFolderMoveJob = KIO::moveAs(QUrl::fromLocalFile(mRestorationPath),
	                                     QUrl::fromLocalFile(mRestorationPath.section(QDir::separator(), 0, -2)),
	                                     KIO::Overwrite | KIO::HideProgressInfo);
	connect(lFolderMoveJob, SIGNAL(result(KJob*)), SLOT(folderMoveCompleted(KJob*)));
	mJobTracker->registerJob(lFolderMoveJob);
	QWidget *lProgressWidget = mJobTracker->widget(lFolderMoveJob);
	mUI->mRestoreProgressLayout->insertWidget(1, lProgressWidget);
	lProgressWidget->show();
	lFolderMoveJob->start();
}


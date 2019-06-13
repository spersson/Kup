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

#include "dirselector.h"

#include <KDirLister>
#include <KDirModel>
#include <KLocalizedString>
#include <KMessageBox>

#include <QDir>
#include <QInputDialog>

DirSelector::DirSelector(QWidget *pParent) : QTreeView(pParent) {
	mDirModel = new KDirModel(this);
	mDirModel->dirLister()->setDirOnlyMode(true);
	setModel(mDirModel);
	for (int i = 1; i < mDirModel->columnCount(); ++i) {
		hideColumn(i);
	}
	setHeaderHidden(true);
	connect(mDirModel, SIGNAL(expand(QModelIndex)), SLOT(expand(QModelIndex)));
	connect(mDirModel, SIGNAL(expand(QModelIndex)), SLOT(selectEntry(QModelIndex)));
}

QUrl DirSelector::url() const {
	const KFileItem lFileItem = mDirModel->itemForIndex(currentIndex());
	return !lFileItem.isNull() ? lFileItem.url() : QUrl();
}

void DirSelector::createNewFolder() {
	bool lUserAccepted;
	QString lNameSuggestion = xi18nc("default folder name when creating a new folder", "New Folder");
	if(QFileInfo::exists(url().adjusted(QUrl::StripTrailingSlash).path() + '/' + lNameSuggestion)) {
		lNameSuggestion = KIO::suggestName(url(), lNameSuggestion);
	}

	QString lSelectedName = QInputDialog::getText(this, xi18nc("@title:window", "New Folder" ),
	                                              xi18nc("@label:textbox", "Create new folder in:\n%1", url().path()),
	                                              QLineEdit::Normal, lNameSuggestion, &lUserAccepted);
	if (!lUserAccepted)
		return;

	QUrl lPartialUrl(url());
	const QStringList lDirectories = lSelectedName.split(QLatin1Char('/'), QString::SkipEmptyParts);
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
	mDirModel->expandToUrl(lPartialUrl);
}

void DirSelector::selectEntry(QModelIndex pIndex) {
	selectionModel()->clearSelection();
	selectionModel()->setCurrentIndex(pIndex, QItemSelectionModel::SelectCurrent);
	scrollTo(pIndex);
}

void DirSelector::expandToUrl(const QUrl &pUrl) {
	mDirModel->expandToUrl(pUrl);
}

void DirSelector::setRootUrl(const QUrl &pUrl) {
	mDirModel->dirLister()->openUrl(pUrl);
}


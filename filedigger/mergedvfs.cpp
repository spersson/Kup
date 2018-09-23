/***************************************************************************
 *   Copyright Luca Carlon                                                 *
 *   carlon.luca@gmail.com                                                 *
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

#include "kupdaemon.h"
#include "mergedvfs.h"
#include "vfshelpers.h"
#include "kupfiledigger_debug.h"

#include <QDebug>
#include <QDir>

MergedNode::MergedNode(QObject *pParent, const QString &pName, uint pMode) :
	QObject(pParent)
{
	mSubNodes = nullptr;
	setObjectName(pName);
	mMode = pMode;
}

MergedNodeList &MergedNode::subNodes() {
	if(mSubNodes == nullptr) {
		mSubNodes = new MergedNodeList();
		if(isDirectory()) {
			generateSubNodes();
		}
	}
	return *mSubNodes;
}

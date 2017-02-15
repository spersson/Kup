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
#include "vfshelpers.h"

#include <QBuffer>
#include <QByteArray>
#include <QDateTime>

#include <unistd.h>
#include <sys/stat.h>

#define RECORD_END 0
#define RECORD_PATH 1
#define RECORD_COMMON 2 // times, user, group, type, perms, etc. (legacy version 1)
#define RECORD_SYMLINK_TARGET 3
#define RECORD_POSIX1E_ACL 4 // getfacl(1), setfacl(1), etc.
#define RECORD_NFSV4_ACL 5 // intended to supplant posix1e acls?
#define RECORD_LINUX_ATTR 6 // lsattr(1) chattr(1)
#define RECORD_LINUX_XATTR 7 // getfattr(1) setfattr(1)
#define RECORD_HARDLINK_TARGET 8 // hard link target
#define RECORD_COMMON_V2 9 // times, user, group, type, perms, etc.


VintStream::VintStream(const void *pData, int pSize, QObject *pParent)
   : QObject(pParent)
{
	mByteArray = QByteArray::fromRawData((char *)pData, pSize);
	mBuffer = new QBuffer(&mByteArray, this);
	mBuffer->open(QIODevice::ReadOnly);
}

VintStream &VintStream::operator>>(qint64 &pInt) {
	char c;
	if(!mBuffer->getChar(&c)) {
		throw 1;
	}
	int lOffset = 6;
	bool lNegative = (c & 0x40);
	pInt = c & 0x3F;
	while(c & 0x80) {
		if(!mBuffer->getChar(&c)) {
			throw 1;
		}
		pInt |= (c & 0x7F) << lOffset;
		lOffset += 7;
	}
	if(lNegative) {
		pInt = -pInt;
	}
	return *this;
}

VintStream &VintStream::operator >>(quint64 &pInt) {
	char c;
	int lOffset = 0;
	pInt = 0;
	do {
		if(!mBuffer->getChar(&c)) {
			throw 1;
		}
		pInt |= (c & 0x7F) << lOffset;
		lOffset += 7;
	} while(c & 0x80);
	return *this;
}

VintStream &VintStream::operator >>(QString &pString) {
	QByteArray lBytes;
	*this >> lBytes;
	pString = QString::fromUtf8(lBytes);
	return *this;
}

VintStream &VintStream::operator >>(QByteArray &pByteArray) {
	quint64 lByteCount;
	*this >> lByteCount;
	pByteArray.resize(lByteCount);
	if(mBuffer->read(pByteArray.data(), pByteArray.length()) < pByteArray.length()) {
		throw 1;
	}
	return *this;
}

quint64 Metadata::mDefaultUid;
quint64 Metadata::mDefaultGid;
bool Metadata::mDefaultsResolved = false;

Metadata::Metadata(quint64 pMode) {
	mMode = pMode;
	mAtime = 0;
	mMtime = 0;
	if(!mDefaultsResolved) {
		mDefaultUid = getuid();
		mDefaultGid = getgid();
		mDefaultsResolved = true;
	}
	mUid = mDefaultUid;
	mGid = mDefaultGid;
}

int readMetadata(VintStream &pMetadataStream, Metadata &pMetadata) {
	try {
		quint64 lTag;
		do {
			pMetadataStream >> lTag;
			switch(lTag) {
			case RECORD_COMMON: {
				qint64 lNotUsedInt;
				quint64 lNotUsedUint, lTempUint;
				QString lNotUsedString;
				pMetadataStream >> lNotUsedUint >> lTempUint;
				pMetadata.mMode = lTempUint;
				pMetadataStream >> lTempUint >> lNotUsedString; // user name
				pMetadata.mUid = lTempUint;
				pMetadataStream >> lTempUint >> lNotUsedString; // group name
				pMetadata.mGid = lTempUint;
				pMetadataStream >> lNotUsedUint; // device number
				pMetadataStream >> pMetadata.mAtime >> lNotUsedUint; //nanoseconds
				pMetadataStream >> pMetadata.mMtime >> lNotUsedUint; // nanoseconds
				pMetadataStream >> lNotUsedInt >> lNotUsedUint; // status change time
				break;
			}
			case RECORD_COMMON_V2: {
				qint64 lNotUsedInt;
				quint64 lNotUsedUint;
				QString lNotUsedString;
				pMetadataStream >> lNotUsedUint >> pMetadata.mMode;
				pMetadataStream >> pMetadata.mUid >> lNotUsedString; // user name
				pMetadataStream >> pMetadata.mGid >> lNotUsedString; // group name
				pMetadataStream >> lNotUsedInt; // device number
				pMetadataStream >> pMetadata.mAtime >> lNotUsedUint; //nanoseconds
				pMetadataStream >> pMetadata.mMtime >> lNotUsedUint; // nanoseconds
				pMetadataStream >> lNotUsedInt >> lNotUsedUint; // status change time
				break;
			}
			case RECORD_SYMLINK_TARGET: {
				pMetadataStream >> pMetadata.mSymlinkTarget;
				break;
			}
			default: {
				if(lTag != RECORD_END) {
					QByteArray lNotUsed;
					pMetadataStream >> lNotUsed;
				}
				break;
			}
			}
		} while(lTag != RECORD_END);
	} catch(int) {
		return 1;
	}
	return 0; // success
}

quint64 calculateChunkFileSize(const git_oid *pOid, git_repository *pRepository) {
	quint64 lLastChunkOffset = 0;
	quint64 lLastChunkSize = 0;
	uint lMode;
	do {
		git_tree *lTree;
		if(0 != git_tree_lookup(&lTree, pRepository, pOid)) {
			return 0;
		}
		uint lEntryCount = git_tree_entrycount(lTree);
		const git_tree_entry *lEntry = git_tree_entry_byindex(lTree, lEntryCount - 1);
		quint64 lEntryOffset;
		if(!offsetFromName(lEntry, lEntryOffset)) {
			git_tree_free(lTree);
			return 0;
		}
		lLastChunkOffset += lEntryOffset;
		pOid = git_tree_entry_id(lEntry);
		lMode = git_tree_entry_filemode(lEntry);
		git_tree_free(lTree);
	} while(S_ISDIR(lMode));

	git_blob *lBlob;
	if(0 != git_blob_lookup(&lBlob, pRepository, pOid)) {
		return 0;
	}
	lLastChunkSize = git_blob_rawsize(lBlob);
	git_blob_free(lBlob);
	return lLastChunkOffset + lLastChunkSize;
}

bool offsetFromName(const git_tree_entry *pEntry, quint64 &pUint) {
	bool lParsedOk;
	pUint = QString::fromUtf8(git_tree_entry_name(pEntry)).toULongLong(&lParsedOk, 16);
	return lParsedOk;
}


void getEntryAttributes(const git_tree_entry *pTreeEntry, uint &pMode, bool &pChunked, const git_oid *&pOid, QString &pName) {
	pMode = git_tree_entry_filemode(pTreeEntry);
	pOid = git_tree_entry_id(pTreeEntry);
	pName = QString::fromUtf8(git_tree_entry_name(pTreeEntry));
	pChunked = false;
	if(pName.endsWith(QStringLiteral(".bupl"))) {
		pName.chop(5);
	} else if(pName.endsWith(QStringLiteral(".bup"))) {
		pName.chop(4);
		pMode = DEFAULT_MODE_FILE;
		pChunked = true;
	}
}


QString vfsTimeToString(git_time_t pTime) {
	QDateTime lDateTime;
	lDateTime.setTime_t(pTime);
	return lDateTime.toLocalTime().toString(QStringLiteral("yyyy-MM-dd hh:mm"));
}

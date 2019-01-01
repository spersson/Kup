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

#include "kupengine.h"
#include "kupservice.h"
#include "kupdaemon.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>

KupEngine::KupEngine(QObject *pParent, const QVariantList &pArgs)
: Plasma::DataEngine(pParent, pArgs)
{
	mSocketName = QStringLiteral("kup-daemon-");
	mSocketName += QString::fromLocal8Bit(qgetenv("USER"));
	mSocket = new QLocalSocket(this);
	connect(mSocket, &QLocalSocket::readyRead, this, &KupEngine::processData);
	connect(mSocket, &QLocalSocket::stateChanged, this, &KupEngine::checkConnection);
	// wait 5 seconds before trying to connect first time
	QTimer::singleShot(5000, mSocket, [&]{mSocket->connectToServer(mSocketName);});
	setData(QStringLiteral("common"), QStringLiteral("plan count"), 0);
}

Plasma::Service *KupEngine::serviceForSource(const QString &pSource) {
	bool lIntOk;
	int lPlanNumber = pSource.toInt(&lIntOk);
	if(lIntOk) {
		return new KupService(lPlanNumber, mSocket, this);
	} else {
		return nullptr;
	}
}

void KupEngine::processData() {
	if(mSocket->bytesAvailable() <= 0) {
		return;
	}
	QJsonDocument lDoc = QJsonDocument::fromBinaryData(mSocket->readAll());
	if(!lDoc.isObject()) {
		return;
	}
	QJsonObject lEvent = lDoc.object();
	if(lEvent["event"] == QStringLiteral("status update")) {
		QJsonArray lPlans = lEvent["plans"].toArray();
		setData(QStringLiteral("common"), QStringLiteral("plan count"), lPlans.count());
		setCommonData(lEvent, QStringLiteral("tray icon active"));
		setCommonData(lEvent, QStringLiteral("tooltip icon name"));
		setCommonData(lEvent, QStringLiteral("tooltip title"));
		setCommonData(lEvent, QStringLiteral("tooltip subtitle"));
		setCommonData(lEvent, QStringLiteral("any plan busy"));
		setCommonData(lEvent, QStringLiteral("no plan reason"));
		for(int i = 0; i < lPlans.count(); ++i) {
			QJsonObject lPlan = lPlans[i].toObject();
			setPlanData(i, lPlan, QStringLiteral("description"));
			setPlanData(i, lPlan, QStringLiteral("destination available"));
			setPlanData(i, lPlan, QStringLiteral("status heading"));
			setPlanData(i, lPlan, QStringLiteral("status details"));
			setPlanData(i, lPlan, QStringLiteral("icon name"));
			setPlanData(i, lPlan, QStringLiteral("log file exists"));
			setPlanData(i, lPlan, QStringLiteral("busy"));
		}
	}
}

void KupEngine::checkConnection(QLocalSocket::LocalSocketState pState) {
	if(pState != QLocalSocket::ConnectedState && pState != QLocalSocket::ConnectingState) {
		QTimer::singleShot(10000, mSocket, [&]{mSocket->connectToServer(mSocketName);});
	}
	if(pState == QLocalSocket::UnconnectedState) {
		// Don't bother to translate this error message, guessing the error string from qt
		// is not translated also.
		QString lErrorText = QStringLiteral("Error, no connection to kup-daemon: ");
		lErrorText += mSocket->errorString();
		setData(QStringLiteral("common"), QStringLiteral("no plan reason"), lErrorText);
	}
}

void KupEngine::setPlanData(int i, const QJsonObject &pPlan, const QString &pKey) {
	setData(QString(QStringLiteral("plan %1")).arg(i), pKey, pPlan[pKey].toVariant());
}

void KupEngine::setCommonData(const QJsonObject &pCommonStatus, const QString &pKey) {
	setData(QStringLiteral("common"), pKey, pCommonStatus[pKey].toVariant());
}

K_EXPORT_PLASMA_DATAENGINE_WITH_JSON(backups, KupEngine, "plasma-dataengine-kup.json")

#include "kupengine.moc"


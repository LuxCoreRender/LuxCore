 /***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#include <QDateTime>
#include <QTextStream>

#include "luxmarkcfg.h"
#include "submitdialog.h"

SubmitDialog::SubmitDialog(const char *scnName,
		const double sampSecs,
		const vector<BenchmarkDeviceDescription> &ds,
		QWidget *parent) : QDialog(parent), ui(new Ui::SubmitDialog),
		descs(ds) {
	sceneName = scnName;
	sampleSecs = sampSecs;

	state = INPUT;

	manager = NULL;
	reply = NULL;

	ui->setupUi(this);
	ui->genericButton->setText("Submit");

	this->setWindowTitle("LuxMark v" LUXMARK_VERSION_MAJOR "." LUXMARK_VERSION_MINOR);
}

SubmitDialog::~SubmitDialog() {
	delete ui;
	delete reply;
	delete manager;
}

void SubmitDialog::ProgessMessage(const QString &msg) {
	QString buf;
	QTextStream ss(&buf);
	ss << QDateTime::currentDateTime().toString(tr("yyyy-MM-dd hh:mm:ss")) <<
			" - " << msg;

	ui->resultText->append(ss.readAll());
}

void SubmitDialog::genericButton() {
	if (state == INPUT) {
		// Send the result

		ui->genericButton->setText("Cancel");

		const QString name = ui->nameEdit->text();
		const QString pwd =  ui->pwdEdit->text();

		// Delete manager/reply if required
		if (manager) {
			delete manager;
			if (reply)
				delete reply;
			manager = NULL;
			reply = NULL;
		}

		manager = new QNetworkAccessManager(this);

		// Create the http request
		SD_LOG("Creating HTTP request...");
		QNetworkRequest request;
		request.setUrl(QUrl("http://www.luxrender.net/luxmark/submit"));
		request.setRawHeader("User-Agent", "LuxMark v" LUXMARK_VERSION_MAJOR "." LUXMARK_VERSION_MINOR);

		// Create the http request parameters
		QUrl params;
		params.addQueryItem("name", name);
		params.addQueryItem("pwd", pwd);

		QByteArray data;
		data = params.encodedQuery();

		// Send the request
		SD_LOG("Posting result...");
		reply = manager->post(request, data);
		connect(reply, SIGNAL(finished()), this, SLOT(httpFinished()));
		connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
				this, SLOT(httpError(QNetworkReply::NetworkError)));
	} else if (state == SUBMITTING) {
		// Cancel the result submission
	}
}

void SubmitDialog::httpFinished() {
	SD_LOG("httpFinished() - " << QString(reply->readAll()).toStdString());
}

void SubmitDialog::httpError(QNetworkReply::NetworkError error) {
	switch (error) {
		case QNetworkReply::ConnectionRefusedError:
			SD_LOG_ERROR("HTTP Error - the remote server refused the connection (the server is not accepting requests)");
			break;
		case QNetworkReply::HostNotFoundError:
			SD_LOG_ERROR("HTTP Error - the remote host name was not found (invalid hostname)");
			break;
		case QNetworkReply::TimeoutError:
			SD_LOG_ERROR("HTTP Error - the connection to the remote server timed out");
			break;
		default:
			SD_LOG_ERROR("HTTP Error " << error);
			break;
	}

	state = INPUT;
	ui->genericButton->setText("Submit");
}

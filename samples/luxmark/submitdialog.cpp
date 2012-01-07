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

#define SD_MSG(a) { std::stringstream _SD_LOG_LOCAL_SS; _SD_LOG_LOCAL_SS << a;  ProgessMessage(QString(_SD_LOG_LOCAL_SS.str().c_str())); }
#define SD_LOG(a) { SD_MSG("<FONT COLOR=\"#0000ff\">" << a << "</FONT>"); }
#define SD_LOG_ERROR(a) { SD_MSG("<FONT COLOR=\"#ff0000\">" << a << "</FONT>"); }

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
	ui->genericButton->setText("&Submit");

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

		state = SUBMITTING;
		ui->genericButton->setText("&Cancel");

		// Data to submit
		const QString name = ui->nameEdit->text();
		const QString pwd =  ui->pwdEdit->text();
#if defined (WIN32)
		const QString os = "Windows";
#elif defined (__linux__)
		const QString os = "Linux";
#elif defined (__APPLE__)
		const QString os = "MacOS";
#else
		const QString os = "Unknown";
#endif
		const QString scene = sceneName;
		const QString score = ToString(int(sampleSecs / 1000.0)).c_str();
		const QString note = ui->noteTextEdit->toPlainText();
		const QString devCount = ToString(descs.size()).c_str();

		// Delete manager/reply if required
		if (manager) {
			if (reply)
				delete reply;
			reply = NULL;

			delete manager;
			manager = NULL;
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
		params.addQueryItem("password", pwd);

		SD_LOG("Submitted data:");
		params.addQueryItem("os", os);
		SD_LOG("os = " << os.toStdString());

		params.addQueryItem("scene_name", scene);
		SD_LOG("scene_name = " << scene.toStdString());

		params.addQueryItem("score", score);
		SD_LOG("score = " << score.toStdString());

		params.addQueryItem("note", note);
		SD_LOG("note = " << note.toStdString());

		params.addQueryItem("dev_count", devCount);
		SD_LOG("dev_count = " << devCount.toStdString());

		for (size_t i = 0; i < descs.size(); ++i) {
			SD_LOG("dev_platform_name = " << descs[i].platformName);
			params.addQueryItem("dev_platform_name[]", QString(descs[i].platformName.c_str()));

			SD_LOG("dev_platform_ver = " << descs[i].platformVersion);
			params.addQueryItem("dev_platform_ver[]", QString(descs[i].platformVersion.c_str()));

			SD_LOG("dev_name = " << descs[i].deviceName);
			params.addQueryItem("dev_name[]", QString(descs[i].deviceName.c_str()));

			SD_LOG("dev_type = " << descs[i].deviceType);
			params.addQueryItem("dev_type[]", QString(descs[i].deviceType.c_str()));

			SD_LOG("dev_units = " << descs[i].units);
			params.addQueryItem("dev_units[]", QString(ToString(descs[i].units).c_str()));

			SD_LOG("dev_clock = " << descs[i].clock);
			params.addQueryItem("dev_clock[]", QString(ToString(descs[i].clock).c_str()));

			SD_LOG("dev_global_mem = " << descs[i].globalMem);
			params.addQueryItem("dev_global_mem[]", QString(ToString(descs[i].globalMem).c_str()));

			SD_LOG("dev_local_mem = " << descs[i].localMem);
			params.addQueryItem("dev_local_mem[]", QString(ToString(descs[i].localMem).c_str()));

			SD_LOG("dev_constant_mem = " << descs[i].localMem);
			params.addQueryItem("dev_constant_mem[]", QString(ToString(descs[i].localMem).c_str()));
		}

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
		reply->abort();

		SD_LOG_ERROR("Submission aborted");
		state = INPUT;
		ui->genericButton->setText("&Submit");
	} else {
		// Done
		this->close();
	}
}

void SubmitDialog::httpFinished() {
	//SD_LOG("httpFinished() - " << QString(reply->readAll()).toStdString());
	string result = QString(reply->readAll()).toStdString();

	if (result == "OK") {
		SD_LOG("Result successfully submitted !");
		state = DONE;
		ui->genericButton->setText("&Done");
	} else {
		SD_LOG_ERROR("Submission error: " << result);
		state = INPUT;
		ui->genericButton->setText("&Submit");
	}
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
	ui->genericButton->setText("&Submit");
}

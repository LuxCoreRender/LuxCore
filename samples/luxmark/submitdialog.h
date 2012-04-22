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

#ifndef _SUBMITDIALOG_H
#define	_SUBMITDIALOG_H

#include <cstddef>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include "smalllux.h"
#include "hardwaretree.h"
#include "ui_submitdialog.h"

class SubmitDialog : public QDialog {
	Q_OBJECT

public:
	SubmitDialog(
			const char *sceneName,
			const double sampleSecs,
			const vector<BenchmarkDeviceDescription> &descs,
			QWidget *parent = NULL);
	~SubmitDialog();

private:
	enum SubmitState { INPUT, SUBMITTING, DONE };

	void ProgessMessage(const QString &msg);

	Ui::SubmitDialog *ui;

	const char *sceneName;
	double sampleSecs;
	const vector<BenchmarkDeviceDescription> &descs;

	SubmitState state;

	QNetworkAccessManager *manager;
	QNetworkReply *reply;

private slots:
	void genericButton();
	void httpFinished();
	void httpError(QNetworkReply::NetworkError error);
};

#endif	/* _SUBMITDIALOG_H */


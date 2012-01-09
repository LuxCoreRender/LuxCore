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

#include "luxmarkcfg.h"
#include "resultdialog.h"
#include "submitdialog.h"
#include "luxmarkapp.h"

ResultDialog::ResultDialog(LuxMarkAppMode mode,
		const char *scnName, const double sampSecs,
		const vector<BenchmarkDeviceDescription> &ds,
		QWidget *parent) : QDialog(parent),
		ui(new Ui::ResultDialog), descs(ds) {
	sceneName = scnName;
	sampleSecs = sampSecs;

	ui->setupUi(this);

	this->setWindowTitle("LuxMark v" LUXMARK_VERSION_MAJOR "." LUXMARK_VERSION_MINOR);
	ui->modeLabel->setText((mode == BENCHMARK_OCL_GPU) ? "OpenCL GPUs" :
				((mode == BENCHMARK_OCL_CPUGPU) ? "OpenCL CPUs+GPUs" :
					((mode == BENCHMARK_OCL_CPU) ? "OpenCL CPUs" :
						((mode == BENCHMARK_OCL_CUSTOM) ? "OpenCL Custom" : "Interactive"))));
	ui->sceneLabel->setText(sceneName);

	deviceListModel = new DeviceListModel(descs);
	ui->deviceListView->setModel(deviceListModel);

	ui->resultLCD->display(int(sampleSecs / 1000.0));

	if ((strcmp(sceneName, SCENE_SALA) != 0) && (strcmp(sceneName, SCENE_LUXBALL_HDR) !=0))
		ui->submitButton->setEnabled(false);
}

ResultDialog::~ResultDialog() {
	delete ui;
	delete deviceListModel;
}

void ResultDialog::submitResult() {
	SubmitDialog *dialog = new SubmitDialog(sceneName, sampleSecs, descs);
	dialog->exec();
	delete dialog;
}

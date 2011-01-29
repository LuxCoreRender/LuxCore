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

#include <qt4/QtGui/qtextedit.h>

#include "mainwindow.h"
#include "aboutdialog.h"
#include "smalllux.h"
#include "renderconfig.h"

MainWindow *LogWindow = NULL;

void DebugHandler(const char *msg) {
	LM_LOG("<B>[LuxRays]</B> " << msg);
}

MainWindow::MainWindow(QWidget *parent, Qt::WindowFlags flags) : QMainWindow(parent, flags),
		ui(new Ui::MainWindow) {
	ui->setupUi(this);

	// Setup rendering area
	renderScene = new QGraphicsScene();
	renderScene->setBackgroundBrush(QColor(127,127,127));
	luxLogo = renderScene->addPixmap(QPixmap(":/images/resources/luxlogo_bg.png"));
	luxFrameBuffer = renderScene->addPixmap(QPixmap(":/images/resources/luxlogo_bg.png"));
	ui->RenderView->setScene(renderScene);

	frameBuffer = NULL;
	fbWidth = 0;
	fbHeight = 0;

	ShowLogo();
}

MainWindow::~MainWindow() {
	delete ui;
	delete luxFrameBuffer;
	delete luxLogo;
	delete renderScene;
	delete frameBuffer;

}

void MainWindow::PrintLog(const string &msg) {
	boost::unique_lock<boost::mutex> lock(logMutex);

	ui->LogView->append(QString(msg.c_str()));
}

void MainWindow::exitApp() {
	exit(0);
}

void MainWindow::showAbout() {
	AboutDialog *dialog = new AboutDialog();

	dialog->exec();
}

void MainWindow::ShowLogo() {
	if (luxFrameBuffer->isVisible()) {
		luxFrameBuffer->hide();
	}

	if (!luxLogo->isVisible()) {
		luxLogo->show();
		renderScene->setSceneRect(0.f, 0.f, 416.f, 389.f);
		ui->RenderView->centerOn(luxLogo);
	}

	ui->RenderView->setInteractive(false);
}

bool MainWindow::IsShowingLogo() const {
	return luxLogo->isVisible();
}

void MainWindow::ShowFrameBuffer(const float *frameBufferFloat,
		const unsigned  int width, const unsigned  int height) {
	if (luxLogo->isVisible())
		luxLogo->hide();

	// Check if I have to allocate a new frame buffer
	if (!frameBuffer || (width != fbWidth) || (height != fbHeight)) {
		delete frameBuffer;
		fbWidth = width;
		fbHeight = height;
		frameBuffer = new unsigned char[fbWidth * fbHeight * 3];
	}

	// Convert the frame buffer from float to unsigned char format and
	// flip along the height
	for (size_t y = 0; y < fbHeight; ++y) {
		size_t lineIndexSrc = y * fbWidth * 3;
		size_t lineIndexDst = (fbHeight - y - 1) * fbWidth * 3;
		for (size_t x = 0; x < fbWidth; ++x) {
			frameBuffer[lineIndexDst + x * 3] =
					(unsigned char)(frameBufferFloat[lineIndexSrc + x * 3] * 255.f + .5f);
			frameBuffer[lineIndexDst + x * 3 + 1] =
					(unsigned char)(frameBufferFloat[lineIndexSrc + x * 3 + 1] * 255.f + .5f);
			frameBuffer[lineIndexDst + x * 3 + 2] =
					(unsigned char)(frameBufferFloat[lineIndexSrc + x * 3 + 2] * 255.f + .5f);
		}
	}

	luxFrameBuffer->setPixmap(QPixmap::fromImage(QImage(
		frameBuffer, width, height, width * 3, QImage::Format_RGB888)));

	if (!luxFrameBuffer->isVisible()) {
		luxFrameBuffer->show();
		renderScene->setSceneRect(0.0f, 0.0f, width, height);
		ui->RenderView->centerOn(luxFrameBuffer);
	}

	ui->RenderView->setDragMode(QGraphicsView::ScrollHandDrag);
	ui->RenderView->setInteractive(true);

	LM_LOG("Screen updated");
}

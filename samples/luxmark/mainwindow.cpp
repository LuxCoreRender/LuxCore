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

#include <QTextEdit>
#include <QGraphicsSceneMouseEvent>
#include <QDialogButtonBox>

#include "mainwindow.h"
#include "aboutdialog.h"
#include "smalllux.h"
#include "renderconfig.h"
#include "luxmarkapp.h"

#include <QDateTime>
#include <QTextStream>
#include <qt4/QtGui/qgraphicsitem.h>

MainWindow *LogWindow = NULL;

int EVT_LUX_LOG_MESSAGE = QEvent::registerEventType();
int EVT_LUX_ERR_MESSAGE = QEvent::registerEventType();

LuxLogEvent::LuxLogEvent(QString msg) : QEvent((QEvent::Type)EVT_LUX_LOG_MESSAGE), message(msg) {
	setAccepted(false);
}

LuxErrorEvent::LuxErrorEvent(QString msg) : QEvent((QEvent::Type)EVT_LUX_ERR_MESSAGE), message(msg) {
	setAccepted(false);
}

void DebugHandler(const char *msg) {
	LM_LOG_LUXRAYS(msg);
}

void SDLDebugHandler(const char *msg) {
	LM_LOG_SDL(msg);
}

//------------------------------------------------------------------------------

LuxFrameBuffer::LuxFrameBuffer(const QPixmap &pixmap) : QGraphicsPixmapItem(pixmap) {
	luxApp = NULL;

	setAcceptHoverEvents(true);
	setFlag(QGraphicsItem::ItemIsSelectable, true);
}

void LuxFrameBuffer::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
	if (luxApp)
		luxApp->HandleMouseMoveEvent(event);
}

void LuxFrameBuffer::mousePressEvent(QGraphicsSceneMouseEvent *event) {
	if (luxApp)
		luxApp->HandleMousePressEvent(event);
}

//------------------------------------------------------------------------------

MainWindow::MainWindow(QWidget *parent, Qt::WindowFlags flags) : QMainWindow(parent, flags),
		ui(new Ui::MainWindow) {
	ui->setupUi(this);

	// Setup rendering area
	renderScene = new QGraphicsScene();
	renderScene->setBackgroundBrush(QColor(127,127,127));
	luxLogo = renderScene->addPixmap(QPixmap(":/images/resources/luxlogo_bg.png"));
	luxFrameBuffer = new LuxFrameBuffer(QPixmap(":/images/resources/luxlogo_bg.png"));
	renderScene->addItem(luxFrameBuffer);

	authorLabel = new QGraphicsSimpleTextItem(QString("Scene designed by Daniel Salazar (http://www.3developer.com)"));
	renderScene->addItem(authorLabel);

	screenLabelBack = renderScene->addRect(0.f, 0.f, 1.f, 1.f, QPen(), Qt::lightGray);
	screenLabel = new QGraphicsSimpleTextItem(QString("[Time: 0secs (Wait)]"));
	renderScene->addItem(screenLabel);

	ui->RenderView->setScene(renderScene);

	frameBuffer = NULL;
	fbWidth = 0;
	fbHeight = 0;

	// Setup status bar
	statusbarLabel = new QLabel(ui->statusbar);
	statusbarLabel->setText("");
	ui->statusbar->addWidget(statusbarLabel);

	ShowLogo();
}

MainWindow::~MainWindow() {
	delete ui;
	delete statusbarLabel;
	delete authorLabel;
	delete screenLabel;
	delete screenLabelBack;
	delete luxFrameBuffer;
	delete luxLogo;
	delete renderScene;
	delete frameBuffer;

}

//------------------------------------------------------------------------------
// Qt Slots
//------------------------------------------------------------------------------

void MainWindow::exitApp() {
	// Win32 refuses to exit without the following line
	((LuxMarkApp *)qApp)->Stop();

	exit(0);
}

void MainWindow::showAbout() {
	AboutDialog *dialog = new AboutDialog();

	dialog->exec();
}

void MainWindow::setLuxBallScene() {
	LM_LOG("Set LuxBall scene");
	authorLabel->setText(QString("Scene designed by LuxRender project"));
	((LuxMarkApp *)qApp)->SetScene(SCENE_LUXBALL);
}

void MainWindow::setLuxBallHDRScene() {
	LM_LOG("Set LuxBall HDR scene");
	authorLabel->setText(QString("Scene designed by LuxRender project"));
	((LuxMarkApp *)qApp)->SetScene(SCENE_LUXBALL_HDR);
}

void MainWindow::setLuxBallSkyScene() {
	LM_LOG("Set LuxBall Sky scene");
	authorLabel->setText(QString("Scene designed by LuxRender project"));
	((LuxMarkApp *)qApp)->SetScene(SCENE_LUXBALL_SKY);
}

void MainWindow::setSalaScene() {
	LM_LOG("Set Sala scene");
	authorLabel->setText(QString("Scene designed by Daniel Salazar (http://www.3developer.com)"));
	((LuxMarkApp *)qApp)->SetScene(SCENE_SALA);
}

void MainWindow::setBenchmarkGPUsMode() {
	LM_LOG("Set Benchmark GPUs mode");
	((LuxMarkApp *)qApp)->SetMode(BENCHMARK_OCL_GPU);
}

void MainWindow::setBenchmarkCPUsGPUsMode() {
	LM_LOG("Set Benchmark CPUs+GPUs mode");
	((LuxMarkApp *)qApp)->SetMode(BENCHMARK_OCL_CPUGPU);
}

void MainWindow::setBenchmarkCPUsMode() {
	LM_LOG("Set Benchmark CPUs mode");
	((LuxMarkApp *)qApp)->SetMode(BENCHMARK_OCL_CPU);
}

void MainWindow::setInteractiveMode() {
	LM_LOG("Set Interactive mode");
	((LuxMarkApp *)qApp)->SetMode(INTERACTIVE);
}

void MainWindow::setPauseMode() {
	LM_LOG("Set Pause mode");
	((LuxMarkApp *)qApp)->SetMode(PAUSE);
}

//------------------------------------------------------------------------------

void MainWindow::ShowLogo() {
	if (luxFrameBuffer->isVisible()) {
		luxFrameBuffer->hide();
		authorLabel->hide();
		screenLabelBack->hide();
		screenLabel->hide();
	}

	if (!luxLogo->isVisible()) {
		luxLogo->show();
		renderScene->setSceneRect(0.f, 0.f, 416.f, 389.f);
		ui->RenderView->centerOn(luxLogo);
	}

	statusbarLabel->setText("");
	ui->RenderView->setInteractive(false);
}

void MainWindow::SetModeCheck(const int index) {
	if (index == 0) {
		ui->action_Benchmark_OpenCL_GPUs->setChecked(true);
		ui->action_Benchmark_OpenCL_CPUs_GPUs->setChecked(false);
		ui->action_Benchmark_OpenCL_CPUs->setChecked(false);
		ui->action_Interactive->setChecked(false);
		ui->action_Pause->setChecked(false);
	} else if (index == 1) {
		ui->action_Benchmark_OpenCL_GPUs->setChecked(false);
		ui->action_Benchmark_OpenCL_CPUs_GPUs->setChecked(true);
		ui->action_Benchmark_OpenCL_CPUs->setChecked(false);
		ui->action_Interactive->setChecked(false);
		ui->action_Pause->setChecked(false);
	} else if (index == 2) {
		ui->action_Benchmark_OpenCL_GPUs->setChecked(false);
		ui->action_Benchmark_OpenCL_CPUs_GPUs->setChecked(false);
		ui->action_Benchmark_OpenCL_CPUs->setChecked(true);
		ui->action_Interactive->setChecked(false);
		ui->action_Pause->setChecked(false);
	} else if (index == 3) {
		ui->action_Benchmark_OpenCL_GPUs->setChecked(false);
		ui->action_Benchmark_OpenCL_CPUs_GPUs->setChecked(false);
		ui->action_Benchmark_OpenCL_CPUs->setChecked(false);
		ui->action_Interactive->setChecked(true);
		ui->action_Pause->setChecked(false);
	} else if (index == 4) {
		ui->action_Benchmark_OpenCL_GPUs->setChecked(false);
		ui->action_Benchmark_OpenCL_CPUs_GPUs->setChecked(false);
		ui->action_Benchmark_OpenCL_CPUs->setChecked(false);
		ui->action_Interactive->setChecked(false);
		ui->action_Pause->setChecked(true);
	} else
		assert(false);
}

void MainWindow::SetSceneCheck(const int index) {
	if (index == 0) {
		ui->action_Sala->setChecked(true);
		ui->action_LuxBall_HDR->setChecked(false);
		ui->action_LuxBall->setChecked(false);
		ui->action_LuxBall_Sky->setChecked(false);
	} else if (index == 1) {
		ui->action_Sala->setChecked(false);
		ui->action_LuxBall_HDR->setChecked(true);
		ui->action_LuxBall->setChecked(false);
		ui->action_LuxBall_Sky->setChecked(false);
	} else if (index == 2) {
		ui->action_Sala->setChecked(false);
		ui->action_LuxBall_HDR->setChecked(false);
		ui->action_LuxBall->setChecked(true);
		ui->action_LuxBall_Sky->setChecked(false);
	} else if (index == 3) {
		ui->action_Sala->setChecked(false);
		ui->action_LuxBall_HDR->setChecked(false);
		ui->action_LuxBall->setChecked(false);
		ui->action_LuxBall_Sky->setChecked(true);
	} else
		assert(false);

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
		frameBuffer, fbWidth, fbHeight, width * 3, QImage::Format_RGB888)));

	if (!luxFrameBuffer->isVisible()) {
		luxFrameBuffer->show();
		authorLabel->show();

		qreal w = Max<qreal>(fbWidth, screenLabel->boundingRect().width());
		qreal h = fbHeight + screenLabel->boundingRect().height();
		renderScene->setSceneRect(0.f, 0.f, w, h);
		luxFrameBuffer->setPos(Max<qreal>(0.f, (w - fbWidth) / 2), 0.f);
		ui->RenderView->centerOn(luxFrameBuffer);

		screenLabelBack->setRect(0.f, fbHeight, w, screenLabel->boundingRect().height());
		screenLabelBack->show();
		screenLabel->show();
	}

	ui->RenderView->setDragMode(QGraphicsView::ScrollHandDrag);
	ui->RenderView->setInteractive(true);

	//LM_LOG("Screen updated");
}

void MainWindow::SetHadwareTreeModel(HardwareTreeModel *treeModel) {
	if (!ui->HardwareView->model())
		ui->HardwareView->setModel(treeModel);
}

bool MainWindow::event(QEvent *event) {
	bool retval = FALSE;
	int eventtype = event->type();

	// Check if it's one of "our" events
	if (eventtype == EVT_LUX_LOG_MESSAGE) {
		QTextStream ss(new QString());
		ss << QDateTime::currentDateTime().toString(tr("yyyy-MM-dd hh:mm:ss")) << " - " <<
				((LuxLogEvent *)event)->getMessage();

		ui->LogView->append(ss.readAll());
	} else if (eventtype == EVT_LUX_ERR_MESSAGE) {
		QTextStream ss(new QString());
		ss << "<FONT COLOR=\"#ff0000\">" << QDateTime::currentDateTime().toString(tr("yyyy-MM-dd hh:mm:ss")) << " - " <<
				((LuxLogEvent *)event)->getMessage() << "</FONT>";

		ui->LogView->append(ss.readAll());

		((LuxMarkApp *)qApp)->SetMode(PAUSE);
	}

	if (retval) {
		// Was our event, stop the event propagation
		event->accept();
	}

	return QMainWindow::event(event);
}

void MainWindow::UpdateScreenLabel(const char *msg, const bool valid) {
	screenLabel->setText(QString(msg));
	//screenLabel->setBrush(valid ? Qt::green : Qt::red);
	screenLabel->setBrush(Qt::blue);
	screenLabel->setPos(0.f, fbHeight);

	// Update scene size
	qreal w = Max<qreal>(fbWidth, screenLabel->boundingRect().width());
	qreal h = fbHeight + screenLabel->boundingRect().height();
	renderScene->setSceneRect(0.f, 0.f, w, h);
	luxFrameBuffer->setPos(Max<qreal>(0.f, (w - fbWidth) / 2), 0.f);
	screenLabelBack->setRect(0.f, fbHeight, w, screenLabel->boundingRect().height());

	// Update author label
	authorLabel->setBrush(Qt::blue);
	authorLabel->setPos(Max<qreal>(0.f, (w - fbWidth) / 2), 0.f);

	// Update status bar with the first line of the message
	QString qMsg(msg);
	statusbarLabel->setText(qMsg.split(QString("\n"))[0]);
}

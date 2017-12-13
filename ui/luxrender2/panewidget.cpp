/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
 *                                                                         *
 *   Lux Renderer is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Lux Renderer is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   This project is based on PBRT ; see http://www.pbrt.org               *
 *   Lux Renderer website : http://www.luxrender.net                       *
 ***************************************************************************/

#include <iostream>

#include <QFont>

#include "panewidget.hxx"
#include "ui_pane.h"

using namespace std;

ClickableLabel::ClickableLabel(const QString& label, QWidget *parent) : QLabel(label,parent) { }

void ClickableLabel::mouseReleaseEvent(QMouseEvent* event) 
{
	emit clicked();
}

PaneWidget::PaneWidget(QWidget *parent, const QString& label, const QString& icon, bool onoffbutton, bool solobutton) : QWidget(parent), ui(new Ui::PaneWidget)
{
	expanded = false;
	onofflabel = NULL;
	sololabel = NULL;

	m_Index = -1;

	ui->setupUi(this);
	
	ui->frame->setStyleSheet(QString::fromUtf8(" QFrame {\n""background-color: qlineargradient(spread:pad, x1:1, y1:0, x2:0, y2:0, stop:0 rgb(120, 120, 120), stop:0.8 rgb(230, 230, 230))\n""}\n"""));

	if (!icon.isEmpty())
		ui->labelPaneIcon->setPixmap(QPixmap(icon));
		ui->labelPaneIcon->setStyleSheet(QString::fromUtf8(" QFrame {\n""background-color: rgba(232, 232, 232, 0)\n""}"));
	
	if (!label.isEmpty())
		ui->labelPaneName->setText(label);
		ui->labelPaneName->setStyleSheet(QString::fromUtf8(" QFrame {\n""background-color: rgba(232, 232, 232, 0)\n""}"));

#if defined(__APPLE__)
	ui->frame->setLineWidth(2);
	ui->labelPaneName->setFont(QFont  ("Lucida Grande", 11, QFont::Bold));
#endif

	expandlabel = new ClickableLabel(">", this);
	expandlabel->setPixmap(QPixmap(":/icons/collapsedicon.png"));
	expandlabel->setStyleSheet(QString::fromUtf8(" QFrame {\n""background-color: rgba(232, 232, 232, 0)\n""}"));
	ui->gridLayout->addWidget(expandlabel, 0, 3, 1, 1);
 
	connect(expandlabel, SIGNAL(clicked()), this, SLOT(expandClicked()));

	powerON = false;
	m_SoloState = SOLO_OFF;
	
	if (onoffbutton)
		showOnOffButton();

	if (solobutton)
		showSoloButton();
}

PaneWidget::~PaneWidget()
{
	delete expandlabel;

	if (onofflabel != NULL)
		delete onofflabel;

	if (sololabel != NULL)
		delete sololabel;
}

void PaneWidget::setTitle(const QString& title)
{
	ui->labelPaneName->setText(title);
}

void PaneWidget::setIcon(const QString& icon)
{
	ui->labelPaneIcon->setPixmap(QPixmap(icon));
}

void PaneWidget::showOnOffButton(bool showbutton)
{
	if (onofflabel == NULL) {
		onofflabel = new ClickableLabel("*", this);
		onofflabel->setPixmap(QPixmap(":/icons/poweronicon.png"));
		onofflabel->setStyleSheet(QString::fromUtf8(" QFrame {\n""background-color: rgba(232, 232, 232, 0)\n""}"));

		ui->gridLayout->removeWidget(expandlabel);
		ui->gridLayout->addWidget(onofflabel, 0, 3, 1, 1);
		ui->gridLayout->addWidget(expandlabel, 0, 4, 1, 1);

		connect(onofflabel, SIGNAL(clicked()), this, SLOT(onoffClicked()));
		powerON = true;
	}

	if (showbutton)
		onofflabel->show();
	else
		onofflabel->hide();
}

void PaneWidget::onoffClicked()
{
	if (mainwidget->isEnabled()) {
		mainwidget->setEnabled(false);
		onofflabel->setPixmap(QPixmap(":/icons/powerofficon.png"));
		emit turnedOff();
		powerON = false;
	}
	else {
		mainwidget->setEnabled(true);
		onofflabel->setPixmap(QPixmap(":/icons/poweronicon.png"));
		emit turnedOn();
		powerON = true;
	}
}

void PaneWidget::expandClicked()
{
	if (expanded)
		collapse();
	else
		expand();

}

void PaneWidget::showSoloButton(bool showbutton)
{
	if (sololabel == NULL) {
		sololabel = new ClickableLabel("S", this);
		sololabel->setPixmap(QPixmap(":/icons/plusicon.png"));
		sololabel->setStyleSheet(QString::fromUtf8(" QFrame {\n""background-color: rgba(232, 232, 232, 0)\n""}"));
		sololabel->setToolTip( "Click to make this lightgroup solo, click again to remove solo mode." );

		ui->gridLayout->removeWidget(expandlabel);
		ui->gridLayout->addWidget(sololabel, 0, 3, 1, 1);
		ui->gridLayout->addWidget(onofflabel, 0, 4, 1, 1);
		ui->gridLayout->addWidget(expandlabel, 0, 5, 1, 1);

		connect(sololabel, SIGNAL(clicked()), this, SLOT(soloClicked()));
	}

	if (showbutton)
		sololabel->show();
	else
		sololabel->hide();
}

void PaneWidget::soloClicked()
{
	if ( m_SoloState == SOLO_ENABLED )
	{
		emit signalLightGroupSolo( -1 );
	}
	else 
	{
		emit signalLightGroupSolo( m_Index );
	}

	emit valuesChanged();
}

void PaneWidget::SetSolo( SoloState esolo )
{
	m_SoloState = esolo;
	
	if ( m_SoloState == SOLO_ENABLED || m_SoloState == SOLO_OFF )
	{
		sololabel->setPixmap(QPixmap(":/icons/plusicon.png"));
	}
	else
	{
		sololabel->setPixmap(QPixmap(":/icons/minusicon.png"));
	}
}

void PaneWidget::expand()
{
	expanded = true;
	expandlabel->setPixmap(QPixmap(":/icons/expandedicon.png"));
	mainwidget->show();
}

void PaneWidget::collapse()
{
	expanded = false;
	expandlabel->setPixmap(QPixmap(":/icons/collapsedicon.png"));
	mainwidget->hide();
}

void PaneWidget::setWidget(QWidget *widget)
{
	mainwidget = widget;
	ui->paneLayout->addWidget(widget);
#if defined(__APPLE__)
	expandlabel->setStyleSheet(QString::fromUtf8(" QFrame {\n""background-color: rgba(232, 232, 232, 0)\n""}"));
#endif
	if (!mainwidget->isEnabledTo(mainwidget->parentWidget()))
		onofflabel->setPixmap(QPixmap(":/icons/powerofficon.png"));
	if (expanded)
		mainwidget->show();
	else
		mainwidget->hide();
}

QWidget *PaneWidget::getWidget()
{
	return mainwidget;
}

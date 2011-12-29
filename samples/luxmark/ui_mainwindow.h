/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created: Thu Dec 29 12:48:08 2011
**      by: Qt User Interface Compiler version 4.7.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QGraphicsView>
#include <QtGui/QHeaderView>
#include <QtGui/QLabel>
#include <QtGui/QMainWindow>
#include <QtGui/QMenu>
#include <QtGui/QMenuBar>
#include <QtGui/QSplitter>
#include <QtGui/QStatusBar>
#include <QtGui/QTextEdit>
#include <QtGui/QTreeView>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QAction *action_Quit;
    QAction *action_About;
    QAction *action_Benchmark_OpenCL_GPUs;
    QAction *action_Interactive;
    QAction *action_LuxBall_HDR;
    QAction *action_LuxBall;
    QAction *action_Benchmark_OpenCL_CPUs_GPUs;
    QAction *action_Pause;
    QAction *action_Benchmark_OpenCL_CPUs;
    QAction *action_LuxBall_Sky;
    QAction *action_Sala;
    QWidget *centralwidget;
    QVBoxLayout *verticalLayout_2;
    QSplitter *splitter_2;
    QSplitter *splitter;
    QGraphicsView *RenderView;
    QWidget *layoutWidget;
    QVBoxLayout *verticalLayout;
    QLabel *hardwareDevicesLabel;
    QTreeView *HardwareView;
    QTextEdit *LogView;
    QMenuBar *menubar;
    QMenu *menu_File;
    QMenu *menu_Help;
    QMenu *menu_Mode;
    QMenu *menu_Scene;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName(QString::fromUtf8("MainWindow"));
        MainWindow->resize(1024, 768);
        MainWindow->setMinimumSize(QSize(128, 128));
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/images/resources/SLG_luxball_sppm_small.png"), QSize(), QIcon::Normal, QIcon::Off);
        MainWindow->setWindowIcon(icon);
        action_Quit = new QAction(MainWindow);
        action_Quit->setObjectName(QString::fromUtf8("action_Quit"));
        action_About = new QAction(MainWindow);
        action_About->setObjectName(QString::fromUtf8("action_About"));
        action_Benchmark_OpenCL_GPUs = new QAction(MainWindow);
        action_Benchmark_OpenCL_GPUs->setObjectName(QString::fromUtf8("action_Benchmark_OpenCL_GPUs"));
        action_Benchmark_OpenCL_GPUs->setCheckable(true);
        action_Interactive = new QAction(MainWindow);
        action_Interactive->setObjectName(QString::fromUtf8("action_Interactive"));
        action_Interactive->setCheckable(true);
        action_LuxBall_HDR = new QAction(MainWindow);
        action_LuxBall_HDR->setObjectName(QString::fromUtf8("action_LuxBall_HDR"));
        action_LuxBall_HDR->setCheckable(true);
        action_LuxBall = new QAction(MainWindow);
        action_LuxBall->setObjectName(QString::fromUtf8("action_LuxBall"));
        action_LuxBall->setCheckable(true);
        action_Benchmark_OpenCL_CPUs_GPUs = new QAction(MainWindow);
        action_Benchmark_OpenCL_CPUs_GPUs->setObjectName(QString::fromUtf8("action_Benchmark_OpenCL_CPUs_GPUs"));
        action_Benchmark_OpenCL_CPUs_GPUs->setCheckable(true);
        action_Pause = new QAction(MainWindow);
        action_Pause->setObjectName(QString::fromUtf8("action_Pause"));
        action_Pause->setCheckable(true);
        action_Benchmark_OpenCL_CPUs = new QAction(MainWindow);
        action_Benchmark_OpenCL_CPUs->setObjectName(QString::fromUtf8("action_Benchmark_OpenCL_CPUs"));
        action_Benchmark_OpenCL_CPUs->setCheckable(true);
        action_LuxBall_Sky = new QAction(MainWindow);
        action_LuxBall_Sky->setObjectName(QString::fromUtf8("action_LuxBall_Sky"));
        action_LuxBall_Sky->setCheckable(true);
        action_Sala = new QAction(MainWindow);
        action_Sala->setObjectName(QString::fromUtf8("action_Sala"));
        action_Sala->setCheckable(true);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
        centralwidget->setMinimumSize(QSize(160, 120));
        verticalLayout_2 = new QVBoxLayout(centralwidget);
        verticalLayout_2->setSpacing(0);
        verticalLayout_2->setContentsMargins(0, 0, 0, 0);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        splitter_2 = new QSplitter(centralwidget);
        splitter_2->setObjectName(QString::fromUtf8("splitter_2"));
        splitter_2->setOrientation(Qt::Vertical);
        splitter = new QSplitter(splitter_2);
        splitter->setObjectName(QString::fromUtf8("splitter"));
        splitter->setOrientation(Qt::Horizontal);
        RenderView = new QGraphicsView(splitter);
        RenderView->setObjectName(QString::fromUtf8("RenderView"));
        splitter->addWidget(RenderView);
        layoutWidget = new QWidget(splitter);
        layoutWidget->setObjectName(QString::fromUtf8("layoutWidget"));
        verticalLayout = new QVBoxLayout(layoutWidget);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalLayout->setContentsMargins(0, 0, 0, 0);
        hardwareDevicesLabel = new QLabel(layoutWidget);
        hardwareDevicesLabel->setObjectName(QString::fromUtf8("hardwareDevicesLabel"));
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(hardwareDevicesLabel->sizePolicy().hasHeightForWidth());
        hardwareDevicesLabel->setSizePolicy(sizePolicy);
        hardwareDevicesLabel->setFrameShape(QFrame::NoFrame);
        hardwareDevicesLabel->setFrameShadow(QFrame::Sunken);
        hardwareDevicesLabel->setAlignment(Qt::AlignCenter);
        hardwareDevicesLabel->setTextInteractionFlags(Qt::NoTextInteraction);

        verticalLayout->addWidget(hardwareDevicesLabel);

        HardwareView = new QTreeView(layoutWidget);
        HardwareView->setObjectName(QString::fromUtf8("HardwareView"));
        QSizePolicy sizePolicy1(QSizePolicy::Preferred, QSizePolicy::Expanding);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(HardwareView->sizePolicy().hasHeightForWidth());
        HardwareView->setSizePolicy(sizePolicy1);
        HardwareView->setFrameShape(QFrame::Box);
        HardwareView->setAnimated(true);
        HardwareView->header()->setVisible(false);

        verticalLayout->addWidget(HardwareView);

        splitter->addWidget(layoutWidget);
        splitter_2->addWidget(splitter);
        LogView = new QTextEdit(splitter_2);
        LogView->setObjectName(QString::fromUtf8("LogView"));
        LogView->setReadOnly(true);
        splitter_2->addWidget(LogView);

        verticalLayout_2->addWidget(splitter_2);

        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName(QString::fromUtf8("menubar"));
        menubar->setGeometry(QRect(0, 0, 1024, 25));
        menu_File = new QMenu(menubar);
        menu_File->setObjectName(QString::fromUtf8("menu_File"));
        menu_Help = new QMenu(menubar);
        menu_Help->setObjectName(QString::fromUtf8("menu_Help"));
        menu_Mode = new QMenu(menubar);
        menu_Mode->setObjectName(QString::fromUtf8("menu_Mode"));
        menu_Scene = new QMenu(menubar);
        menu_Scene->setObjectName(QString::fromUtf8("menu_Scene"));
        MainWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName(QString::fromUtf8("statusbar"));
        MainWindow->setStatusBar(statusbar);

        menubar->addAction(menu_File->menuAction());
        menubar->addAction(menu_Mode->menuAction());
        menubar->addAction(menu_Scene->menuAction());
        menubar->addAction(menu_Help->menuAction());
        menu_File->addAction(action_Quit);
        menu_Help->addAction(action_About);
        menu_Mode->addAction(action_Benchmark_OpenCL_GPUs);
        menu_Mode->addAction(action_Benchmark_OpenCL_CPUs_GPUs);
        menu_Mode->addAction(action_Benchmark_OpenCL_CPUs);
        menu_Mode->addAction(action_Interactive);
        menu_Mode->addAction(action_Pause);
        menu_Scene->addAction(action_Sala);
        menu_Scene->addAction(action_LuxBall_HDR);
        menu_Scene->addAction(action_LuxBall);
        menu_Scene->addAction(action_LuxBall_Sky);

        retranslateUi(MainWindow);
        QObject::connect(action_Quit, SIGNAL(triggered()), MainWindow, SLOT(exitApp()));
        QObject::connect(action_About, SIGNAL(triggered()), MainWindow, SLOT(showAbout()));
        QObject::connect(action_LuxBall, SIGNAL(triggered()), MainWindow, SLOT(setLuxBallScene()));
        QObject::connect(action_LuxBall_HDR, SIGNAL(triggered()), MainWindow, SLOT(setLuxBallHDRScene()));
        QObject::connect(action_Benchmark_OpenCL_CPUs_GPUs, SIGNAL(triggered()), MainWindow, SLOT(setBenchmarkCPUsGPUsMode()));
        QObject::connect(action_Interactive, SIGNAL(triggered()), MainWindow, SLOT(setInteractiveMode()));
        QObject::connect(action_Benchmark_OpenCL_GPUs, SIGNAL(triggered()), MainWindow, SLOT(setBenchmarkGPUsMode()));
        QObject::connect(action_Pause, SIGNAL(triggered()), MainWindow, SLOT(setPauseMode()));
        QObject::connect(action_Benchmark_OpenCL_CPUs, SIGNAL(triggered()), MainWindow, SLOT(setBenchmarkCPUsMode()));
        QObject::connect(action_LuxBall_Sky, SIGNAL(triggered()), MainWindow, SLOT(setLuxBallSkyScene()));
        QObject::connect(action_Sala, SIGNAL(triggered()), MainWindow, SLOT(setSalaScene()));

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QApplication::translate("MainWindow", "LuxMark", 0, QApplication::UnicodeUTF8));
        action_Quit->setText(QApplication::translate("MainWindow", "&Quit", 0, QApplication::UnicodeUTF8));
        action_Quit->setShortcut(QApplication::translate("MainWindow", "Ctrl+Q", 0, QApplication::UnicodeUTF8));
        action_About->setText(QApplication::translate("MainWindow", "About", 0, QApplication::UnicodeUTF8));
        action_Benchmark_OpenCL_GPUs->setText(QApplication::translate("MainWindow", "Benchmark (OpenCL &GPUs-only)", 0, QApplication::UnicodeUTF8));
        action_Interactive->setText(QApplication::translate("MainWindow", "&Interactive (OpenCL GPUs-only)", 0, QApplication::UnicodeUTF8));
        action_LuxBall_HDR->setText(QApplication::translate("MainWindow", "LuxBall &HDR (262K triangles)", 0, QApplication::UnicodeUTF8));
        action_LuxBall_HDR->setShortcut(QApplication::translate("MainWindow", "Ctrl+C", 0, QApplication::UnicodeUTF8));
        action_LuxBall->setText(QApplication::translate("MainWindow", "&LuxBall  (262K triangles)", 0, QApplication::UnicodeUTF8));
        action_LuxBall->setShortcut(QApplication::translate("MainWindow", "Ctrl+L", 0, QApplication::UnicodeUTF8));
        action_Benchmark_OpenCL_CPUs_GPUs->setText(QApplication::translate("MainWindow", "Benchmark (OpenCL CPUs &+ GPUs)", 0, QApplication::UnicodeUTF8));
        action_Pause->setText(QApplication::translate("MainWindow", "&Pause", 0, QApplication::UnicodeUTF8));
        action_Benchmark_OpenCL_CPUs->setText(QApplication::translate("MainWindow", "Benchmark (OpenCL &CPUs-only)", 0, QApplication::UnicodeUTF8));
        action_LuxBall_Sky->setText(QApplication::translate("MainWindow", "LuxBall S&ky  (262K triangles)", 0, QApplication::UnicodeUTF8));
        action_LuxBall_Sky->setShortcut(QApplication::translate("MainWindow", "Ctrl+K", 0, QApplication::UnicodeUTF8));
        action_Sala->setText(QApplication::translate("MainWindow", "&Sala (488K triangles)", 0, QApplication::UnicodeUTF8));
        action_Sala->setShortcut(QApplication::translate("MainWindow", "Ctrl+A", 0, QApplication::UnicodeUTF8));
        hardwareDevicesLabel->setText(QApplication::translate("MainWindow", "Hardware Devices", 0, QApplication::UnicodeUTF8));
        menu_File->setTitle(QApplication::translate("MainWindow", "&File", 0, QApplication::UnicodeUTF8));
        menu_Help->setTitle(QApplication::translate("MainWindow", "&Help", 0, QApplication::UnicodeUTF8));
        menu_Mode->setTitle(QApplication::translate("MainWindow", "&Mode", 0, QApplication::UnicodeUTF8));
        menu_Scene->setTitle(QApplication::translate("MainWindow", "&Scene", 0, QApplication::UnicodeUTF8));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H

/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created: Mon Jan 31 15:42:52 2011
**      by: Qt User Interface Compiler version 4.6.2
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
#include <QtGui/QMainWindow>
#include <QtGui/QMenu>
#include <QtGui/QMenuBar>
#include <QtGui/QScrollArea>
#include <QtGui/QSplitter>
#include <QtGui/QStatusBar>
#include <QtGui/QTextEdit>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QAction *action_Quit;
    QAction *action_About;
    QAction *action_Benchmark;
    QAction *action_Interactive;
    QAction *action_LuxBall_HDR;
    QAction *action_LuxBall;
    QWidget *centralwidget;
    QVBoxLayout *verticalLayout;
    QSplitter *splitter_2;
    QSplitter *splitter;
    QGraphicsView *RenderView;
    QScrollArea *SettingsArea;
    QWidget *scrollAreaWidgetContents;
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
        action_Benchmark = new QAction(MainWindow);
        action_Benchmark->setObjectName(QString::fromUtf8("action_Benchmark"));
        action_Benchmark->setCheckable(true);
        action_Interactive = new QAction(MainWindow);
        action_Interactive->setObjectName(QString::fromUtf8("action_Interactive"));
        action_Interactive->setCheckable(true);
        action_LuxBall_HDR = new QAction(MainWindow);
        action_LuxBall_HDR->setObjectName(QString::fromUtf8("action_LuxBall_HDR"));
        action_LuxBall_HDR->setCheckable(true);
        action_LuxBall = new QAction(MainWindow);
        action_LuxBall->setObjectName(QString::fromUtf8("action_LuxBall"));
        action_LuxBall->setCheckable(true);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
        centralwidget->setMinimumSize(QSize(160, 120));
        verticalLayout = new QVBoxLayout(centralwidget);
        verticalLayout->setSpacing(0);
        verticalLayout->setContentsMargins(0, 0, 0, 0);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        splitter_2 = new QSplitter(centralwidget);
        splitter_2->setObjectName(QString::fromUtf8("splitter_2"));
        splitter_2->setOrientation(Qt::Vertical);
        splitter = new QSplitter(splitter_2);
        splitter->setObjectName(QString::fromUtf8("splitter"));
        splitter->setOrientation(Qt::Horizontal);
        RenderView = new QGraphicsView(splitter);
        RenderView->setObjectName(QString::fromUtf8("RenderView"));
        splitter->addWidget(RenderView);
        SettingsArea = new QScrollArea(splitter);
        SettingsArea->setObjectName(QString::fromUtf8("SettingsArea"));
        SettingsArea->setWidgetResizable(true);
        scrollAreaWidgetContents = new QWidget();
        scrollAreaWidgetContents->setObjectName(QString::fromUtf8("scrollAreaWidgetContents"));
        scrollAreaWidgetContents->setGeometry(QRect(0, 0, 82, 355));
        SettingsArea->setWidget(scrollAreaWidgetContents);
        splitter->addWidget(SettingsArea);
        splitter_2->addWidget(splitter);
        LogView = new QTextEdit(splitter_2);
        LogView->setObjectName(QString::fromUtf8("LogView"));
        LogView->setReadOnly(true);
        splitter_2->addWidget(LogView);

        verticalLayout->addWidget(splitter_2);

        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName(QString::fromUtf8("menubar"));
        menubar->setGeometry(QRect(0, 0, 1024, 26));
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
        menu_Mode->addAction(action_Benchmark);
        menu_Mode->addAction(action_Interactive);
        menu_Scene->addAction(action_LuxBall_HDR);
        menu_Scene->addAction(action_LuxBall);

        retranslateUi(MainWindow);
        QObject::connect(action_Quit, SIGNAL(triggered()), MainWindow, SLOT(exitApp()));
        QObject::connect(action_About, SIGNAL(triggered()), MainWindow, SLOT(showAbout()));
        QObject::connect(action_LuxBall, SIGNAL(triggered()), MainWindow, SLOT(setLuxBallScene()));
        QObject::connect(action_LuxBall_HDR, SIGNAL(triggered()), MainWindow, SLOT(setLuxBallHDRScene()));
        QObject::connect(action_Benchmark, SIGNAL(triggered()), MainWindow, SLOT(setBenchmarkMode()));
        QObject::connect(action_Interactive, SIGNAL(triggered()), MainWindow, SLOT(setInteractiveMode()));

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QApplication::translate("MainWindow", "LuxMark", 0, QApplication::UnicodeUTF8));
        action_Quit->setText(QApplication::translate("MainWindow", "&Quit", 0, QApplication::UnicodeUTF8));
        action_Quit->setShortcut(QApplication::translate("MainWindow", "Ctrl+Q", 0, QApplication::UnicodeUTF8));
        action_About->setText(QApplication::translate("MainWindow", "About", 0, QApplication::UnicodeUTF8));
        action_Benchmark->setText(QApplication::translate("MainWindow", "&Benchmark", 0, QApplication::UnicodeUTF8));
        action_Interactive->setText(QApplication::translate("MainWindow", "&Interactive", 0, QApplication::UnicodeUTF8));
        action_LuxBall_HDR->setText(QApplication::translate("MainWindow", "LuxBall &HDR (262K triangles)", 0, QApplication::UnicodeUTF8));
        action_LuxBall_HDR->setShortcut(QApplication::translate("MainWindow", "Ctrl+H", 0, QApplication::UnicodeUTF8));
        action_LuxBall->setText(QApplication::translate("MainWindow", "&LuxBall  (262K triangles)", 0, QApplication::UnicodeUTF8));
        action_LuxBall->setShortcut(QApplication::translate("MainWindow", "Ctrl+L", 0, QApplication::UnicodeUTF8));
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

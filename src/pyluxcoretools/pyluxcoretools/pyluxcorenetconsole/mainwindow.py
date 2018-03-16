# -*- coding: utf-8 -*-

# Form implementation generated from reading ui file '/home/david/projects/luxcorerender/LuxCore/src/pyluxcoretools/pyluxcoretools/pyluxcorenetconsole/mainwindow.ui'
#
# Created: Fri Mar 16 10:16:23 2018
#      by: pyside-uic 0.2.15 running on PySide 1.2.4
#
# WARNING! All changes made in this file will be lost!

from PySide import QtCore, QtGui

class Ui_MainWindow(object):
    def setupUi(self, MainWindow):
        MainWindow.setObjectName("MainWindow")
        MainWindow.resize(600, 600)
        self.centralwidget = QtGui.QWidget(MainWindow)
        self.centralwidget.setObjectName("centralwidget")
        self.gridLayout_2 = QtGui.QGridLayout(self.centralwidget)
        self.gridLayout_2.setObjectName("gridLayout_2")
        self.tabWidget = QtGui.QTabWidget(self.centralwidget)
        self.tabWidget.setObjectName("tabWidget")
        self.jobs = QtGui.QWidget()
        self.jobs.setObjectName("jobs")
        self.gridLayout = QtGui.QGridLayout(self.jobs)
        self.gridLayout.setObjectName("gridLayout")
        self.tabWidget.addTab(self.jobs, "")
        self.nodes = QtGui.QWidget()
        self.nodes.setObjectName("nodes")
        self.gridLayout_3 = QtGui.QGridLayout(self.nodes)
        self.gridLayout_3.setObjectName("gridLayout_3")
        self.tabWidget.addTab(self.nodes, "")
        self.gridLayout_2.addWidget(self.tabWidget, 0, 0, 1, 1)
        self.textEditLog = QtGui.QTextEdit(self.centralwidget)
        self.textEditLog.setUndoRedoEnabled(False)
        self.textEditLog.setLineWrapMode(QtGui.QTextEdit.NoWrap)
        self.textEditLog.setReadOnly(True)
        self.textEditLog.setObjectName("textEditLog")
        self.gridLayout_2.addWidget(self.textEditLog, 1, 0, 1, 1)
        MainWindow.setCentralWidget(self.centralwidget)
        self.menubar = QtGui.QMenuBar(MainWindow)
        self.menubar.setGeometry(QtCore.QRect(0, 0, 600, 25))
        self.menubar.setObjectName("menubar")
        self.menuFile = QtGui.QMenu(self.menubar)
        self.menuFile.setObjectName("menuFile")
        MainWindow.setMenuBar(self.menubar)
        self.statusbar = QtGui.QStatusBar(MainWindow)
        self.statusbar.setObjectName("statusbar")
        MainWindow.setStatusBar(self.statusbar)
        self.actionQuit = QtGui.QAction(MainWindow)
        self.actionQuit.setObjectName("actionQuit")
        self.menuFile.addAction(self.actionQuit)
        self.menubar.addAction(self.menuFile.menuAction())

        self.retranslateUi(MainWindow)
        self.tabWidget.setCurrentIndex(0)
        QtCore.QObject.connect(self.actionQuit, QtCore.SIGNAL("activated()"), MainWindow.clickedQuit)
        QtCore.QMetaObject.connectSlotsByName(MainWindow)
        MainWindow.setTabOrder(self.tabWidget, self.textEditLog)

    def retranslateUi(self, MainWindow):
        MainWindow.setWindowTitle(QtGui.QApplication.translate("MainWindow", "PyLuxCore Tool NetConsole", None, QtGui.QApplication.UnicodeUTF8))
        self.tabWidget.setTabText(self.tabWidget.indexOf(self.jobs), QtGui.QApplication.translate("MainWindow", "Jobs", None, QtGui.QApplication.UnicodeUTF8))
        self.tabWidget.setTabText(self.tabWidget.indexOf(self.nodes), QtGui.QApplication.translate("MainWindow", "Nodes", None, QtGui.QApplication.UnicodeUTF8))
        self.menuFile.setTitle(QtGui.QApplication.translate("MainWindow", "File", None, QtGui.QApplication.UnicodeUTF8))
        self.actionQuit.setText(QtGui.QApplication.translate("MainWindow", "&Quit", None, QtGui.QApplication.UnicodeUTF8))
        self.actionQuit.setShortcut(QtGui.QApplication.translate("MainWindow", "Ctrl+Q", None, QtGui.QApplication.UnicodeUTF8))


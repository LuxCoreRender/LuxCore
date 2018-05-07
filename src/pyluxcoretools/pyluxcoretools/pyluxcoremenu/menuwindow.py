# -*- coding: utf-8 -*-

# Form implementation generated from reading ui file '/home/david/projects/luxcorerender/LuxCore/src/pyluxcoretools/pyluxcoretools/pyluxcoremenu/menuwindow.ui'
#
# Created: Mon May  7 16:37:52 2018
#      by: pyside-uic 0.2.15 running on PySide 1.2.4
#
# WARNING! All changes made in this file will be lost!

from PySide import QtCore, QtGui

class Ui_MenuWindow(object):
    def setupUi(self, MenuWindow):
        MenuWindow.setObjectName("MenuWindow")
        MenuWindow.resize(260, 240)
        sizePolicy = QtGui.QSizePolicy(QtGui.QSizePolicy.Preferred, QtGui.QSizePolicy.Preferred)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(MenuWindow.sizePolicy().hasHeightForWidth())
        MenuWindow.setSizePolicy(sizePolicy)
        self.centralwidget = QtGui.QWidget(MenuWindow)
        self.centralwidget.setObjectName("centralwidget")
        self.verticalLayout = QtGui.QVBoxLayout(self.centralwidget)
        self.verticalLayout.setObjectName("verticalLayout")
        self.pushButtonNetNode = QtGui.QPushButton(self.centralwidget)
        self.pushButtonNetNode.setObjectName("pushButtonNetNode")
        self.verticalLayout.addWidget(self.pushButtonNetNode)
        self.pushButtonNetConsole = QtGui.QPushButton(self.centralwidget)
        self.pushButtonNetConsole.setObjectName("pushButtonNetConsole")
        self.verticalLayout.addWidget(self.pushButtonNetConsole)
        spacerItem = QtGui.QSpacerItem(20, 40, QtGui.QSizePolicy.Minimum, QtGui.QSizePolicy.Expanding)
        self.verticalLayout.addItem(spacerItem)
        self.pushButtonQuit = QtGui.QPushButton(self.centralwidget)
        self.pushButtonQuit.setObjectName("pushButtonQuit")
        self.verticalLayout.addWidget(self.pushButtonQuit)
        MenuWindow.setCentralWidget(self.centralwidget)
        self.menubar = QtGui.QMenuBar(MenuWindow)
        self.menubar.setGeometry(QtCore.QRect(0, 0, 260, 25))
        self.menubar.setObjectName("menubar")
        self.menuTools = QtGui.QMenu(self.menubar)
        self.menuTools.setObjectName("menuTools")
        MenuWindow.setMenuBar(self.menubar)
        self.actionQuit = QtGui.QAction(MenuWindow)
        self.actionQuit.setObjectName("actionQuit")
        self.menuTools.addAction(self.actionQuit)
        self.menubar.addAction(self.menuTools.menuAction())

        self.retranslateUi(MenuWindow)
        QtCore.QObject.connect(self.actionQuit, QtCore.SIGNAL("activated()"), MenuWindow.close)
        QtCore.QObject.connect(self.pushButtonQuit, QtCore.SIGNAL("clicked()"), MenuWindow.close)
        QtCore.QObject.connect(self.pushButtonNetNode, QtCore.SIGNAL("clicked()"), MenuWindow.clickedNetNode)
        QtCore.QObject.connect(self.pushButtonNetConsole, QtCore.SIGNAL("clicked()"), MenuWindow.clickedNetConsole)
        QtCore.QMetaObject.connectSlotsByName(MenuWindow)
        MenuWindow.setTabOrder(self.pushButtonNetNode, self.pushButtonNetConsole)
        MenuWindow.setTabOrder(self.pushButtonNetConsole, self.pushButtonQuit)

    def retranslateUi(self, MenuWindow):
        MenuWindow.setWindowTitle(QtGui.QApplication.translate("MenuWindow", "PyLuxCore Tools Menu", None, QtGui.QApplication.UnicodeUTF8))
        self.pushButtonNetNode.setText(QtGui.QApplication.translate("MenuWindow", "NetNode", None, QtGui.QApplication.UnicodeUTF8))
        self.pushButtonNetConsole.setText(QtGui.QApplication.translate("MenuWindow", "NetConsole", None, QtGui.QApplication.UnicodeUTF8))
        self.pushButtonQuit.setText(QtGui.QApplication.translate("MenuWindow", "Quit", None, QtGui.QApplication.UnicodeUTF8))
        self.menuTools.setTitle(QtGui.QApplication.translate("MenuWindow", "Tools", None, QtGui.QApplication.UnicodeUTF8))
        self.actionQuit.setText(QtGui.QApplication.translate("MenuWindow", "&Quit", None, QtGui.QApplication.UnicodeUTF8))
        self.actionQuit.setShortcut(QtGui.QApplication.translate("MenuWindow", "Ctrl+Q", None, QtGui.QApplication.UnicodeUTF8))


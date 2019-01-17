# -*- coding: utf-8 -*-

# Form implementation generated from reading ui file './luxcorerender/LuxCore/src/pyluxcoretools/pyluxcoretools/pyluxcoremenu/menuwindow.ui',
# licensing of './luxcorerender/LuxCore/src/pyluxcoretools/pyluxcoretools/pyluxcoremenu/menuwindow.ui' applies.
#
# Created: Sat Jan 12 17:36:32 2019
#      by: pyside2-uic  running on PySide2 5.12.0
#
# WARNING! All changes made in this file will be lost!

from PySide2 import QtCore, QtGui, QtWidgets

class Ui_MenuWindow(object):
    def setupUi(self, MenuWindow):
        MenuWindow.setObjectName("MenuWindow")
        MenuWindow.resize(260, 240)
        sizePolicy = QtWidgets.QSizePolicy(QtWidgets.QSizePolicy.Preferred, QtWidgets.QSizePolicy.Preferred)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(MenuWindow.sizePolicy().hasHeightForWidth())
        MenuWindow.setSizePolicy(sizePolicy)
        self.centralwidget = QtWidgets.QWidget(MenuWindow)
        self.centralwidget.setObjectName("centralwidget")
        self.verticalLayout = QtWidgets.QVBoxLayout(self.centralwidget)
        self.verticalLayout.setObjectName("verticalLayout")
        self.pushButtonNetNode = QtWidgets.QPushButton(self.centralwidget)
        self.pushButtonNetNode.setObjectName("pushButtonNetNode")
        self.verticalLayout.addWidget(self.pushButtonNetNode)
        self.pushButtonNetConsole = QtWidgets.QPushButton(self.centralwidget)
        self.pushButtonNetConsole.setObjectName("pushButtonNetConsole")
        self.verticalLayout.addWidget(self.pushButtonNetConsole)
        spacerItem = QtWidgets.QSpacerItem(20, 40, QtWidgets.QSizePolicy.Minimum, QtWidgets.QSizePolicy.Expanding)
        self.verticalLayout.addItem(spacerItem)
        self.pushButtonQuit = QtWidgets.QPushButton(self.centralwidget)
        self.pushButtonQuit.setObjectName("pushButtonQuit")
        self.verticalLayout.addWidget(self.pushButtonQuit)
        MenuWindow.setCentralWidget(self.centralwidget)
        self.menubar = QtWidgets.QMenuBar(MenuWindow)
        self.menubar.setGeometry(QtCore.QRect(0, 0, 260, 25))
        self.menubar.setObjectName("menubar")
        self.menuTools = QtWidgets.QMenu(self.menubar)
        self.menuTools.setObjectName("menuTools")
        MenuWindow.setMenuBar(self.menubar)
        self.actionQuit = QtWidgets.QAction(MenuWindow)
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
        MenuWindow.setWindowTitle(QtWidgets.QApplication.translate("MenuWindow", "PyLuxCore Tools Menu", None, -1))
        self.pushButtonNetNode.setText(QtWidgets.QApplication.translate("MenuWindow", "NetNode", None, -1))
        self.pushButtonNetConsole.setText(QtWidgets.QApplication.translate("MenuWindow", "NetConsole", None, -1))
        self.pushButtonQuit.setText(QtWidgets.QApplication.translate("MenuWindow", "Quit", None, -1))
        self.menuTools.setTitle(QtWidgets.QApplication.translate("MenuWindow", "Tools", None, -1))
        self.actionQuit.setText(QtWidgets.QApplication.translate("MenuWindow", "&Quit", None, -1))
        self.actionQuit.setShortcut(QtWidgets.QApplication.translate("MenuWindow", "Ctrl+Q", None, -1))


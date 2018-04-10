# -*- coding: utf-8 -*-

# Form implementation generated from reading ui file '/home/david/projects/luxcorerender/LuxCore/src/pyluxcoretools/pyluxcoretools/pyluxcorenetconsole/addnodedialog.ui'
#
# Created: Mon Apr  9 16:05:53 2018
#      by: pyside-uic 0.2.15 running on PySide 1.2.4
#
# WARNING! All changes made in this file will be lost!

from PySide import QtCore, QtGui

class Ui_DialogAddNode(object):
    def setupUi(self, DialogAddNode):
        DialogAddNode.setObjectName("DialogAddNode")
        DialogAddNode.resize(400, 150)
        DialogAddNode.setModal(True)
        self.gridLayout = QtGui.QGridLayout(DialogAddNode)
        self.gridLayout.setObjectName("gridLayout")
        self.formLayout = QtGui.QFormLayout()
        self.formLayout.setLabelAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignTrailing|QtCore.Qt.AlignVCenter)
        self.formLayout.setObjectName("formLayout")
        self.label_3 = QtGui.QLabel(DialogAddNode)
        self.label_3.setObjectName("label_3")
        self.formLayout.setWidget(0, QtGui.QFormLayout.LabelRole, self.label_3)
        self.lineEditIPAddress = QtGui.QLineEdit(DialogAddNode)
        sizePolicy = QtGui.QSizePolicy(QtGui.QSizePolicy.Preferred, QtGui.QSizePolicy.Fixed)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(self.lineEditIPAddress.sizePolicy().hasHeightForWidth())
        self.lineEditIPAddress.setSizePolicy(sizePolicy)
        self.lineEditIPAddress.setMinimumSize(QtCore.QSize(100, 0))
        self.lineEditIPAddress.setInputMask("")
        self.lineEditIPAddress.setMaxLength(15)
        self.lineEditIPAddress.setObjectName("lineEditIPAddress")
        self.formLayout.setWidget(0, QtGui.QFormLayout.FieldRole, self.lineEditIPAddress)
        self.label_2 = QtGui.QLabel(DialogAddNode)
        self.label_2.setObjectName("label_2")
        self.formLayout.setWidget(1, QtGui.QFormLayout.LabelRole, self.label_2)
        self.lineEditPort = QtGui.QLineEdit(DialogAddNode)
        sizePolicy = QtGui.QSizePolicy(QtGui.QSizePolicy.Fixed, QtGui.QSizePolicy.Fixed)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(self.lineEditPort.sizePolicy().hasHeightForWidth())
        self.lineEditPort.setSizePolicy(sizePolicy)
        self.lineEditPort.setMinimumSize(QtCore.QSize(100, 0))
        self.lineEditPort.setBaseSize(QtCore.QSize(0, 0))
        self.lineEditPort.setInputMask("")
        self.lineEditPort.setMaxLength(5)
        self.lineEditPort.setObjectName("lineEditPort")
        self.formLayout.setWidget(1, QtGui.QFormLayout.FieldRole, self.lineEditPort)
        self.gridLayout.addLayout(self.formLayout, 1, 0, 1, 2)
        spacerItem = QtGui.QSpacerItem(40, 20, QtGui.QSizePolicy.Expanding, QtGui.QSizePolicy.Minimum)
        self.gridLayout.addItem(spacerItem, 2, 0, 1, 1)
        self.buttonBox = QtGui.QDialogButtonBox(DialogAddNode)
        self.buttonBox.setOrientation(QtCore.Qt.Horizontal)
        self.buttonBox.setStandardButtons(QtGui.QDialogButtonBox.Cancel|QtGui.QDialogButtonBox.Ok)
        self.buttonBox.setObjectName("buttonBox")
        self.gridLayout.addWidget(self.buttonBox, 2, 1, 1, 1)
        self.label = QtGui.QLabel(DialogAddNode)
        self.label.setAlignment(QtCore.Qt.AlignCenter)
        self.label.setObjectName("label")
        self.gridLayout.addWidget(self.label, 0, 0, 1, 2)

        self.retranslateUi(DialogAddNode)
        QtCore.QObject.connect(self.buttonBox, QtCore.SIGNAL("accepted()"), DialogAddNode.accept)
        QtCore.QObject.connect(self.buttonBox, QtCore.SIGNAL("rejected()"), DialogAddNode.reject)
        QtCore.QMetaObject.connectSlotsByName(DialogAddNode)

    def retranslateUi(self, DialogAddNode):
        DialogAddNode.setWindowTitle(QtGui.QApplication.translate("DialogAddNode", "Add a new rendering node", None, QtGui.QApplication.UnicodeUTF8))
        self.label_3.setText(QtGui.QApplication.translate("DialogAddNode", "Hostname or IP address:", None, QtGui.QApplication.UnicodeUTF8))
        self.label_2.setText(QtGui.QApplication.translate("DialogAddNode", "Port:", None, QtGui.QApplication.UnicodeUTF8))
        self.label.setText(QtGui.QApplication.translate("DialogAddNode", "<B>Rendering Node Configuration</B>", None, QtGui.QApplication.UnicodeUTF8))


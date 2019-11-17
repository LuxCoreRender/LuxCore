# -*- coding: utf-8 -*-

# Form implementation generated from reading ui file '/home/david/projects/luxcorerender/LuxCore/src/pyluxcoretools/pyluxcoretools/pyluxcorenetconsole/addnodedialog.ui',
# licensing of '/home/david/projects/luxcorerender/LuxCore/src/pyluxcoretools/pyluxcoretools/pyluxcorenetconsole/addnodedialog.ui' applies.
#
# Created: Fri Nov 15 11:19:32 2019
#      by: pyside2-uic  running on PySide2 5.13.2
#
# WARNING! All changes made in this file will be lost!

from PySide2 import QtCore, QtGui, QtWidgets

class Ui_DialogAddNode(object):
    def setupUi(self, DialogAddNode):
        DialogAddNode.setObjectName("DialogAddNode")
        DialogAddNode.resize(400, 150)
        DialogAddNode.setModal(True)
        self.gridLayout = QtWidgets.QGridLayout(DialogAddNode)
        self.gridLayout.setObjectName("gridLayout")
        self.formLayout = QtWidgets.QFormLayout()
        self.formLayout.setLabelAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignTrailing|QtCore.Qt.AlignVCenter)
        self.formLayout.setObjectName("formLayout")
        self.label_3 = QtWidgets.QLabel(DialogAddNode)
        self.label_3.setObjectName("label_3")
        self.formLayout.setWidget(0, QtWidgets.QFormLayout.LabelRole, self.label_3)
        self.lineEditIPAddress = QtWidgets.QLineEdit(DialogAddNode)
        sizePolicy = QtWidgets.QSizePolicy(QtWidgets.QSizePolicy.Preferred, QtWidgets.QSizePolicy.Fixed)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(self.lineEditIPAddress.sizePolicy().hasHeightForWidth())
        self.lineEditIPAddress.setSizePolicy(sizePolicy)
        self.lineEditIPAddress.setMinimumSize(QtCore.QSize(100, 0))
        self.lineEditIPAddress.setInputMask("")
        self.lineEditIPAddress.setMaxLength(15)
        self.lineEditIPAddress.setObjectName("lineEditIPAddress")
        self.formLayout.setWidget(0, QtWidgets.QFormLayout.FieldRole, self.lineEditIPAddress)
        self.label_2 = QtWidgets.QLabel(DialogAddNode)
        self.label_2.setObjectName("label_2")
        self.formLayout.setWidget(1, QtWidgets.QFormLayout.LabelRole, self.label_2)
        self.lineEditPort = QtWidgets.QLineEdit(DialogAddNode)
        sizePolicy = QtWidgets.QSizePolicy(QtWidgets.QSizePolicy.Fixed, QtWidgets.QSizePolicy.Fixed)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(self.lineEditPort.sizePolicy().hasHeightForWidth())
        self.lineEditPort.setSizePolicy(sizePolicy)
        self.lineEditPort.setMinimumSize(QtCore.QSize(100, 0))
        self.lineEditPort.setBaseSize(QtCore.QSize(0, 0))
        self.lineEditPort.setInputMask("")
        self.lineEditPort.setMaxLength(5)
        self.lineEditPort.setObjectName("lineEditPort")
        self.formLayout.setWidget(1, QtWidgets.QFormLayout.FieldRole, self.lineEditPort)
        self.gridLayout.addLayout(self.formLayout, 1, 0, 1, 2)
        spacerItem = QtWidgets.QSpacerItem(40, 20, QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Minimum)
        self.gridLayout.addItem(spacerItem, 2, 0, 1, 1)
        self.buttonBox = QtWidgets.QDialogButtonBox(DialogAddNode)
        self.buttonBox.setOrientation(QtCore.Qt.Horizontal)
        self.buttonBox.setStandardButtons(QtWidgets.QDialogButtonBox.Cancel|QtWidgets.QDialogButtonBox.Ok)
        self.buttonBox.setObjectName("buttonBox")
        self.gridLayout.addWidget(self.buttonBox, 2, 1, 1, 1)
        self.label = QtWidgets.QLabel(DialogAddNode)
        self.label.setAlignment(QtCore.Qt.AlignCenter)
        self.label.setObjectName("label")
        self.gridLayout.addWidget(self.label, 0, 0, 1, 2)

        self.retranslateUi(DialogAddNode)
        QtCore.QObject.connect(self.buttonBox, QtCore.SIGNAL("accepted()"), DialogAddNode.accept)
        QtCore.QObject.connect(self.buttonBox, QtCore.SIGNAL("rejected()"), DialogAddNode.reject)
        QtCore.QMetaObject.connectSlotsByName(DialogAddNode)

    def retranslateUi(self, DialogAddNode):
        DialogAddNode.setWindowTitle(QtWidgets.QApplication.translate("DialogAddNode", "Add a new rendering node", None, -1))
        self.label_3.setText(QtWidgets.QApplication.translate("DialogAddNode", "Hostname or IP address:", None, -1))
        self.label_2.setText(QtWidgets.QApplication.translate("DialogAddNode", "Port:", None, -1))
        self.label.setText(QtWidgets.QApplication.translate("DialogAddNode", "<B>Rendering Node Configuration</B>", None, -1))


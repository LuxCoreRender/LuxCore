# -*- coding: utf-8 -*-
################################################################################
# Copyright 1998-2018 by authors (see AUTHORS.txt)
#
#   This file is part of LuxCoreRender.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
################################################################################

try:
	import PySide.QtCore as QtCore
	import PySide.QtGui as QtGui
	import PySide.QtGui as QtWidgets
except ImportError:
	try:
		from PySide2 import QtGui, QtCore, QtWidgets
	except ImportError:
		from PySide6 import QtGui, QtCore, QtWidgets

class LogEvent(QtCore.QEvent):
	EVENT_TYPE = QtCore.QEvent.Type(QtCore.QEvent.registerEventType())

	def __init__(self, msg):
		super(LogEvent, self).__init__(self.EVENT_TYPE)
		
		if msg.startswith("[LuxCore]"):
			self.isHtml = True
			self.msg = "<font color='#0000AA'>[LuxCore]</font>" + msg[len("[LuxCore]"):]
		elif msg.startswith("[LuxRays]"):
			self.isHtml = True
			self.msg = "<font color='#00AAAA'>[LuxRays]</font>" + msg[len("[LuxRays]"):]
		elif msg.startswith("[SDL]"):
			self.isHtml = True
			self.msg = "<font color='#00AA00'>[SDL]</font>" + msg[len("[SDL]"):]
		else:
			self.isHtml = False
			self.msg = msg + "\n"

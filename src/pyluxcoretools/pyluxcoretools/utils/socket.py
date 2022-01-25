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

import os
import io
import time
import logging
import socket

import pyluxcoretools.utils.loghandler as loghandler

logger = logging.getLogger(loghandler.loggerName + ".utils.socket")

BUFF_SIZE = 8192

def DataSize(size):
	if (size < 1024):
		return "%d bytes" % size
	elif (size < 1024 * 1024):
		return "%.2f Kbytes" % (size / 1024.0)
	else:
		return "%.2f Mbytes" % (size / (1024.0 * 1024.0))


def RecvLine(soc):
	data = soc.recv(BUFF_SIZE)
	if (not data):
		raise RuntimeError("Unable to receive a line")

	sio = io.StringIO(data.decode("utf-8"))

	# I potentially return only the first line but this function should be use to
	# receive only one line anyway
	return sio.getvalue().splitlines()[0]

def RecvLineWithTimeOut(soc, timeOut):
	soc.settimeout(timeOut)
	try:
		return RecvLine(soc)
	except socket.timeout:
		return None
	finally:
		soc.settimeout(None)

def RecvOk(soc):
	line = RecvLine(soc)
	if (line != "OK"):
		logger.info(line)
		raise RuntimeError("Error while waiting for an OK measage")

def SendLine(soc, msg):
	soc.sendall((msg + "\n").encode("utf-8"))

def SendOk(soc):
	soc.sendall("OK".encode("utf-8"))
	
def SendFile(soc, fileName):
	logger.info("Sending file: " + fileName)
	size = os.path.getsize(fileName)

	# Send the file size
	SendLine(soc, str(size))
	RecvOk(soc)

	# Send the file

	t1 = time.time()
	with open(fileName, "rb") as f:
		for chunk in iter(lambda: f.read(BUFF_SIZE), b""):
			soc.sendall(chunk)
	t2 = time.time()
	dt = t2 - t1

	RecvOk(soc)

	logger.info("Transferred " + DataSize(size) + " in " + time.strftime("%H:%M:%S", time.gmtime(dt)) + " (" + DataSize(0.0 if dt <= 0.0 else size / dt) + "/sec)")

def RecvFile(soc, fileName):
	logger.info("Receiving file: " + fileName)

	# Receive the file size
	transResult = RecvLine(soc)
	if (transResult.startswith("ERROR")):
		raise RuntimeError(transResult)

	size = int(transResult)
	SendOk(soc)

	# Receive the file

	t1 = time.time()
	with open(fileName, "wb") as f:
		recvData = 0
		while recvData < size:
			data = soc.recv(BUFF_SIZE)
			if (len(data) > 0):
				f.write(data)
				recvData += len(data)
			else:
				raise RuntimeError("Interrupted transmission while receiving file: " + fileName)

	t2 = time.time()
	dt = t2 - t1
	
	SendOk(soc)
	
	logger.info("Transferred " + DataSize(size) + " in " + time.strftime("%H:%M:%S", time.gmtime(dt)) + " (" + DataSize(0.0 if dt <= 0.0 else size / dt) + "/sec)")


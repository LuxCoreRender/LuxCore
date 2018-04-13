#! /usr/bin/python
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

import sys
sys.path.append("../lib")

import os
import re
import argparse
import unittest
from time import localtime, strftime
import pyluxcore
import pyluxcoreunittests.tests.utils

import logging
logger = logging.getLogger("pyunittests")
logging.basicConfig(level=logging.INFO, format="[%(threadName)s][%(asctime)s] %(message)s")

printLuxCoreLog = False

def LuxCoreLogHandler(msg):
	if printLuxCoreLog:
		logger.info("[%s]%s" % (strftime("%Y-%m-%d %H:%M:%S", localtime()), msg), file=sys.stderr)

def FilterTests(pattern, testSuite):
	try:
		suite = iter(testSuite)
	except TypeError:
		if not pattern or re.search(pattern, testSuite.id()):
			yield testSuite
	else:
		for test in suite:
			for subtest in FilterTests(pattern, test):
				yield subtest

def ListAllTests(testSuite):
	try:
		suite = iter(testSuite)
	except TypeError:
		yield testSuite.id()
	else:
		for test in suite:
			for subtest in ListAllTests(test):
				yield subtest

class StreamToLogger:
	def __init__( self ):
		self.buf = ""

	def write(self, buf):
		self.buf += buf

		if "\n" in self.buf:
			lines = self.buf.splitlines(keepends=True)
			self.buf = ""
			for line in lines:
				if line.endswith("\n"):
					logger.info(line.rstrip())
				else:
					self.buf += line

	def flush(self):
		logger.info(self.buf)
		self.buf = ""

# To run the tests:
#
# python3 pyluxcoreunittests/unittests.py

def main():
	logger.info("LuxCore Unit tests")

	try:
		pyluxcore.Init(LuxCoreLogHandler)
		logger.info("LuxCore %s" % pyluxcore.Version())
		logger.info("LuxCore has OpenCL: %r" % pyluxcoreunittests.tests.utils.LuxCoreHasOpenCL())

		# Delete all images in the images directory
		logger.info("Deleting all images...")
		folder = "images"
		for f in [png for png in os.listdir(folder) if png.endswith(".png")]:
			filePath = os.path.join(folder, f)
			os.unlink(filePath)
		logger.info("ok")

		# Parse command line options

		parser = argparse.ArgumentParser(description='Runs LuxCore test suite.')
		parser.add_argument('--config',
			help='custom configuration properties for the unit tests')
		parser.add_argument('--filter',
			help='select only the tests matching the specified regular expression')
		parser.add_argument('--list', action='store_true',
			help='list all tests available tests')
		parser.add_argument('--subset', action='store_true',
			help='list all tests available tests')
		parser.add_argument('--verbose', default=2,
			help='set the verbosity level (i.e 0, 1, 2 or 3)')
		args = parser.parse_args()

		global printLuxCoreLog
		if int(args.verbose) >= 3:
			printLuxCoreLog = True

		# Read the custom configuration file
		if args.config:
			LuxCoreTest.customConfigProps.SetFromFile(args.config)

		# Mostly used to save time (to not hit the cap) on Travis CI
		pyluxcoreunittests.tests.utils.USE_SUBSET = args.subset

		# Discover all tests

		propertiesSuite = unittest.TestLoader().discover("pyluxcoreunittests.tests.properties", top_level_dir=".")
		basicSuite = unittest.TestLoader().discover("pyluxcoreunittests.tests.basic", top_level_dir=".")
		lightSuite = unittest.TestLoader().discover("pyluxcoreunittests.tests.lights", top_level_dir=".")
		materialSuite = unittest.TestLoader().discover("pyluxcoreunittests.tests.materials", top_level_dir=".")
		textureSuite = unittest.TestLoader().discover("pyluxcoreunittests.tests.textures", top_level_dir=".")
		sceneSuite = unittest.TestLoader().discover("pyluxcoreunittests.tests.scene", top_level_dir=".")
		haltSuite = unittest.TestLoader().discover("pyluxcoreunittests.tests.halt", top_level_dir=".")
		serializationSuite = unittest.TestLoader().discover("pyluxcoreunittests.tests.serialization", top_level_dir=".")

		allTests = unittest.TestSuite([propertiesSuite, basicSuite, lightSuite, materialSuite,
			textureSuite, sceneSuite, haltSuite, serializationSuite])

		# List the tests if required

		if args.list:
			logger.info("All tests available:")
			l = ListAllTests(allTests)
			count = 0
			for t in l:
				logger.info("  %s" % t)
				count += 1
			logger.info("%d test(s) listed" % count)
			return

		# Filter the tests if required

		if args.filter:
			logger.info("Filtering tests by: %s" % args.filter)
			allTests = unittest.TestSuite(FilterTests(args.filter, allTests))

		result = unittest.TextTestRunner(stream=StreamToLogger(), verbosity=int(args.verbose)).run(allTests)
		sys.exit(not result.wasSuccessful())
	finally:
		pyluxcore.SetLogHandler(None)

if __name__ == "__main__":
    main()

# -*- coding: utf-8 -*-
################################################################################
# Copyright 1998-2015 by authors (see AUTHORS.txt)
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

import unittest
import pyluxcore

class TestProperty(unittest.TestCase):
	def test_Property_GetName(self):
		prop = pyluxcore.Property("test1.prop1", "aa")
		self.assertEqual(prop.GetName(), "test1.prop1")

	def test_Property_Get(self):
		prop = pyluxcore.Property("test1.prop1", "aa")
		self.assertEqual(prop.Get(), ["aa"])
		prop.Add(["bb"])
		self.assertEqual(prop.Get(), ["aa", "bb"])

	def test_Property_ClearGetSize(self):
		prop = pyluxcore.Property("test1.prop1", "aa")
		self.assertEqual(prop.GetSize(), 1)
		prop.Clear()
		self.assertEqual(prop.GetSize(), 0)

	def test_Property_AddSet(self):
		prop = pyluxcore.Property("test1.prop1").Add(["aa"]).Add([1, 2, 3]).Add([4])
		prop.Set(0, 0)
		self.assertEqual(prop.GetSize(), 5)
		self.assertEqual(prop.Get(), [0, 1, 2, 3, 4])

	def test_Property_GetTypes(self):
		prop = pyluxcore.Property("test1.prop1", "aa")
		
		prop.Set(0, True)
		self.assertEqual(prop.GetBool(), True)
		prop.Set(0, 123)
		self.assertEqual(prop.GetInt(), 123)
		prop.Set(0, 123.45)
		self.assertEqual(prop.GetFloat(), 123.45)
		prop.Set(0, "abc")
		self.assertEqual(prop.GetString(), "abc")

		prop.Clear().Add([True, False])
		self.assertEqual(prop.GetBools(), [True, False])
		prop.Clear().Add([123, 456])
		self.assertEqual(prop.GetInts(), [123, 456])
		prop.Clear().Add([123.45, 678.9])
		self.assertEqual(prop.GetFloats(), [123.45, 678.9])
		prop.Clear().Add(["abc", "def"])
		self.assertEqual(prop.GetStrings(), ["abc", "def"])

	def test_Property_GetString(self):
		prop = pyluxcore.Property("test1.prop1", "aa")
		self.assertEqual(prop.GetString(0), "aa")

	def test_Property_String(self):
		prop = pyluxcore.Property("test1.prop1", ["aa", 1])
		self.assertEqual(prop.GetValuesString(), "aa 1")
		self.assertEqual(prop.ToString(), "test1.prop1 = \"aa\" 1")

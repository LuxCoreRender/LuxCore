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

import unittest
import pyluxcore

class TestProperties(unittest.TestCase):
	def test_Properties_SetPrefix(self):
		props = pyluxcore.Properties()
		props.Set(pyluxcore.Property("test1.prop1", "aa"))
		
		props2 = pyluxcore.Properties()
		props2.Set(pyluxcore.Property("aa.prop1", "bb"))
		props2.Set(pyluxcore.Property("bb.prop2", "cc"))
		props.Set(props2, "test2.")

		self.assertEqual(props.GetAllNames(), ["test1.prop1", "test2.aa.prop1", "test2.bb.prop2"])

	def test_Properties_SetFromString(self):
		props = pyluxcore.Properties()
		props.SetFromString("# comment 1\ntest1.prop1 = 1 2.0 aa \"quoted\"\n  # comment 2\ntest2.prop2 = 1 2.0 'quoted' bb\ntest2.prop3 = 1")
		self.assertEqual(props.Get("test1.prop1").Get(), ["1", "2.0", "aa", "quoted"])
		self.assertEqual(props.Get("test2.prop2").Get(), ["1", "2.0", "quoted", "bb"])
		self.assertEqual(props.Get("test2.prop3").Get(), ["1"])

	def test_Properties_SetFromFile(self):
		props = pyluxcore.Properties()
		props.SetFromFile("resources/test.properties")
		self.assertEqual(props.GetSize(), 4)
		self.assertEqual(props.Get("test1.prop1.aa").Get(), ["aa"])
		self.assertEqual(props.Get("test1.prop1.bb").Get(), ["aa", "bb"])
		self.assertEqual(props.Get("test1.prop2.aa").Get(), ["aa"])
		self.assertEqual(props.Get("test2.prop1.aa").Get(), ["aa"])

	def test_Properties_GetAllNames(self):
		props = pyluxcore.Properties()
		props.Set(pyluxcore.Property("test1.prop1", "aa"))
		props.Set(pyluxcore.Property("test1.prop2", "bb"))
		props.Set(pyluxcore.Property("test2.prop1", "cc"))

		self.assertEqual(props.GetAllNames(), ["test1.prop1", "test1.prop2", "test2.prop1"])
		self.assertEqual(props.GetAllNames("test2"), ["test2.prop1"])

	def test_Properties_GetAllNamesRE(self):
		props = pyluxcore.Properties()
		props.Set(pyluxcore.Property("test1.prop1", "aa"))
		props.Set(pyluxcore.Property("test1.prop2", "bb"))
		props.Set(pyluxcore.Property("test2.prop1", "cc"))

		self.assertEqual(props.GetAllNamesRE(".*\\.prop1"), ["test1.prop1", "test2.prop1"])

	def test_Properties_GetAllUniqueSubNames(self):
		props = pyluxcore.Properties()
		props.Set(pyluxcore.Property("test1.prop1.aa", "aa"))
		props.Set(pyluxcore.Property("test1.prop1.bb", "bb"))
		props.Set(pyluxcore.Property("test1.prop1.prop2.aa", "aa"))
		props.Set(pyluxcore.Property("test1.prop1.prop2.bb", "bb"))
		props.Set(pyluxcore.Property("test1.prop2.aa", "aa"))
		props.Set(pyluxcore.Property("test2.prop1.aa", "aa"))
		props.Set(pyluxcore.Property("test2.prop1.bb", "bb"))
		props.Set(pyluxcore.Property("test2.prop2", "aa"))
		props.Set(pyluxcore.Property("test3.prop1.aa", "aa"))

		self.assertEqual(props.GetAllUniqueSubNames("test1"), ["test1.prop1", "test1.prop2"])
		self.assertEqual(props.GetAllUniqueSubNames("test1.prop1"), ["test1.prop1.prop2"])

	def test_Properties_HaveNames(self):
		props = pyluxcore.Properties()
		props.Set(pyluxcore.Property("test1.prop1", "aa"))
		props.Set(pyluxcore.Property("test1.prop2", "bb"))
		props.Set(pyluxcore.Property("test2.prop1", "cc"))

		self.assertEqual(props.HaveNames("test1"), True)
		self.assertEqual(props.HaveNames("test2.prop1"), True)
		self.assertEqual(props.HaveNames("test3"), False)
	
	def test_Properties_HaveNamesRE(self):
		props = pyluxcore.Properties()
		props.Set(pyluxcore.Property("test1.prop1", "aa"))
		props.Set(pyluxcore.Property("test1.prop2", "bb"))
		props.Set(pyluxcore.Property("test2.prop1.aa", "cc"))

		self.assertEqual(props.HaveNamesRE(".*\\.aa"), True)
		self.assertEqual(props.HaveNamesRE(".*\\.prop1"), True)
		self.assertEqual(props.HaveNamesRE("test3.*"), False)

	def test_Properties_Clear(self):
		props = pyluxcore.Properties()
		props.Set(pyluxcore.Property("test1.prop1", "aa"))
		props.Set(pyluxcore.Property("test1.prop2", "bb"))
		props.Set(pyluxcore.Property("test2.prop1", "cc"))
		self.assertEqual(props.GetSize(), 3)

		props.Clear()
		self.assertEqual(props.GetSize(), 0)
		
	def test_Properties_IsDefined(self):
		props = pyluxcore.Properties()
		props.Set(pyluxcore.Property("test1.prop1", "aa"))
		props.Set(pyluxcore.Property("test1.prop2", "bb"))
		props.Set(pyluxcore.Property("test2.prop1.aa", "cc"))

		self.assertEqual(props.IsDefined("test1.prop2"), True)
		self.assertEqual(props.IsDefined("test1.prop2.aa"), False)
		
	def test_Properties_Delete(self):
		props = pyluxcore.Properties()
		props.Set(pyluxcore.Property("test1.prop1", "aa"))
		props.Set(pyluxcore.Property("test1.prop2", "bb"))
		props.Set(pyluxcore.Property("test2.prop1.aa", "cc"))
		self.assertEqual(props.GetSize(), 3)
		
		self.assertEqual(props.IsDefined("test1.prop1"), True)
		props.Delete("test1.prop1")
		self.assertEqual(props.GetSize(), 2)
		self.assertEqual(props.IsDefined("test1.prop1"), False)
		
		self.assertEqual(props.IsDefined("test1.prop2"), True)
		props.DeleteAll(["test1.prop2"]);
		self.assertEqual(props.GetSize(), 1)
		self.assertEqual(props.IsDefined("test1.prop2"), False)

	def test_Properties_ToString(self):
		props = pyluxcore.Properties()
		props.Set(pyluxcore.Property("test1.prop1", "aa"))
		props.Set(pyluxcore.Property("test1.prop2", "bb"))
		props.Set(pyluxcore.Property("test2.prop1.aa", "cc"))

		self.assertEqual(props.ToString(), 'test1.prop1 = "aa"\ntest1.prop2 = "bb"\ntest2.prop1.aa = "cc"\n')

	def test_Properties_SpaceInName(self):
		props = pyluxcore.Properties()
		props.Set(pyluxcore.Property("aa.b b.cc", "123"))
		
		self.assertEqual(props.Get("aa.b b.cc").Get(), ["123"])
		
		props = pyluxcore.Properties()
		props.SetFromString("test1.prop1  = 1 2.0 aa \"quoted\"\ntest2.pr op2   = 1 2.0 'quoted' bb\ntest2.prop3 = 1")
		self.assertEqual(props.Get("test1.prop1").Get(), ["1", "2.0", "aa", "quoted"])
		self.assertEqual(props.Get("test2.pr op2").Get(), ["1", "2.0", "quoted", "bb"])
		self.assertEqual(props.Get("test2.prop3").Get(), ["1"])

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

from pyluxcoreunittests.tests.utils import *
from pyluxcoreunittests.tests.imagetest import *

# Note: keep in alphabetical order

################################################################################
# Bevel shape test: meshes without a convex edge
################################################################################

# A bevel shape whose source has no convex edge (a flat panel, a concave fold,
# or a piece split off a solid) used to crash while building the bevel
# accelerator over zero primitives. Each case renders in a child process so a
# crash shows as a failed test instead of ending the test run.
BEVEL_SCENE_TEMPLATE = """
scene.camera.lookat.orig = 30 -30 30
scene.camera.lookat.target = 5 5 0
scene.materials.grey.type = matte
scene.materials.grey.kd = 0.6 0.6 0.6
scene.shapes.source.type = inlinedmesh
scene.shapes.source.vertices = {vertices}
scene.shapes.source.faces = {faces}
scene.shapes.bevelled.type = bevel
scene.shapes.bevelled.source = source
scene.shapes.bevelled.bevel.radius = 0.5
scene.objects.obj.shape = bevelled
scene.objects.obj.material = grey
scene.lights.sky.type = constantinfinite
"""

BEVEL_CHILD = """
import os
import sys
import time
import pyluxcore
pyluxcore.Init(lambda msg: None)
scene = pyluxcore.Scene()
props = pyluxcore.Properties()
props.SetFromString(sys.argv[1])
scene.Parse(props)
cfg = pyluxcore.Properties()
cfg.SetFromString(\"\"\"
renderengine.type = PATHCPU
sampler.type = RANDOM
film.width = 32
film.height = 24
batch.haltspp = 1
\"\"\")
config = pyluxcore.RenderConfig(cfg, scene)
session = pyluxcore.RenderSession(config)
session.Start()
while True:
	time.sleep(0.2)
	# Runs the halt test (batch.haltspp)
	session.UpdateStats()
	if session.HasDone():
		break
session.Stop()
print("BEVEL-RENDER-OK", flush=True)
# Skip interpreter teardown: pyluxcore can crash while unloading after a
# successful render (unrelated to bevel), which would hide this test's result.
os._exit(0)
"""

BEVEL_MESHES = {
	"FlatQuad": ("0 0 0  10 0 0  10 10 0  0 10 0", "0 1 2  0 2 3"),
	"ConcaveFold": ("0 0 0  10 0 0  10 10 0  0 0 10", "0 1 2  0 3 1"),
	# Control: a box has convex edges and must still render with its bevel
	"Box": ("0 0 0  10 0 0  10 10 0  0 10 0  0 0 10  10 0 10  10 10 10  0 10 10",
		"0 2 1  0 3 2  4 5 6  4 6 7  0 1 5  0 5 4  1 2 6  1 6 5  2 3 7  2 7 6  3 0 4  3 4 7"),
}

class BevelShapeWithoutConvexEdges(unittest.TestCase):
	def RenderBevel(self, name):
		import subprocess
		import sys
		vertices, faces = BEVEL_MESHES[name]
		result = subprocess.run(
			[sys.executable, "-c", BEVEL_CHILD,
				BEVEL_SCENE_TEMPLATE.format(vertices=vertices, faces=faces)],
			capture_output=True, text=True, timeout=120)
		self.assertEqual(result.returncode, 0,
			"bevel on %s: exit code %d\n%s" % (name, result.returncode, result.stderr[-2000:]))
		self.assertIn("BEVEL-RENDER-OK", result.stdout)

	def test_BevelShape_FlatQuad(self):
		self.RenderBevel("FlatQuad")

	def test_BevelShape_ConcaveFold(self):
		self.RenderBevel("ConcaveFold")

	def test_BevelShape_Box(self):
		self.RenderBevel("Box")

################################################################################
# Displacement shape test
################################################################################

def TestDisplacementShape(cls, params):
	StandardSceneTest(cls, params, "simple/shape-displacement-area.cfg", "DisplacementShape")

class DisplacementShape(ImageTest):
    pass

DisplacementShape = AddTests(DisplacementShape, TestDisplacementShape, GetTestCases())

################################################################################
# Subdiv shape test
################################################################################

def TestSubdivShape(cls, params):
	StandardSceneTest(cls, params, "simple/shape-subdiv-area.cfg", "SubdivShape")

class SubdivShape(ImageTest):
    pass

SubdivShape = AddTests(SubdivShape, TestSubdivShape, GetTestCases())

################################################################################
# Simplify shape test
################################################################################

def TestSimplifyShape(cls, params):
	StandardSceneTest(cls, params, "simple/shape-simplify-area.cfg", "SimplifyShape")

class SimplifyShape(ImageTest):
    pass

SimplifyShape = AddTests(SimplifyShape, TestSimplifyShape, GetTestCases())

################################################################################
# Copyright 1998-2013 by authors (see AUTHORS.txt)
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

import time
import sys
sys.path.append("./lib")

import pyluxcore

print("LuxCore %s\n" % pyluxcore.version())

################################################################################
## Properties examples
################################################################################

print("Properties examples...")
prop = pyluxcore.Property("test1.prop1", "aa")
print("test1.prop1 => %s\n" % prop.GetValue(0))

prop.Clear().Add(0).Add(2).Add(3)
prop.Set(0, 1)
print("[%s]\n" % prop.ToString())

print("[%s]\n" % pyluxcore.Property("test1.prop1").Add(1).Add(2).Add(3))

prop = pyluxcore.Property("test1.prop1", [True, 1, 2.0, "aa"])
print("[%s]" % prop)
print("Size: %d" % prop.GetSize())
print("List: %s\n" % str(prop.GetValues()))

props = pyluxcore.Properties()
props.SetFromString("test1.prop1 = 1 2.0 aa \"quoted\"\ntest2.prop2 = 1 2.0 'quoted' bb\ntest2.prop3 = 1")
print("[\n%s]\n" % props)

print("%s" % props.GetAllNames())
print("%s" % props.GetAllNames("test1"))
print("%s\n" % props.GetAllUniqueSubNames("test2"))

props0 = pyluxcore.Properties()
props1 = pyluxcore.Properties() \
	.Set(pyluxcore.Property("test1.prop1", [True, 1, 2.0, "aa"])) \
	.Set(pyluxcore.Property("test2.prop1", ["bb"]));

props0.Set(props1, "prefix.")
print("[\n%s]\n" % props0)

print("Get: %s" % props0.Get("prefix.test1.prop1"))
print("Get default: %s\n" % props0.Get("doesnt.exist", ["default_value0", "default_value1"]))

################################################################################
## RenderConfig and RenderSession examples
################################################################################

print("RenderConfig and RenderSession examples (requires scenes directory)...")

# Load the configuration from file
props = pyluxcore.Properties("scenes/luxball/luxball-hdr.cfg")

# Change the render engine to PATHCPU
props.Set(pyluxcore.Property("renderengine.type", ["PATHCPU"]))

config = pyluxcore.RenderConfig(props)
session = pyluxcore.RenderSession(config)

session.Start()

startTime = time.time()
while True:
	time.sleep(1)

	elapsedTime = time.time() - startTime

	# Print some information about the rendering progress

	# Update statistics
	session.UpdateStats()

	stats = session.GetStats();
	print("[Elapsed time: %3d/5sec][Samples %4d][Avg. samples/sec % 3.2fM on %.1fK tris]" % (\
			stats.Get("stats.renderengine.time").GetFloat(), \
			stats.Get("stats.renderengine.pass").GetInt(), \
			(stats.Get("stats.renderengine.total.samplesec").GetFloat()  / 1000000.0), \
			(stats.Get("stats.dataset.trianglecount").GetFloat() / 1000.0)))

	if elapsedTime > 5.0:
		# Time to stop the rendering
		break

session.Stop()

# Save the rendered image
session.SaveFilm()

print("Done.\n")

################################################################################

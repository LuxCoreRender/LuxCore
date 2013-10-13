import sys
sys.path.append(".")

import pyluxcore

print("LuxCore %s\n" % pyluxcore.version())

################################################################################

print("Properties examples...")
prop = pyluxcore.Property("test1.prop1", "aa")
print("test1.prop1 => %s\n" % prop.Get(0))

prop.Clear().Add(0).Add(2).Add(3)
prop.Set(0, 1)
print(prop.ToString())

print(pyluxcore.Property("test1.prop1").Add(1).Add(2).Add(3).ToString())

################################################################################

import sys
sys.path.append(".")

import pyluxcore

print("LuxCore %s\n" % pyluxcore.version())

################################################################################

print("Properties examples...")
prop = pyluxcore.Property("test1.prop1", "aa")
print("test1.prop1 => %s\n" % prop.Get(0))

################################################################################

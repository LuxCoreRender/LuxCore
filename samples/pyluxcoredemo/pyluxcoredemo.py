import sys
sys.path.append(".")

import pyluxcore

print("LuxCore %s\n" % pyluxcore.version())

################################################################################
## Properties example
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
print("List: %s\n" % str(prop.Get()))

props = pyluxcore.Properties()
props.SetFromString("test1.prop1 = 1 2.0 aa \"quoted\"\ntest2.prop2 = 1 2.0 'quoted' bb\ntest2.prop3 = 1")
print("[\n%s]\n" % props)

print("%s" % props.GetAllNames())
print("%s" % props.GetAllNames("test1."))
print("%s\n" % props.GetAllUniqueNames("test2."))

################################################################################

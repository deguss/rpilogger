#!/usr/bin/python -u

print("test directory listing with glob module")
_DIR = "data/tmp/*"
print("ls "+_DIR)

import glob
gen = glob.iglob(_DIR)

for elem in gen:
    print(elem)

print("end")

#!/usr/bin/python

import string

f = open("/proc/distance_me007","r")
d = f.readline()
val = d.split(":")
dist = string.atof(val[1])/5764

print "Distance: %6.2f mm" % dist

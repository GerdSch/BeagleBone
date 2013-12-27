#!/usr/bin/python

import string
import re
import time

i=0
# max. 25 tries to read the values
max_repeat = 25

while i<max_repeat: 
	f = open("/proc/dht11_data","r")
	d = f.readline()
	m = re.findall(r'\d+',d)
	if int(m[4])==0:
		m=re.search("Humidity: (.*) Temperature: (.*) - ",d)
		HR = float(m.group(1))
		T = float(m.group(2))
		print "relative humidity: %f" % HR
		print "Temperature %f C" % T
		i=max_repeat
	f.close
	i=i+1
        if (i<max_repeat):
		time.sleep(5)

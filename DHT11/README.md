# Beaglebone Black DHT11 Kernel modules

Read temperature and relative humidity from DHT11 (http://www.micro4you.com/files/sensor/DHT11.pdf) and write the values to the /proc/dht11_data file. 

##Compile, install and read values from the DHT11 module
make ARCH=arm			compile the module
insmod dht11.ko			install the module into the running kernel
cat /proc/dht11_data	read the raw data provided by the kernel module --> if Status = 0 the values are valid
./get_dht11_values.py 	Pyhton script to read the values (read will be repeated until the values are valid)

##Hardware setup
+		P9/3 - VDD_3V3B
out		P9/12 - GPIO1_28	--> output/input data line
- 		P9/1 - D GND


 
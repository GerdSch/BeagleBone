# Beaglebone Black Kernel modules

Each directory contains the source file of the kernel module witch the appropriate Makefile. The module should be compiled with __make ARCH=arm__.
After __insmod module_name__ a new file in the /proc file system will be created. The example python script within the module directory reads the proc file and creates an output.  

##DHT11
Temperature and humidity measurement. The specification of the sensor can be downloaded from http://www.micro4you.com/files/sensor/DHT11.pdf 

##ME007
Distance measurement based on ultrasonic DYP-ME007 sensor (http://www.elektor.com/Uploads/2013/POST/PROJECTS/13/files/datasheets/DYP-ME007.pdf)
 
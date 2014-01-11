# Beaglebone Black DYP-ME007 Kernel modules

Read signal runtime from DYP-ME007U2 (http://www.elektor.com/Uploads/2013/POST/PROJECTS/13/files/datasheets/DYP-ME007.pdf), write it to /proc/runtime_me007 and calculate the distance in mm with get_distance.py.

##Compile, install and read distance from the distance_me007 module
make ARCH=arm				compile the module
insmod distance_me007.ko	install the module into the running kernel
cat /proc/runtime_me007		read the raw data provided by the kernel module 
./get_disctance.py 			Pyhton script to read the runtime and calculate the distance in mm

##Hardware setup              
1 VCC		- P9/6	- 	VDD_5V               
2 Trigger	- P8/12 -	GPIO1_12                   
3 Echo		- P9/15	- 	GPIO1_16               
4 Out		- no connection                   
5 GND		- P9/1	-	D_GND                  
           
P9/15: a 100Ohm is used to ensure the right level                    



 
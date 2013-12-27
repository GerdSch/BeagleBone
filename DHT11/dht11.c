/*******************************************************************************
 *                                                                             *
 * Kernel module for the DHT11 Temp and Humidity sensor                        *
 *                                                                             *
 * Gerd Schlager                                               2.10.2013       *
 *                                                                             *
 * TRG and data signal p9/12                                                 *
 *                                                                             *
 ******************************************************************************/

#define BEAGLEBONE		// BB and BBB,   

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include <linux/delay.h>

#include <linux/proc_fs.h>	/* Necessary because we use the proc fs */
#include <asm/uaccess.h>	/* for copy_from_user */

#define PROCFS_MAX_SIZE		512
#define PROCFS_NAME 		"dht11_data"

#define AM33XX_CONTROL_BASE	0x44e10000

#define DRV_NAME		"dht11"

#define START_LOW		20	// at least 18ms pull down to start communication 

uint state;				// 0 ... new
					// 1 ... trigger was sent
uint z, i;
char st[50];
						
static struct proc_dir_entry *info_file;
static char procfs_buffer[PROCFS_MAX_SIZE];

struct measure_dist {
   struct timespec trg_time, irq_time_high, irq_time_low;
   u16 irq;
   u8 irq_high, irq_low;
   u16 irq_pin;				// IRQ and data pin
};

static struct measure_dist test_data;
static int setup_pinmux_in(void);
static int setup_pinmux_out(void);
static void start_measure(unsigned long ptr);

/*********************************************************************************
*
* insert content into proc file if activated from user space
*
*********************************************************************************/
int proc_read(char *buf, char **start, off_t offset, int count, int *eof)
{
    int len;
	
    start_measure((unsigned long)&test_data);
	
	/* Initialize the total length */
    len = 0;    /* Format the data in the buffer */
    len += sprintf( buf + len, procfs_buffer );
   
    /* If our data length is smaller than what
       the application requested, mark End-Of-File */
    if( len <= count + offset )
        *eof = 1;    /* Initialize the start pointer */
    *start = procfs_buffer + offset; 

    /* Reduce the offset from the total length */
    len -= offset;
   
    /* Normalize the len variable */
    if( len > count )
        len = count; 
    if( len < 0 )
        len = 0;
 
    return len;
}
/*********************************************************************************
*
* measure time between each interrupt
*
*********************************************************************************/
static irqreturn_t measure_distance_iterrupt(int irq, void* dev_id)
{
   struct timespec delta;
   struct measure_dist* data = (struct measure_dist*)dev_id;
   
   if (gpio_get_value(data->irq_pin)>0) {
		getnstimeofday(&data->irq_time_high);
		state = 1;	// in high state
   }
   else {
		if (state == 1) {
			getnstimeofday(&data->irq_time_low);
			delta = timespec_sub(data->irq_time_low, data->irq_time_high);

			if (delta.tv_nsec < 65000) st[z]='0';
			else st[z]='1';
			//sprintf(str, "%d - high for: %lu - %s\n" , z, delta.tv_nsec, st[z]);
			//printk(KERN_INFO "%s", str);
			state = 0;
			z++;
			}
	   }
  
   return IRQ_HANDLED;
}
/*********************************************************************************
*
* configure pin als input pin with interrupt capability
*
*********************************************************************************/
static int configure_input(void) {
	int err;
	err = setup_pinmux_in();

	err = gpio_request_one(test_data.irq_pin, GPIOF_IN, DRV_NAME " irq input");
	if (err < 0) {
		printk(KERN_ALERT DRV_NAME " : failed to request IRQ pin %d.\n", test_data.irq_pin);
	}
	
	err = gpio_to_irq(test_data.irq_pin);
	if (err < 0) {
	      printk(KERN_ALERT DRV_NAME " : failed to get IRQ for irq pin %d.\n", test_data.irq_pin);
	      goto err_free_irq_return;
	} else {
	      test_data.irq = (u16)err;
	      err = 0;
	}
	
	test_data.irq_high = 0;
	test_data.irq_low = 0;
	
	err = request_any_context_irq(
	test_data.irq,
	measure_distance_iterrupt,
	//IRQF_TRIGGER_RISING | IRQF_DISABLED,
	IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
	DRV_NAME,
	(void*)&test_data
   );
   
   if (err < 0) {
      printk(KERN_ALERT DRV_NAME " : failed to enable IRQ %d for pin %d.\n",
         test_data.irq, test_data.irq_pin);
      goto err_free_irq_return;
   }
   
   return 0;
   
err_free_irq_return:
   gpio_free(test_data.irq_pin);
}
/*********************************************************************************
*
* configure pin als output pin
*
*********************************************************************************/
static int configure_output(void) {
   int err;

   err = setup_pinmux_out();
   state = 0;
   
   if (err < 0) {
      printk(KERN_ALERT DRV_NAME " : failed to apply pinmux settings.\n");
      goto err_return;
   }

   err = gpio_request_one(test_data.irq_pin, GPIOF_OUT_INIT_HIGH, DRV_NAME " irq input");
   if (err < 0) {
      printk(KERN_ALERT DRV_NAME " : failed to request IRQ pin %d.\n", test_data.irq_pin);
   }
	
   return 0;

err_return:
   return err;
}
/*********************************************************************************
*
* Free input pin after measurement
*
*********************************************************************************/
static void free_input(void) {
   free_irq(test_data.irq, (void*)&test_data);
   gpio_free(test_data.irq_pin);
}
/*********************************************************************************
*
* start measurement
*
*********************************************************************************/
static void start_measure(unsigned long ptr) {
	struct measure_dist* data = (struct measure_dist*)ptr;
	uint le = 0;
	
	unsigned int T_int=0;
	unsigned int T_dec=0;
	unsigned int HR_int=0;
	unsigned int HR_dec=0;
	unsigned int CRK_sum=0;
	int status;
	char str[50];
		
	z=0;
	state=0;
	
	configure_output();
	gpio_set_value(data->irq_pin, 0);
	msleep(START_LOW);
	gpio_free(test_data.irq_pin);
	configure_input();
	
	msleep(10);
	
	le = z-40;
	
	for (i=0;i<8;i++) {
		if (st[le+i]=='1') HR_int += 1 << (7-i); 
		if (st[le+i+8]=='1') HR_dec += 1 << (7-i); 
		if (st[le+i+16]=='1') T_int += 1 << (7-i); 
		if (st[le+i+24]=='1') T_dec += 1 << (7-i);
		if (st[le+i+32]=='1') CRK_sum += 1 << (7-i);
	}
	
	status = CRK_sum-HR_int-HR_dec-T_int-T_dec;
	
	sprintf(str, "Humidity: %d.%d Temperature: %d.%d - (Status: %d)\n",HR_int,HR_dec,T_int,T_dec,status);
	memcpy(procfs_buffer,str,strlen(str));
	
	free_input();
}

/*********************************************************************************
*
* init kernel module
*
*********************************************************************************/
int __init distance_measure_init_module(void)
{
	/* create the proc file */
	info_file = create_proc_entry(PROCFS_NAME, 0666, NULL);
	
	if (info_file == NULL) {
		remove_proc_entry(PROCFS_NAME, NULL);
		printk(KERN_ALERT "Error: Could not initialize /proc/%s file!\n",PROCFS_NAME);
		return -ENOMEM;
	}

	info_file->read_proc  	= proc_read;
	info_file->write_proc 	= NULL;
	info_file->mode   	= S_IFREG | S_IRUGO;
	info_file->uid 		= 0;
	info_file->gid 		= 0;
	info_file->size   	= 37;

	printk(KERN_INFO "/proc/%s created\n", PROCFS_NAME);
	
	return 0;	
}

void __exit distance_measure_exit_module(void)
{
   if (info_file) {
		remove_proc_entry(PROCFS_NAME, NULL);
		printk(KERN_INFO PROCFS_NAME " removed.\n");
   }
   
   printk(KERN_INFO DRV_NAME " : unloaded dht11 module\n");
}

module_init(distance_measure_init_module);
module_exit(distance_measure_exit_module);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("kernel module to measure temperature & humidity with DHT11");
MODULE_VERSION("1.0");


static int setup_pinmux_out(void)
{
   static u32 pins[] = {
      AM33XX_CONTROL_BASE + 0x878,   	// test pin (60): gpio1_28 (beaglebone p9/12)
      0x7 | (2 << 3),        		//       mode 7 (gpio), PULLUP, OUTPUT, 
   };

   void* addr = ioremap(pins[0], 4);

      if (NULL == addr)
         return -EBUSY;

   iowrite32(pins[1], addr);
   iounmap(addr);

   test_data.irq_pin = 60;	// output 

   return 0;
}
static int setup_pinmux_in(void)
{
   static u32 pins[] = {
      AM33XX_CONTROL_BASE + 0x878,   	// test pin (60): gpio1_28 (beaglebone p9/12)
      0x17 | (2 << 3) | (1 << 5),     //       mode 7 (gpio), PULLUP, INPUT
   };

   void* addr = ioremap(pins[0], 4);

      if (NULL == addr)
         return -EBUSY;

   iowrite32(pins[1], addr);
   iounmap(addr);

   test_data.irq_pin = 60;	// input 

   return 0;
}

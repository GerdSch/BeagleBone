/*******************************************************************************
 *                                                                             *
 * distance measurement with DYP-ME007U2                                             *
 *                                                                             *
 * Gerd Schlager                                               2.10.2013       *
 *                                                                             *
 * TRG signal 10mysec on p8/12                                                 *
 * ECHO measure high time on the p9/15                                         *
 *                                                                             *
 * VCC P9/7																	   *
 * ECHO - 10Ohm - P9/15 - 10Ohm - Ground 									   *
 ******************************************************************************/

#define BEAGLEBONE      // BB and BBB,   
// output p8/12
// input  p9/15

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

#define PROCFS_MAX_SIZE		1024
#define PROCFS_NAME 		"distance_me007"

#define DRV_NAME           "me007"

uint state;				// 0 ... new
						// 1 ... trigger was sent
/**
 * This structure hold information about the /proc file
 *
 */
static struct proc_dir_entry *info_file;

/**
 * The buffer used to store character for this module
 *
 */
static char procfs_buffer[PROCFS_MAX_SIZE];

struct measure_dist {
   struct timespec trg_time, irq_time_high, irq_time_low;
   u16 irq;
   u8 irq_high, irq_low;
   u16 trg_pin;				// Trigger pin
   u16 irq_pin;				// IRQ - Measure pin
};

static struct measure_dist test_data;
static int setup_pinmux(void);
static void set_trigger(unsigned long ptr);
/**
 * This funtion is called when the /proc file is read
 *
 */
static int proc_read(char *buf, char **start, off_t offset, int count, int *eof)
{
    int len;
	
    set_trigger((unsigned long)&test_data);

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

// measure distance due to the interrupt
static irqreturn_t measure_distance_iterrupt(int irq, void* dev_id)
{
   char str[50];
   int val;
   struct timespec delta;
   struct measure_dist* data = (struct measure_dist*)dev_id;
   
   val = gpio_get_value(data->irq_pin);
   
   if (val>0) {
		getnstimeofday(&data->irq_time_high);
   }
   else {
		getnstimeofday(&data->irq_time_low);
		delta = timespec_sub(data->irq_time_low, data->irq_time_high);
		sprintf(str, "runtime: %lu\n" , delta.tv_nsec);
		memcpy(procfs_buffer,str,strlen(str));
		state = 2;
	   }
  
   return IRQ_HANDLED;
}

static void set_trigger(unsigned long ptr) {
	struct measure_dist* data = (struct measure_dist*)ptr;
	
	state = 1;
	
	getnstimeofday(&data->trg_time);
	gpio_set_value(data->trg_pin, 1);
	udelay(10);
	gpio_set_value(data->trg_pin, 0);
	msleep(500);		// sleep 500 msec to ensure the signal is completed
}

/**
* Kernel modul init
**/
int __init distance_measure_init_module(void)
{
   int err;

   err = setup_pinmux();
   state = 0;
   
   if (err < 0) {
      printk(KERN_ALERT DRV_NAME " : failed to apply pinmux settings.\n");
      goto err_return;
   }

	/* create the proc file */
	info_file = create_proc_entry(PROCFS_NAME, 0644, NULL);
	
	if (info_file == NULL) {
		remove_proc_entry(PROCFS_NAME, NULL);
		printk(KERN_ALERT "Error: Could not initialize /proc/%s file!\n",PROCFS_NAME);
		return -ENOMEM;
	}

	info_file->read_proc  	= proc_read;
	info_file->write_proc 	= NULL;
	info_file->mode 	  	= S_IFREG | S_IRUGO;
	info_file->uid 	  		= 0;
	info_file->gid 	  		= 0;
	info_file->size 	  	= 37;

	printk(KERN_INFO "/proc/%s created\n", PROCFS_NAME);	

   err = gpio_request_one(test_data.trg_pin, GPIOF_OUT_INIT_LOW, DRV_NAME "  trigger output");
   if (err < 0) {
      printk(KERN_ALERT DRV_NAME " : failed to request TRG pin %d.\n", test_data.trg_pin);
   }
   
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
      IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
      DRV_NAME,
      (void*)&test_data
   );
   
   if (err < 0) {
      printk(KERN_ALERT DRV_NAME " : failed to enable IRQ %d for pin %d.\n", test_data.irq, test_data.irq_pin);
      goto err_free_irq_return;
   } 
   
   return 0;

err_free_irq_return:
   gpio_free(test_data.irq_pin);
err_return:
   return err;
}

void __exit distance_measure_exit_module(void)
{
   gpio_set_value(test_data.trg_pin, 0);
   free_irq(test_data.irq, (void*)&test_data);
	
   gpio_free(test_data.irq_pin);
   gpio_free(test_data.trg_pin);
   
   if (info_file) {
		remove_proc_entry(PROCFS_NAME, NULL);
		printk(KERN_INFO PROCFS_NAME " removed.\n");
   }
   
   printk(KERN_INFO DRV_NAME " : unloaded distance DYP-ME007U2 measurement\n");
}

module_init(distance_measure_init_module);
module_exit(distance_measure_exit_module);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("kernel module to measure the distance with DYP-ME007U2");
MODULE_VERSION("1.0");

#ifdef BEAGLEBONE
#define AM33XX_CONTROL_BASE		0x44e10000
static int setup_pinmux(void)
{
   int i;
   static u32 pins[] = {
      AM33XX_CONTROL_BASE + 0x830,   // test pin (44): gpio1_12 (beaglebone p8/12)
      0x7 | (2 << 3),                //       mode 7 (gpio), PULLUP, OUTPUT
      AM33XX_CONTROL_BASE + 0x840,   // irq pin (48): gpio1_16 (beaglebone p9/15)
      0x17 | (2 << 3) | (1 << 5),     //       mode 7 (gpio), PULLUP, INPUT
   };

   for (i=0; i<4; i+=2) {
      void* addr = ioremap(pins[i], 4);

      if (NULL == addr)
         return -EBUSY;

      iowrite32(pins[i+1], addr);
      iounmap(addr);
   }

   test_data.irq_pin = 48;	// input
   test_data.trg_pin = 44;	// output

   return 0;
}
#else
#error PLEASE CHOOSE A VALID BOARD AT THE HEAD OF THIS FILE BEFORE COMPILING!
static int setup_pinmux(void) { return -ENODEV; }
#endif

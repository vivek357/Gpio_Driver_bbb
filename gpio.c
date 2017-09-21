#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <uapi/asm-generic/errno-base.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/time.h>

//------------------------------------------------------/* User-defined macros */-------------------------------------------
#define NUM_GPIO_PINS 19
#define MAX_GPIO_NUMBER 117
#define DEVICE_NAME "bbb-gpio"
#define BUF_SIZE 512
#define INTERRUPT_DEVICE_NAME "gpio interrupt"

//------------------------------------------------------/* User-defined data types */--------------------------------------------
enum state {low, high};
enum direction {in, out};

/*
* struct bbb_gpio_dev -Per gpio pin data structure
* @cdev:instance of struct cdev
* @pin:instance of struct gpio
* @state:logic state (low, high) of a GPIO pin
* @dir:direction of a GPIO pin
* @irq_perm: used to enable/disable interrupt on GPIO pin
* @irq_flag:used to indicate rising/falling edge trigger
* @lock:used to protect atomic code section
*/
 
struct bbb_gpio_dev 
{
	struct cdev cdev;
	struct gpio pin;
	enum state state;
	enum direction dir;
	bool irq_perm;
	unsigned long irq_flag;
	unsigned int irq_counter;
	spinlock_t lock;
};


//------------------------------------------------------------------/* Declaration of entry points */-------------------------

static int bbb_gpio_open(struct inode *inode, struct file *filp);
static ssize_t bbb_gpio_read (struct file *filp, char *buf, size_t count, loff_t *f_pos);
static ssize_t bbb_gpio_write (struct file *filp, const char *buf, size_t count, loff_t *f_pos);
static int bbb_gpio_release(struct inode *inode, struct file *filp);


//-----------------------------------------------------------------/* File operation structure */------------------------------
static struct file_operations bbb_gpio_fops = {
	.owner = THIS_MODULE,
	.open= bbb_gpio_open,
	.release= bbb_gpio_release,
	.read= bbb_gpio_read,
	.write= bbb_gpio_write,
	};



static int bbb_gpio_init(void);
static void bbb_gpio_exit(void);
unsigned int millis (void);
static irqreturn_t irq_handler(int irq, void *arg);

//----------------------------------------------------------------/*global variables*/---------------------------------------
struct bbb_gpio_dev *bbb_gpio_devp[NUM_GPIO_PINS];
static dev_t first;
static struct class *bbb_gpio_class;
static unsigned int last_interrupt_time = 0;
static uint64_t epochMilli;

/*
* millis - Get current time
*
* This function returns current time in ms. It is primarily used for
* debouncing
*/

unsigned int millis (void)
{
	struct timeval timeval ;
	uint64_t timeNow ;
	do_gettimeofday(&timeval) ;
	timeNow=(uint64_t)timeval.tv_sec * (uint64_t)1000 + (uint64_t)(timeval.tv_usec/1000);
	return (uint32_t)(timeNow - epochMilli) ;
}

/*
* irq_handler - Interrupt request handler for GPIO pin
*
* This feature is pretty experiment, so more work needs to
* be done to make the feature useful for application
*/

//---------------------------------------------------------/*INT HANDLER*/-----------------------------------------
static irqreturn_t irq_handler(int irq, void *arg) 
{
	unsigned long flags;
	unsigned int interrupt_time = millis();
	if (interrupt_time - last_interrupt_time < 200)
	{
		printk(KERN_NOTICE "Ignored Interrupt [%d]\n", irq);
		return IRQ_HANDLED;
	}
	last_interrupt_time = interrupt_time;
	local_irq_save(flags);
	printk(KERN_NOTICE "Interrupt [%d] was triggered\n", irq);
	local_irq_restore(flags);
	return IRQ_HANDLED;
}

/*
* bbb_gpio_open 
-
Open GPIO device node in /dev
*
* This function allocates GPIO interrupt resource when requested
* on the condition that interrupt flag is enabled and pin direction
* set to input, then allow the
specified GPIO pin to set interrupt.
*/

//-----------------------------------------------------------/*OPEN METHOD*/-----------------------------------------

static int bbb_gpio_open (struct inode *inode, struct file *filp) 
{
	struct bbb_gpio_dev *bbb_gpio_devp;
	unsigned int gpio;
	int err, irq;
	unsigned long flags;
	gpio = iminor(inode);
	printk(KERN_INFO "GPIO[%d] opened\n", gpio);
	bbb_gpio_devp = container_of(inode->i_cdev, struct bbb_gpio_dev,cdev);
	filp->private_data = bbb_gpio_devp;
	if ((bbb_gpio_devp->irq_perm == true) && (bbb_gpio_devp->dir == in)) 
	{
		if ((bbb_gpio_devp->irq_counter++ == 0)) 
		{
			irq = gpio_to_irq(gpio);
			if (bbb_gpio_devp->irq_flag == IRQF_TRIGGER_RISING)
			{
				spin_lock_irqsave(&bbb_gpio_devp->lock, flags);
				err = request_irq(irq,irq_handler,IRQF_SHARED | IRQF_TRIGGER_RISING,
							INTERRUPT_DEVICE_NAME,bbb_gpio_devp);
				printk(KERN_INFO "interrupt requested\n");
				spin_unlock_irqrestore(&bbb_gpio_devp->lock, flags);
			} 
			else 
			{
				spin_lock_irqsave(&bbb_gpio_devp->lock, flags);
				err = request_irq(irq,irq_handler,IRQF_SHARED | IRQF_TRIGGER_FALLING,
							INTERRUPT_DEVICE_NAME,bbb_gpio_devp);
				printk(KERN_INFO "interrupt requested\n");
				spin_unlock_irqrestore(&bbb_gpio_devp->lock, flags);
			}
			if (err != 0)
			{
				printk(KERN_ERR "unable to claim irq: %d, error %d\n", irq,err);
				return err;
			}
		}
	}

return 0;
}

//---------------------------------------------------------------------RELEASE METHOD--------------------------------------
/*
* bbb_gpio_release - Release GPIO pin
*
* This functions releases GPIO interrupt resource when the device is  
last closed. When requested to disable interrupt, it releases GPIO interrupt 
resource regardless of how many devices are using interrupt.
*/

static int bbb_gpio_release (struct inode *inode, struct file *filp)
{
	unsigned int gpio;
	struct bbb_gpio_dev *bbb_gpio_devp;
	bbb_gpio_devp = container_of(inode->i_cdev, struct bbb_gpio_dev,cdev);
	gpio = iminor(inode);
	printk(KERN_INFO "Closing GPIO %d\n", gpio);
	spin_lock(&bbb_gpio_devp->lock);
	if (bbb_gpio_devp->irq_perm == true) 
	{
		if (bbb_gpio_devp->irq_counter > 0) 
		{
			bbb_gpio_devp->irq_counter--;
			if (bbb_gpio_devp->irq_counter == 0)
			{
				printk(KERN_INFO "interrupt on gpio[%d] released\n", gpio);
				free_irq(gpio_to_irq(gpio), bbb_gpio_devp);
			}
		}
	}
	spin_unlock(&bbb_gpio_devp->lock);
	if (bbb_gpio_devp->irq_perm == false && bbb_gpio_devp->irq_counter > 0)
	{
		spin_lock(&bbb_gpio_devp->lock);
		free_irq(gpio_to_irq(gpio), bbb_gpio_devp);
		bbb_gpio_devp->irq_counter = 0;
		spin_unlock(&bbb_gpio_devp->lock);
		printk(KERN_INFO "interrupt on gpio[%d] disabled\n", gpio);
	}
return 0;
}

//----------------------------------------------------------------------------READ METHOD--------------------------------
/*
* bbb_gpio_read -
Read the state of GPIO pins
*
* This functions allows to read the logic state of input GPIO pins and
output GPIO pins. Since it multiple processes can read the logic state 
of the GPIO, spin lock is not used here.
*/

static ssize_t bbb_gpio_read (struct file *filp,char *buf,size_t count,loff_t *f_pos)
{
	unsigned int gpio;
	ssize_t retval;
	char byte;
	gpio = iminor(filp->f_path.dentry->d_inode);
	for (retval = 0; retval < count; ++retval) 
	{
		byte = '0' + gpio_get_value(gpio);
		if(put_user(byte, buf+retval))
		break;
	}
	return retval;
}

/*
* bbb_gpio_write -Write to GPIO pin
*
* This function allows to set GPIO pin direction (input/out),to
set GPIO pin logic level (high/low), and to enable/disable edge-
triggered interrupt on a GPIO pin (rising/falling)
*Set logic level (high/low) to an input GPIO pin is not permitted
* The command set for setting GPIO pins is as follows
* Command Description
* "out">> Set GPIO direction to output via gpio_direction_ouput
* "in">> Set GPIO direction to input via gpio_direction_input
* "1">> Set GPIO pin logic level to high
* "0">> Set GPIO pin logic level to low
* "rising">> Enable rising edge trigger
* "falling">> Enable falling edge trigger
* "disable-irq">> Disable interrupt on a GPIO pin
*/
//---------------------------------------------------------------------------WRITE METHOD-------------------------------

static ssize_t bbb_gpio_write (struct file *filp,const char *buf,size_t count,loff_t *f_pos)
{
	unsigned int gpio, len = 0, value = 0;
	char kbuf[BUF_SIZE];
	struct bbb_gpio_dev *bbb_gpio_devp = filp->private_data;
	unsigned long flags;
	gpio = iminor(filp->f_path.dentry->d_inode);
	len = count < BUF_SIZE ? count-1 : BUF_SIZE-1;
	if(copy_from_user(kbuf, buf, len) != 0)
	return -EFAULT;
	kbuf[len] = '\0';
	printk(KERN_INFO "Request from user: %s\n", kbuf);

// Check the content of kbuf and set GPIO pin accordingly
	if (strcmp(kbuf, "out") == 0) 
	{
		printk(KERN_ALERT "gpio[%d] direction set to ouput\n", gpio);
		if (bbb_gpio_devp->dir != out) 
		{
			spin_lock_irqsave(&bbb_gpio_devp->lock, flags);
			gpio_direction_output(gpio, low);
			bbb_gpio_devp->dir = out;
			bbb_gpio_devp->state = low;
			spin_unlock_irqrestore(&bbb_gpio_devp->lock, flags);
		}
	} 
	else if (strcmp(kbuf, "in") == 0)
	{
		if (bbb_gpio_devp->dir != in)
		{
			printk(KERN_INFO "Set gpio[%d] direction: input\n", gpio);
			spin_lock_irqsave(&bbb_gpio_devp->lock, flags);
			gpio_direction_input(gpio);
			bbb_gpio_devp->dir = in;
			spin_unlock_irqrestore(&bbb_gpio_devp->lock, flags);
		}
	}
	else if ((strcmp(kbuf, "1") == 0) || (strcmp(kbuf, "0") == 0))
	{
		sscanf(kbuf, "%d", &value);
		if (bbb_gpio_devp->dir == in) 
		{
			printk("Cannot set GPIO %d, direction: input\n", gpio);
			return -EPERM;
		}
		if (bbb_gpio_devp->dir == out)
		{
			if (value > 0) 
			{
				spin_lock_irqsave(&bbb_gpio_devp->lock, flags);
				gpio_set_value(gpio, high);
				bbb_gpio_devp->state = high;
				spin_unlock_irqrestore(&bbb_gpio_devp->lock, flags);
			} 
			else 
			{
				spin_lock_irqsave(&bbb_gpio_devp->lock, flags);
				gpio_set_value(gpio, low);
				bbb_gpio_devp->state = low;
				spin_unlock_irqrestore(&bbb_gpio_devp->lock, flags);
			}
		}
	} 
	else if ((strcmp(kbuf, "rising") == 0) || (strcmp(kbuf, "falling")== 0))
	{
		spin_lock_irqsave(&bbb_gpio_devp->lock, flags);
		gpio_direction_input(gpio);
		bbb_gpio_devp->dir = in;
		bbb_gpio_devp->irq_perm = true;
		if (strcmp(kbuf, "rising") == 0)
		bbb_gpio_devp->irq_flag = IRQF_TRIGGER_RISING;
		else
		bbb_gpio_devp->irq_flag = IRQF_TRIGGER_FALLING;
		spin_unlock_irqrestore(&bbb_gpio_devp->lock, flags);
	} 
	else if (strcmp(kbuf, "disable-irq") == 0)
	{
		spin_lock_irqsave(&bbb_gpio_devp->lock, flags);
		bbb_gpio_devp->irq_perm = false;
		spin_unlock_irqrestore(&bbb_gpio_devp->lock, flags);
	} 
	else 
	{
		printk(KERN_ERR "Invalid value\n");
		return -EINVAL;
	}	
	*f_pos += count;
	return count;
}


/*
* bbb_gpio_init -Initialize GPIO device driver
*
* This function performs the following tasks:
* Dynamically register a character device major
* Create "bbb-gpio" class
* Claim GPIO resource
* Initialize the per-device data structure bbb_gpio_dev
* Initialize spin lock used for synchronization
* Register character device to the kernel
* Create device nodes to expose GPIO resource
*/

//---------------------------------------------------------------------------INIT METHOD-------------------------------

static int __init bbb_gpio_init(void)
{
	int i, ret, index = 0;
	struct timeval tv ;
	if (alloc_chrdev_region(&first, 0, NUM_GPIO_PINS, DEVICE_NAME) < 0) 
	{
		printk(KERN_DEBUG "Cannot register device\n");
		return -1;
	}
	if ((bbb_gpio_class = class_create(THIS_MODULE, DEVICE_NAME)) == NULL)
	{
		printk(KERN_DEBUG "Cannot create class %s\n", DEVICE_NAME);
		unregister_chrdev_region(first, NUM_GPIO_PINS);
		return -EINVAL;
	}
	for (i = 0; i < MAX_GPIO_NUMBER; i++) 
	{
		/*if (i==20 || i==26 || i==27 || i==44 || i==45 || i==46 || i==47 || i==48 ||i==49 || i==60 || i==65 || i==66 ||i==67 || i==68 || i==69 || i==81 ||i==112 || i==115 || i==117)*/
		if(i==49 || i==48) 
		{
			bbb_gpio_devp[index] = kmalloc(sizeof(struct bbb_gpio_dev),GFP_KERNEL);
			if (!bbb_gpio_devp[index]) 
			{
				printk("error in allocating memory\n");
				return -ENOMEM;
			}
			if (gpio_request_one(i, GPIOF_OUT_INIT_LOW, NULL) < 0)
			{
				printk(KERN_ALERT "Error requesting GPIO %d\n", i);
				return -ENODEV;
			}
			bbb_gpio_devp[index]->dir = out;
			bbb_gpio_devp[index]->state = low;
			bbb_gpio_devp[index]->irq_perm = false;
			bbb_gpio_devp[index]->irq_flag = IRQF_TRIGGER_RISING;
			bbb_gpio_devp[index]->irq_counter = 0;
			bbb_gpio_devp[index]->cdev.owner = THIS_MODULE;
			spin_lock_init(&bbb_gpio_devp[index]->lock);
			cdev_init(&bbb_gpio_devp[index]->cdev, &bbb_gpio_fops);
			if ((ret = cdev_add(&bbb_gpio_devp[index]->cdev, (first + i), 1)))
			{
				printk (KERN_ALERT "Error %d adding cdev\n", ret);
				for (i = 0; i < MAX_GPIO_NUMBER; i++)
				{
					/*if (i==20 || i==26 || i==27 || i==44 || i==45 || i==46 || i==47 || i==48 ||i==49 || i==60 || i==65 || i==66 ||i==67 || i==68 || i==69 || i==81 ||i==112 || i==115 || i==117)*/
					if(i==49 || i==48)
					{
						device_destroy (bbb_gpio_class,MKDEV(MAJOR(first),MINOR(first) + i));
					}

				}
				class_destroy(bbb_gpio_class);
				unregister_chrdev_region(first, NUM_GPIO_PINS);
				return ret;
			}
			if (device_create(bbb_gpio_class,NULL,MKDEV(MAJOR(first), MINOR(first)+i),NULL,"bbbGpio%d",i) == NULL)
			{
				class_destroy(bbb_gpio_class);
				unregister_chrdev_region(first, NUM_GPIO_PINS);
				return -1;
			}
			index++;
		}
	}
// Configure interrupt
	do_gettimeofday(&tv) ;
	epochMilli =(uint64_t)tv.tv_sec *(uint64_t)1000 +(uint64_t)(tv.tv_usec/1000);
	printk("BBB GPIO driver initialized\n");
	return 0;
}


/*
* bbb_gpio_exit -Clean up GPIO device driver when unloaded
*
* This functions performs the following tasks:
* Release major number
* Release device nodes in /dev
* Release per-device structure arrays
* Detroy class in /sys
* Set all GPIO pins to output, low level
*/
//---------------------------------------------------------------------------EXIT METHOD-------------------------------

static void __exit bbb_gpio_exit(void)
{
	int i = 0;
	unregister_chrdev_region(first, NUM_GPIO_PINS);
	for(i = 0; i < NUM_GPIO_PINS; i++)
	kfree(bbb_gpio_devp[i]);
	for (i = 0; i < MAX_GPIO_NUMBER; i++)
	{
		/*if (i==20 || i==26 || i==27 || i==44 || 
		    i==45 || i==46 || i==47 || i==48 ||
		    i==49 || i==60 || i==65 || i==66 ||
		    i==67 || i==68 || i==69 || i==81 ||
		    i==112 || i==115 || i==117)*/
		if(i==49 || i==48)
		{
			gpio_direction_output(i, 0);
			device_destroy (bbb_gpio_class,MKDEV(MAJOR(first), MINOR(first) + i));gpio_free(i);
		}
	}
	class_destroy(bbb_gpio_class);
	printk(KERN_INFO "BBB GPIO driver removed\n");
}

//-----------------------------------------------------------------------------------------------------------------------


module_init(bbb_gpio_init);
module_exit(bbb_gpio_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group 19");
MODULE_DESCRIPTION("GPIO device driver for BBB platform");    

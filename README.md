# Design of the GPIO Device Driver

_Since the per-device structure (driver-specific data structure) is the information reposi-tory which the GPIO character device driver instantiates and revolves, it was designed first._

**The structure consists of the following data elements:** 


• A cdev kernel abstraction element for character device drivers

• An instance of struct gpio structure describing a GPIO pin with configuration

• A variable indicating the state of a GPIO pin (low or high) 

• A variable indicating the direction of a GPIO pin (input or output) 

• A boolean variable used to enable or disable interrupt on a GPIO pin 

• A flag indicating rising or falling edge trigger on a GPIO pin 

• A counter for keeping track of the number of requests for interrupt 

• A spinlock used for synchronization. 

**The per-device data structure plays the role of representing a GPIO pin on BBB.**


_After the per-device structure had been created, the design of driver initialization routine and driver exit routine took place.
The driver initialization routine is the one which is called first when the kernel installs the GPIO device driver. The driver exit 
routine, in contrast, is the last one called by the kernel when being unloaded from the system._
 
 _In this thesis report, device driver and kernel module will be used interchangeably. Be-cause the driver
 initialization routine is the bedrock for registering the GPIO character device driver to the kernel,_
 
 **it will be responsible for doing the following steps:**
 
 
 • Register a range of character device numbers 
 
 • Create a device class in /sys directory 
 
 • Claim GPIO pin resources 
 
 • Allocate memory for the per-device structure 
 
 • Initialize a spinlock to be used for synchronization
 
 • Register character device to the kernel 
 
 • Create device nodes to expose GPIO resources to userspace 
 
 • Get current timestamp used for contact debouncing. 

:+1: ----------------------------------------------------------------------------------------------------------------------------------

# OutPut Functionality :

The first test case was carried out to verify the output functionality offered by the GPIO device driver. An application program called output_test was run in the userspace to test the driver, and it has the following syntax: 
./output_test <logic_level>  

where <logic_level> refers to the logic state that is passed as an argument to the pro-gram for setting a GPIO pin low or high. If <logic_level> is “1”, the application will set the logic level of all GPIO pins high, whereas passing “0” as an argument to the application program will set all the logic level of all GPIO pins low.


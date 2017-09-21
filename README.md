Design of the GPIO Device Driver
Since the per-device structure (driver-specific data structure) is the information reposi-tory which the GPIO character device driver instantiates and revolves, it was designed first.

The structure consists of the following data elements: 


• A cdev kernel abstraction element for character device drivers

• An instance of struct gpio structure describing a GPIO pin with configuration

• A variable indicating the state of a GPIO pin (low or high) 

• A variable indicating the direction of a GPIO pin (input or output) 

• A boolean variable used to enable or disable interrupt on a GPIO pin 

• A flag indicating rising or falling edge trigger on a GPIO pin 

• A counter for keeping track of the number of requests for interrupt 

• A spinlock used for synchronization. 

The per-device data structure plays the role of representing a GPIO pin on BBB.


After the per-device structure had been created, the design of driver initialization routine and driver exit routine took place.
The driver initialization routine is the one which is called first when the kernel installs the GPIO device driver. The driver exit 
routine, in contrast, is the last one called by the kernel when being unloaded from the system.
 
 In this thesis report, device driver and kernel module will be used interchangeably. Be-cause the driver
 initialization routine is the bedrock for registering the GPIO character device driver to the kernel,
 
 it will be responsible for doing the following steps:
 
 
 • Register a range of character device numbers 
 
 • Create a device class in /sys directory 
 
 • Claim GPIO pin resources 
 
 • Allocate memory for the per-device structure 
 
 • Initialize a spinlock to be used for synchronization
 
 • Register character device to the kernel 
 
 • Create device nodes to expose GPIO resources to userspace 
 
 • Get current timestamp used for contact debouncing. 


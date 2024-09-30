### BUILD ###
make <- compiling module and test  
make clean <- cleaning module output files and test

### scull.ko ###
Module can be loaded with below arguments:  
major - major number, if greather than 0 static chrdev region allocation will be used, if equals 0 dynamic chrdev region allocation will be used, by default 0  
minor_base - defines base of the minor number, by default 0  
devs_nb - number of device nodes, by default 4  
flag - if set to 1 device class and device creation is done duirng init of the module, by default 0  

When loading 'scull.ko' module directly without using 'scull_load' script then flag must be set to 1.  

### test_scull ###
User space program in C for 'scull' device testing purpose.

### scull_load ###
Script loads 'scull.ko' module and it creates 'scull[0-3]' /dev nodes.  
In order to create different number of device nodes script must be edited.  
Script passes all received arguments to the inserting module.  
Executing './scull_load major=x' ,where x is greather than 0 will cause static  
allocation of chrdev region. When 'major' argument is not present or is equal  
to 0 then dynamic allocation of chrdev region is invoked. 


### scull_unload ###
Script calls rmmod for 'scull' module and removes 'scull[0-3]' device nodes  
form /dev. When number of nodes was manyally changed by editing scull_load  
script then scull_unload shall be also modified with appropriate number of  
nodes to remove.
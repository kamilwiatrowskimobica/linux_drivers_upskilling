# linux_drivers_upskilling

## Sync methods summary
In Linux driver development, there are several synchronization mechanisms available to synchronize access to shared resources between different parts of a driver or between a driver and user space. Here are the differences between some of the commonly used mechanisms and when to use each, along with some real-life examples:

### Workqueue
A workqueue is a mechanism that allows a driver to schedule work to be performed in the background, asynchronously. Workqueues are often used when a driver needs to perform a non-blocking operation, such as processing data received from a hardware device or performing some other computation in the background. 

Workqueue: Workqueues are typically used when a driver needs to perform a non-blocking operation in the background, such as processing data received from a hardware device or performing some other computation that does not require immediate attention. 

Example: A network driver may use a workqueue to process incoming packets asynchronously in the background. The workqueue can be used to perform any processing needed on the received data, such as parsing and decoding the packet, and storing it in a buffer for further processing.

### Tasklets
Tasklets are a type of workqueue that allows a driver to perform deferred work in interrupt context. Tasklets are often used when a driver needs to perform a lightweight, non-blocking operation in response to an interrupt. 

Tasklets: Tasklets are typically used for short, time-critical operations that need to be performed in the context of an interrupt handler or kernel thread. 

Example: A network driver may use a tasklet to handle interrupts that occur when a packet is received. The tasklet can be used to perform any processing needed on the received data, such as checking for errors, before passing it on to the higher-level network stack.

### Wait Queue
A wait queue is a mechanism that allows a process or thread to sleep until a certain condition is met, such as the availability of a resource or the completion of an operation. Wait queues are often used to synchronize access to shared resources between different parts of a driver or between a driver and user space. 

Wait Queue: Wait queues are typically used when a process or thread needs to wait for a resource to become available or for an event to occur, such as the completion of an I/O operation. 

Example: A storage driver may use a wait queue to wait for a pending I/O request to complete. The driver can put the calling process to sleep on a wait queue until the I/O request is completed, at which point the process can be woken up and the result of the request returned.

### Completion
A completion is a mechanism that allows a process or thread to wait for the completion of an operation, such as the completion of an I/O request. Completions are often used to synchronize access to shared resources between different parts of a driver. 

Completion: Completions are typically used when a part of a driver needs to wait for an operation to complete before proceeding. 

Example: A video driver may use a completion to wait for a frame to be displayed on the screen before continuing to render the next frame. The driver can use a completion to block until the previous frame has been displayed, at which point it can continue rendering the next frame.

### Polling
Polling is a mechanism that allows a driver to check for the availability of a resource or the completion of an operation periodically, without blocking. Polling is often used when a driver needs to perform a non-blocking operation and does not need to wait for a specific event or condition. 

Polling: Polling is typically used when a driver needs to periodically check for the availability of a resource or the completion of an operation without blocking.

Example: A network driver may use polling to check for the availability of data to send periodically. The driver can poll the device periodically to check for available data, and if data is available, it can be sent to the network stack.

### Spinlocks
A spinlock is a mechanism that allows a driver to acquire a lock on a shared resource and spin in a loop, waiting for the lock to become available. Spinlocks are often used when the time spent waiting for a lock to become available is expected to be short. 
Note: 
* "The kernel preemption case is handled by the spinlock code itself. Any time kernel code holds a spinlock, preemption is disabled on the relevant processor. Even uniprocessor systems must disable preemption in this way to avoid race conditions." - LDD book 

Spinlocks: Spinlocks are typically used when the time spent waiting for a lock to become available is expected to be short.

Example: A network driver may use a spinlock to protect access to a shared data structure used to track pending packets. When a packet is received, the driver can acquire the spinlock to update the data structure, and release the lock when the update is complete.

### Semaphores
A semaphore is a mechanism that allows a driver to control access to a shared resource by limiting the number of processes or threads that can access the resource at the same time. Semaphores are often used when a driver needs to limit the concurrency of access to a shared resource. 

Semaphores: Semaphores are typically used when a driver needs to limit the concurrency of access to a shared resource.

Example: A driver for a network interface card might use a semaphore to limit the number of processes that can transmit packets simultaneously.

### Mutexes
A mutex is a mechanism that allows a driver to acquire a lock on a shared resource and block other processes or threads from accessing the resource until the lock is released. Mutexes are often used when the time spent waiting for a lock to become available is expected to be longer than with spinlocks. 
Note:
* Linux kernel provides a variant of mutexes called "adaptive mutexes", which are preemptive. Adaptive mutexes automatically detect when a thread holding a mutex is running for an extended period of time and voluntarily relinquish the mutex to allow other threads to run.
* There is a variant of the mutex called the PI (Priority Inheritance) mutex, which implements priority inheritance.
  
 Mutexes: Mutexes are typically used when the time spent waiting for a lock to become available is expected to be longer than with spinlocks.
  
Example: A driver for a device that has a shared buffer might use a mutex to protect access to the buffer.

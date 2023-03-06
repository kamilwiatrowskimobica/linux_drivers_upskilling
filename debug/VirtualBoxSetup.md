Virtualbox needs adjustments

Serial port forwarding need to be enabled:
	regarding guest:
			seettings -> Serial Ports -> Port 1 -> Enable Serial Port, COM1, IRQ 4, I/O Port: 0x3f8, Port MOde: Host Pipe, Path/Address: \\.\pipe\mtlepipe
	regarding host:
			seettings -> Serial Ports -> Port 1 -> Enable Serial Port, COM1, IRQ 4, I/O Port: 0x3f8, Port MOde: Host Pipe, Connetct to existing pipe/socket, Path/Address: \\.\pipe\mtlepipe

	Test above settings:
    	in host:
    		cat /dev/ttyS0
    	in guest:
    		printf "test string\r" >> /dev/ttyS0
            # this should print "test string" in the host

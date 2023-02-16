
ON TARGET:

vim /etc/default/grub
    GRUB_CMDLINE_LINUX="nomodeset rhgb debug nokaslr kgdboc=/dev/ttyS0,115200 kgdbwait"
    instead of
    GRUB_CMDLINE_LINUX="nomodeset rhgb quiet"
    # the 'debug' option makes it easier to debug system booting (prints extended info to tty and syslog)


grub2-mkconfig -o /etc/grub2.cfg


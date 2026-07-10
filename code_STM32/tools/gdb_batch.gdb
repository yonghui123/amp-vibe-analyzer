target extended-remote 127.0.0.1:5000
monitor reset halt
load
break main
break HardFault_Handler
break StartGUITask
continue
echo \n=== STOPPED ===\n
info line
bt 8
quit

target extended-remote 127.0.0.1:5000
monitor reset halt
break HardFault_Handler
break StartGUITask
break GUI_Init
continue
echo \n=== AFTER CONTINUE ===\n
info line
bt 10
quit

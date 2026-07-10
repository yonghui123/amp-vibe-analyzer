target extended-remote 127.0.0.1:5000
monitor reset halt
load
break HardFault_Handler
break StartGUITask
break GUI_Init
commands 3
  echo \n=== GUI_Init reached ===\n
  continue
end
continue
echo \n=== STOP ===\n
info line
bt 10
quit

G1000 HID unit 

This is a hardware simulator for the G1000, the most popular glass cockpit system in the GA fleet.  

The 3D model is based on the original device, has accurate dimensions and is available in STP format.  

Schematics and PCB layout are in the Eagle folder.  
All inputs are read by three ATMega16 MCU with USB firmware, linked together on a FE1.1s USB Hub chip.
No BOM is provided, as most parts are generic. The encoders are Alps STEC (volume) and ELMA E33 / E37 (all others)
The joystick is an Alps RKJXT1F42001, which is soldered on the copper side.

Sourcecode is mainly based on the V-USB driver firmware from obdev/v-usb


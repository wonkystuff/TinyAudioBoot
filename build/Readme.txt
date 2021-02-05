How to flash example for AVR-Dragon:
Flash the file:
avrdude -v -pt85 -c dragon_isp -Pusb -b115200 -Uflash:w:AudioBootAttiny_AudioPB3_PB1.hex 

set the fuses with reset enabled:
avrdude -v -pt85 -c dragon_isp -Pusb -b115200 -U efuse:w:0xfe:m -U hfuse:w:0xdd:m -U lfuse:w:0xe1:m





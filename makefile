arduino_os: 
	avr-gcc -mmcu=atmega328p -DF_CPU=16000000 -O2 -o main.elf main.c os.c serial.c syncro.c SdReader.c ext.c
	avr-objcopy -O ihex main.elf main.hex
	avr-size main.elf

#Flash the Arduino
#Be sure to change the device (the argument after -P) to match the device on your computer
#On Windows, change the argument after -P to appropriate COM port
program: main.hex
	avrdude -pm328p -P /dev/tty.usbmodemfd121 -c arduino -F -u -U flash:w:main.hex
	screen /dev/tty.usbmodemfd121 115200

#remove build files
clean:
	rm -fr *.elf *.hex *.o
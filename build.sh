
rm avrgcc/throwie2.*
avr-gcc -mmcu=attiny5 -Wall -Os -o avrgcc/throwie2.elf main.c
avr-objcopy -j .text -j .data -O ihex avrgcc/throwie2.elf avrgcc/throwie2.hex

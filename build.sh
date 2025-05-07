rm avrgcc/throwie2.*

avr-gcc -mmcu=attiny5 \
    -Wl,--print-memory-usage -Wl,--gc-sections -Wl,--print-gc-sections \
    -fstack-usage -fdata-sections -ffunction-sections -flto \
    -Wall -Os -o avrgcc/throwie2.elf main.c

# print object sizes
nm -S --size-sort avrgcc/throwie2.elf

avr-objcopy -j .text -j .data -O ihex avrgcc/throwie2.elf avrgcc/throwie2.hex

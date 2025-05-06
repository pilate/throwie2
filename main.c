#include <avr/io.h>
#include <avr/interrupt.h>

#include "lib8tion/lib8tion.h"


#define BREATHE 1
// #define FLICKER 1
// #define SIREN 1
// #define MORSE 1 // Only works in microchip studio??

// Embed source link in hex
const uint8_t volatile pilate[] = "github.com/Pilate";

uint8_t led_color[3] = {0x00, 0x00, 0x00};

void update_led()
{
    /*
    8 MHz - 125ns (0.125us)

    Min code period time: 1.2us

    0 code timing:
      - 0.375us HIGH (3 cycles)
      - 0.875us LOW (7 cycles)
    1 code timing:
      - 0.625 HIGH (5 cycles)
      - 0.625 LOW (5 cycles)

    Interrupts should be disabled
    */

    asm volatile(
        "setup: "
        " cli \n"        // disable interrupts, timing has to be perfect

        " clr r25 \n"    // Set r25 to 0
        " ldi r24, 3 \n" // Set r24 to number of bytes to send
        " ldi r23, 4 \n" // Set r23 to 'PB2 on'

        "start: "
        " dec r24 \n"
        " brmi end \n"
        " ld r21, X+ \n" // Load next byte into r21
        " ldi r22, 8 \n" // set bit counter to 8

        "bitloop: "
        " out %[port], r23 \n" // Set pin to HIGH
        " lsl r21 \n"          // Shift the bit we're working on into C flag
        " brcs sendhigh \n"    // Stay high on 1 bit
        " out %[port], r25 \n" // Send 0 bit

        "sendhigh: "
        " dec r22 \n" // decrease bit counter

        "endlow: "
        " out %[port], r25 \n" // Shared LOW
        " breq start \n"       // if bit counter was 0, back to start
        " nop \n"
        " rjmp bitloop \n"

        "end:"
        :
        : [port] "m"(PORTB),
          [data] "x"(led_color)
        : "r21", "r22", "r23", "r24", "r25", "cc", "memory");
}

void nap(uint16_t nap_time)
{
    uint16_t timeout;
    uint8_t wdp;

    asm volatile("sei");

    SMCR = (1 << SM1) | // Sleep mode: power down
           (1 << SE);   // Sleep mode enable

    // doing wdp higher than 7 requires setting the wdp3 bit separately
    for (timeout = 2048, wdp = 7; timeout >= 16; timeout /= 2, wdp--)
    {
        CCP = 0xD8;
        WDTCSR = wdp | (1 << WDIE); // Watchdog interrupt enable

        while (nap_time >= timeout)
        {
            asm volatile("sleep");
            nap_time -= timeout;
        }
    }
}

// Watchdog
ISR(WDT_vect, ISR_NAKED)
{
    asm volatile("reti");
}

// ADC
ISR(ADC_vect, ISR_NAKED)
{
    asm volatile("reti");
}

// Claude *magic* RNG
uint8_t tiny_rand(void)
{
    static uint8_t lfsr = 1;

    uint8_t bit = lfsr & 1;
    lfsr >>= 1;
    if (bit)
    {
        lfsr ^= 0xB4;
    }
    return lfsr;
}

uint8_t adc_sample()
{
    PORTB = 1 << PB1; // turn on power to photoresitor

    ADCSRA = (1 << ADEN) |  // Enable ADC
             (1 << ADIE) |  // Enable completion interrupt
             (1 << ADPS2) | // Set prescaler to 64
             (1 << ADPS1);

    asm volatile("sei");
    SMCR = (1 << SM0) | // Sleep mode: ADC Noise reduction
           (1 << SE);   // Sleep enable

    asm volatile("sleep");

    uint8_t result = ADCL;

    PORTB = 0;
    ADCSRA = 0;

    return result;
}

#ifdef BREATHE

uint8_t rand_color[3] = {0x00, 0x00, 0x00};
uint8_t scale[4] = {0x00, 0x55, 0xaa, 0xff};

void dim(uint8_t divider)
{
    for (uint8_t i = 0; i < 3; i++)
    {
        led_color[i] = scale8(rand_color[i], divider);
    }
}

void effect()
{
    while (1)
    {
        uint8_t byte = tiny_rand();

        for (uint8_t i = 0; i < 3; i++)
        {
            rand_color[i] = scale[byte & 0b11];
            byte >>= 2;
        }

        while (adc_sample() < 100)
        {
            // nap(0xf000); // 60 seconds
            nap(10240);
        }

        uint8_t loops = 3;
        while (loops--)
        {
            uint8_t counter = 1;
            int8_t direction = 1;

            while (counter)
            {
                counter += direction;
                if (counter == 127)
                {
                    direction = -1;
                }

                dim(ease8InOutApprox(counter));
                update_led();
                nap(16);
            }

            nap(512);
        }
    }
}

#elif FLICKER

void effect()
{
    led_color[1] = 0xff;

    while (1)
    {
        while (adc_sample() < 100)
        {
            if (led_color[1])
            {
                led_color[1] = 0;
                update_led();
            }
            // nap(0xf000); // 60 seconds
            nap(10240);
        }

        // Don't want to hit the ADC every loop
        uint8_t counter = 0xff;
        while (counter--)
        {
            uint8_t rand_byte = tiny_rand();
            if ((rand_byte % 8) == 0)
            {
                led_color[1] = 0x60;
            }
            else
            {
                led_color[1] = 0x7f;
            }
            update_led();
            if ((rand_byte % 5) == 0)
            {
                nap(64);
            }
            nap(64);
        }
    }
}

#elif SIREN

void effect()
{
    while (1)
    {

        while (adc_sample() < 100)
        {
            if (led_color[2])
            {
                led_color[2] = 0x00;
                update_led();
            }
            // nap(0xf000); // 60 seconds
            nap(10240);
        }

        // Don't want to hit the ADC every loop
        uint8_t counter = 0xff;
        while (counter--)
        {
            led_color[2] = 0x00;
            led_color[1] = 0xff;
            update_led();
            nap(128);
            led_color[1] = 0x00;
            led_color[2] = 0xff;
            update_led();
            nap(128);
        }
    }
}

#elif MORSE

const char str[] = "TESTING";
const uint8_t str_len = sizeof(str) - 1;
const uint8_t unit_len = 128;

// Morse code mapping
// highest 3 bits are the length of the sequence
// lower bits are the sequence from low to high
// eg: A = 0b00100010, 010 = 2, 00010, from right to left: 0 = dot, 1 = dash
const char codes[43] = {
    // 0-9
    0b10111111,
    0b10111110,
    0b10111100,
    0b10111000,
    0b10110000,
    0b10100000,
    0b10100001,
    0b10100011,
    0b10100111,
    0b10101111,
    // :-@
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    // A-Z
    0b01000010,
    0b10000001,
    0b10000101,
    0b01100001,
    0b00100000,
    0b10000100,
    0b01100011,
    0b10000000,
    0b01000000,
    0b10001110,
    0b01100101,
    0b10000010,
    0b01000011,
    0b01000001,
    0b01100111,
    0b10000110,
    0b10001011,
    0b01100010,
    0b01100000,
    0b00100001,
    0b01100100,
    0b10001000,
    0b01100110,
    0b10001001,
    0b10001101,
    0b10000011,
};

void blink(uint8_t code)
{
    register uint8_t code_len = code >> 5;

    while (code_len--)
    {
        led_color[0] = 0xff;
        update_led();

        if (code & 1)
        {
            nap(unit_len * 3); // send dash
        }
        else
        {
            nap(unit_len); // send dit
        }

        led_color[0] = 0x00;
        update_led();

        nap(unit_len); // 1 unit between parts

        code >>= 1;
    }
    nap(unit_len * 3); // 3 units between letters
}

void effect()
{
    static uint8_t code;

    while (1)
    {
        if (adc_sample() > 100)
        {
            for (uint8_t i = 0; i < str_len; i++)
            {
                code = str[i];
                // 7 units for space
                if (code == 0x20)
                {
                    nap(unit_len * 7);
                }
                // number or letter
                else
                {
                    blink(codes[code - 0x30]);
                }
            }
        }
        nap(0xf000);
    }
}

#endif

int main(void)
{
    // disable protection
    CCP = 0xD8;
    // disable clock divider
    CLKPSR = 0;

    // PB0 used for photoresistor input
    DDRB = 0b11111110;
    PORTB = 0;

    // Digital input disable on ADC pins
    DIDR0 = (1 << ADC0D) | (1 << ADC1D);

    // Clear LED
    update_led();

    effect();
}
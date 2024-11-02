#include <avr/io.h>
#include <avr/interrupt.h>

#include "lib8tion/lib8tion.h"


// #define FLICKER 1
#define BREATHE 1

// Embed source link in hex
uint8_t pilate[] = "github.com/Pilate";


void __attribute__((noinline)) write_pixel(uint8_t data[])
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

    */

    asm volatile(
        "setup: "
        " cli \n" // disable interrupts

        "start: "
        " dec %[data_len] \n"
        " brmi end \n"
        " ld r16, X+ \n" // Load next byte into r16
        " ldi r17, 8 \n" // set bit counter to 8

        "bitloop: "
        " out %[port], %[pin] \n"  // Set pin to HIGH
        " lsl r16 \n"              // Shift the bit we're working on into C flag
        " brcs sendhigh \n"        // Stay high on 1 bit
        " out %[port], %[zero] \n" // Send 0 bit

        "sendhigh: "
        " dec r17 \n" // decrease bit counter

        "endlow: "
        " out %[port], %[zero] \n" // Shared LOW
        " breq start \n"           // if bit counter was 0, back to start
        " nop \n"
        " rjmp bitloop \n"

        "end:"
        " sei \n"
        :
        : [port] "m"(PORTB),
          [data] "x"(data),
          [data_len] "r"(3),
          [pin] "r"(1 << PB2),
          [zero] "r"(0)
        : "r16", "r17");
}

// Set up watchdog timer
void inline tn_wdt_setup(uint8_t wdp)
{
    // interrupt flag, interupt enable, change enable, enable
    wdp |= (1 << WDIF) | (1 << WDIE) | (1 << WDE);

    WDTCSR = wdp;
}

void __attribute__((noinline)) nap(uint16_t nap_time)
{
    uint16_t timeout;
    uint8_t wdp;

    for (timeout = 1024, wdp = 6; timeout >= 16; timeout /= 2, wdp--)
    {
        while (nap_time >= timeout)
        {
            tn_wdt_setup(wdp);
            asm volatile("sei");
            SMCR = (1 << SM1) | // Sleep mode: power down
                   (1 << SE);   // Sleep mode enable
            asm volatile("sleep");
            asm volatile("cli");
            nap_time -= timeout;
        }
    }
}

ISR(WDT_vect)
{
    WDTCSR = 0;
}

ISR(ADC_vect)
{
}

// int8_t volatile rand_tiny()
// {
//     static uint8_t seed = 3;

//     seed *= 13;
//     return seed;
// }


// Claude *magic*
uint8_t volatile rand_tiny(void)
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

uint8_t volatile adc_sample()
{
    PORTB = 1 << PB1; // turn on power to photoresitor

    ADCSRA = (1 << ADEN) |  // Enable ADC
             (1 << ADIE) |  // Enable completion interrupt
             (1 << ADPS2) | // Set prescaler to 64
             (1 << ADPS1);

    SMCR = (1 << SM0) | // Sleep mode: ADC Noise reduction
           (1 << SE);   // Sleep enable

    asm volatile("sleep");

    uint8_t result = ADCL;

    PORTB = 0;
    ADCSRA = 0;

    return result;
}

uint8_t base_color[3] = {0x00, 0x00, 0x00};
uint8_t show_color[3] = {0x00, 0x00, 0x00};

void volatile dim(uint8_t divider)
{
    for (uint8_t i = 0; i < 3; i++)
    {
        show_color[i] = scale8(base_color[i], divider);
    }
}

#ifdef BREATHE

void volatile effect()
{
    while (1)
    {
        // New random color
        for (uint8_t i = 0; i < 3; i++)
        {
            base_color[i] = rand_tiny();
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
            uint8_t up = 1;

            while (counter)
            {
                if (up)
                {
                    counter++;
                    if (counter == 127)
                    {
                        up = 0;
                    }
                }
                else
                {
                    counter--;
                }

                dim(ease8InOutApprox(counter));
                write_pixel(show_color);
                nap(16);
            }

            nap(512);
        }
    }
}

#elif FLICKER

void effect()
{
    base_color[1] = 0xff;

    while (1)
    {
        while (adc_sample() < 100)
        {
            // nap(0xf000); // 60 seconds
            nap(10240);
        }

        // Don't want to hit the ADC every loop
        uint8_t counter = 0xff;
        while (counter--) {
            uint8_t rand_byte = rand_tiny();
            if ((rand_byte % 8) == 0)
            {
                base_color[1] = 0x60;
            }
            else
            {
                base_color[1] = 0x7f;
            }
            write_pixel(base_color);
            if ((rand_byte % 5) == 0) {
                nap(64);
            }
            nap(64);
        }
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

    cli();

    // Clear pixel
    write_pixel(base_color);

    effect();
}
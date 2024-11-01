#include <avr/io.h>
#include <avr/interrupt.h>

#include "lib8tion/lib8tion.h"


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

int8_t rand_tiny()
{
    static uint8_t seed = 1;

    seed *= 13;
    return seed;
}

uint8_t adc_sample()
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

uint8_t base_colors[3] = {0x00, 0x00, 0x00};
uint8_t dim_colors[3] = {0x00, 0x00, 0x00};

void dim(uint8_t divider)
{
    for (uint8_t i = 0; i < 3; i++)
    {
        dim_colors[i] = scale8(base_colors[i], divider);
    }
}

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
    write_pixel(base_colors);

    while (1)
    {
        base_colors[0] = rand_tiny();
        base_colors[1] = rand_tiny();
        base_colors[2] = rand_tiny();

        uint8_t loops = 2;

        while (adc_sample() < 127)
        {
            nap(0xf000); // 60 seconds
        }

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
                write_pixel(dim_colors);
                nap(16);
            }

            nap(512);
        }
    }
}
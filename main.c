#include <avr/io.h>
#include <avr/interrupt.h>

#include "lib8tion/lib8tion.h"


#define BREATHE 1
// #define FLICKER 1
// #define SIREN 1

// Embed source link in hex
const uint8_t volatile pilate[] = "github.com/Pilate";

uint8_t led_color[3] = {0x00, 0x00, 0x00};


void volatile update_led()
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
        " sei \n"
        :
        : [port] "m"(PORTB),
          [data] "x"(led_color)
        : "r21", "r22", "r23", "r24", "r25");
}

// Set up watchdog timer
void inline tn_wdt_setup(uint8_t wdp)
{
    // interrupt flag, interupt enable, change enable, enable
    wdp |= (1 << WDIF) | (1 << WDIE) | (1 << WDE);

    WDTCSR = wdp;
}

void nap(uint16_t nap_time)
{
    uint16_t timeout;
    uint8_t wdp;

    asm volatile("sei");
    for (timeout = 1024, wdp = 6; timeout >= 16; timeout /= 2, wdp--)
    {
        while (nap_time >= timeout)
        {
            tn_wdt_setup(wdp);
            SMCR = (1 << SM1) | // Sleep mode: power down
                   (1 << SE);   // Sleep mode enable
            asm volatile("sleep");
            nap_time -= timeout;
        }
    }
    asm volatile("cli");
}

// Watchdog
ISR(WDT_vect, ISR_NAKED)
{
    asm volatile ("reti");
}

// ADC
ISR(ADC_vect, ISR_NAKED)
{
    asm volatile ("reti");
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
    asm volatile("cli");

    uint8_t result = ADCL;

    PORTB = 0;
    ADCSRA = 0;

    return result;
}

#ifdef BREATHE

uint8_t rand_color[3] = {0x00, 0x00, 0x00};

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
        // New random color
        for (uint8_t i = 0; i < 3; i++)
        {
            rand_color[i] = tiny_rand();
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
            if (led_color[2]) {
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

    // Clear LED
    update_led();

    effect();
}
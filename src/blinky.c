/*
 * blinky.c
 * 
 * LED blink test to validate STM32F103C8 chips. This test will find
 * remarked and cloned low-density devices with less than 20kB RAM,
 * and/or missing timer TIM4.
 * 
 * Tests are applied in the following order:
 *  1. If TIM4 is missing, the onboard LED (pin B12 or C13) will not light.
 *  2. If there is not at least 20kB SRAM, the onboard LED will remain light.
 *  3. If TIM4 and >=20kB SRAM are both present, the LED will blink at 2Hz.
 * 
 * As the LED blinks, a character is written to USART1 at 9600 baud (8n1).
 * 
 * Written & released by Keir Fraser <keir.xen@gmail.com>
 * 
 * This is free and unencumbered software released into the public domain.
 * See the file COPYING for more details, or visit <http://unlicense.org>.
 */

int EXC_reset(void) __attribute__((alias("main")));

void IRQ_30(void) __attribute__((alias("IRQ_tim4")));
#define IRQ_TIM4 30

#ifndef NDEBUG
/* Keep the linker happy. */
int printk(const char *format, ...) { return 0; }
#endif

static void IRQ_tim4(void)
{
    static bool_t x;

    /* Quiesce the IRQ source. */
    tim4->sr = 0;

    /* Blink the LED. */
    gpio_write_pin(gpiob, 12, x);
    gpio_write_pin(gpioc, 13, x);
    x ^= 1;

    /* Write to the serial line. */
    usart1->dr = '.';
}

/* Pseudorandom LFSR. */
static uint32_t srand = 0x87a2263c;
static uint32_t rand(void)
{
    uint32_t x = srand;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    srand = x;
    return x;
}

int main(void)
{
    /* Relocate DATA. Initialise BSS. */
    if (_sdat != _ldat)
        memcpy(_sdat, _ldat, _edat-_sdat);
    memset(_sbss, 0, _ebss-_sbss);

    stm32_init();

    /* Configure USART1: 9600,8n1. */
    rcc->apb2enr |= RCC_APB2ENR_USART1EN;
    gpio_configure_pin(gpioa, 9, AFO_pushpull(_10MHz));
    gpio_configure_pin(gpioa, 10, GPI_pull_up);
    usart1->brr = SYSCLK / 9600;
    usart1->cr1 = (USART_CR1_UE | USART_CR1_TE | USART_CR1_RE);

    /* Configure LED pin(s). LED is connected to VDD. */
    gpio_configure_pin(gpiob, 12, GPO_opendrain(_2MHz, HIGH));
    gpio_configure_pin(gpioc, 13, GPO_opendrain(_2MHz, HIGH));

    /* (Attempt to) Configure TIM4 to overflow at 2Hz. */
    tim4->psc = sysclk_us(100)-1;
    tim4->arr = 5000-1;
    tim4->dier = TIM_DIER_UIE;
    tim4->cr2 = 0;
    tim4->cr1 = TIM_CR1_URS | TIM_CR1_CEN;

    /* Enable TIM4 IRQ, to be triggered at 2Hz. */
    IRQx_set_prio(IRQ_TIM4, TIMER_IRQ_PRI);
    IRQx_clear_pending(IRQ_TIM4);
    IRQx_enable(IRQ_TIM4);

    /* Endlessly test SRAM by filling with pseudorandom junk and then 
     * testing the values read back okay. */
    for (;;) {
        uint32_t *p = (uint32_t *)_ebss, sr = srand;
        while (p < (uint32_t *)(0x20000000 + 20*1024))
            *p++ = rand();
        srand = sr;
        p = (uint32_t *)_ebss;
        while (p < (uint32_t *)(0x20000000 + 20*1024))
            if (*p++ != rand())
                goto ram_fail;
    }

ram_fail:
    /* On SRAM failure we light the LED(s) and hang. */
    IRQ_global_disable();
    gpio_write_pin(gpiob, 12, LOW);
    gpio_write_pin(gpioc, 13, LOW);
    for (;;) ;

    return 0;
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
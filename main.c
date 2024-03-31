/******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for Hello World Example using HAL APIs.
*
* Related Document: See README.md
*
*
*******************************************************************************
* Copyright 2022-2023, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "macros.h"


/*******************************************************************************
* Global Variables
*******************************************************************************/
bool timer_interrupt_flag = false;
bool pump_timer_interrupt_flag = false;
bool led_blink_active_flag = true;

volatile unsigned flow_count = 0;
cyhal_gpio_callback_data_t gpio_flow_pin_callback_data;
cyhal_gpio_callback_data_t gpio_temp_pin_callback_data;

extern volatile unsigned ringBuffer[RING_BUFFER_SIZE];


/* Variable for storing character read from terminal */
uint8_t uart_read_value;

/* Timer objects*/
cyhal_timer_t led_blink_timer;
cyhal_timer_t pump_timer;
cyhal_timer_t wire_timer;




/*******************************************************************************
* Function Prototypes
*******************************************************************************/
void timer_init(void);
void gpio_init(void);
static void isr_timer(void *callback_arg, cyhal_timer_event_t event);
static void isr_pump_timer(void *callback_arg, cyhal_timer_event_t event);
static void isr_wire_timer(void *callback_arg, cyhal_timer_event_t event);
static void isr_counter(void *callback_arg, cyhal_gpio_event_t event);
static void isr_wire(void *callback_arg, cyhal_gpio_event_t event);

/* Single channel initialization function*/
void adc_single_channel_init(void);

/* Function to read input voltage from channel 0 */
void adc_single_channel_process(void);

//Temp Sensor Functions
int initialize_wire(void);
void reset_pulse(void);
void listen(void);




/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function. It sets up a timer to trigger a periodic interrupt.
* The main while loop checks for the status of a flag set by the interrupt and
* toggles an LED at 1Hz to create an LED blinky. Will be achieving the 1Hz Blink
* rate based on the The LED_BLINK_TIMER_CLOCK_HZ and LED_BLINK_TIMER_PERIOD
* Macros,i.e. (LED_BLINK_TIMER_PERIOD + 1) / LED_BLINK_TIMER_CLOCK_HZ = X ,Here,
* X denotes the desired blink rate. The while loop also checks whether the
* 'Enter' key was pressed and stops/restarts LED blinking.
*
* Parameters:
*  none
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;

#if defined (CY_DEVICE_SECURE)
    cyhal_wdt_t wdt_obj;

    /* Clear watchdog timer so that it doesn't trigger a reset */
    result = cyhal_wdt_init(&wdt_obj, cyhal_wdt_get_max_timeout_ms());
    CY_ASSERT(CY_RSLT_SUCCESS == result);
    cyhal_wdt_free(&wdt_obj);
#endif /* #if defined (CY_DEVICE_SECURE) */

    /* Initialize the device and board peripherals */
    result = cybsp_init();

    /* Board init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();

    /* Initialize retarget-io to use the debug UART port */
    result = cy_retarget_io_init_fc(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX,
            CYBSP_DEBUG_UART_CTS,CYBSP_DEBUG_UART_RTS,CY_RETARGET_IO_BAUDRATE);

    /* retarget-io init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Initialize gpio pins */
    gpio_init();

    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");

    printf("****************** "
           "HAL: Hello World! Example "
           "****************** \r\n\n");

    printf("Hello World!!!\r\n\n");
    

    /* Initialize timer to toggle the LED */
    timer_init();

    /* Initialize Channel 0 */
    adc_single_channel_init();

    

    printf("Press 'Enter' key to pause or "
           "resume blinking the user LED \r\n\r\n");

    for (;;)
    {
        /* Check if 'Enter' key was pressed */
        if (cyhal_uart_getc(&cy_retarget_io_uart_obj, &uart_read_value, 1)
             == CY_RSLT_SUCCESS)
        {
            if (uart_read_value == '\r')
            {
                /* Pause LED blinking by stopping the timer */
                if (led_blink_active_flag)
                {
                    cyhal_timer_stop(&led_blink_timer);

                    printf("LED blinking paused \r\n");
                }
                else /* Resume LED blinking by starting the timer */
                {
                    cyhal_timer_start(&led_blink_timer);

                    printf("LED blinking resumed\r\n");
                }

                /* Move cursor to previous line */
                //printf("\x1b[1F");

                led_blink_active_flag ^= 1;

                //Start pump
                cyhal_gpio_toggle(DOSE_PUMP_A);
                //Start timer
                cyhal_timer_start(&pump_timer);

                //Start 1-Wire
                initialize_wire();

            }
        }
        /* Check if timer elapsed (interrupt fired) and toggle the LED */
        if (timer_interrupt_flag)
        {
            /* Clear the flag */
            timer_interrupt_flag = false;

            /* Invert the USER LED state */
            cyhal_gpio_toggle(CYBSP_USER_LED);

            // for (int i = 0; i < flow_count; i++){
            //     printf("1\r\n");
            // }
            // printf("0\r\n");
            // flow_count = 0;

            adc_single_channel_process();
            
        }

        if (pump_timer_interrupt_flag){
            cyhal_timer_stop(&pump_timer);
            pump_timer_interrupt_flag = false;
            printf("pump timer\r\n");
        }
    }
}

/*******************************************************************************
* Function Name: gpio_init
********************************************************************************
* Summary:
* This function initializes the gpio pins
*
* Parameters:
*  none
*
* Return :
*  void
*
*******************************************************************************/
void gpio_init(void){
    cy_rslt_t result;

    /* Initialize the User LED */
    result = cyhal_gpio_init(CYBSP_USER_LED, CYHAL_GPIO_DIR_OUTPUT,
                             CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);

    /* GPIO init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Initialize Dose pump A pin */
    result = cyhal_gpio_init(DOSE_PUMP_A, CYHAL_GPIO_DIR_OUTPUT,
                             CYHAL_GPIO_DRIVE_STRONG, 0);

    /* GPIO init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

     /* Initialize GPIO Flow Sensor Input */
    result = cyhal_gpio_init(FLOW_PIN, CYHAL_GPIO_DIR_INPUT,
                             CYHAL_GPIO_DRIVE_NONE, 0);

    /* GPIO init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

     /* Initialize GPIO Temperature Sensor Input/Output */
    result = cyhal_gpio_init(TEMP_PIN, CYHAL_GPIO_DIR_BIDIRECTIONAL,
                             CYHAL_GPIO_DRIVE_PULLUP, 1);

    /* GPIO init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }


    /*Configure Sensor Fet Pins */
    cyhal_gpio_init(PHFET, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, 0);
    cyhal_gpio_init(TDSFET, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, 0);

    /* Configure GPIO interrupt */
    gpio_flow_pin_callback_data.callback = isr_counter;
    gpio_temp_pin_callback_data.callback = isr_wire;
    
    cyhal_gpio_register_callback(FLOW_PIN, 
                                 &gpio_flow_pin_callback_data);
    cyhal_gpio_enable_event(FLOW_PIN, CYHAL_GPIO_IRQ_RISE, 
                                 7u, true);

    cyhal_gpio_register_callback(TEMP_PIN, 
                                 &gpio_temp_pin_callback_data);
    cyhal_gpio_enable_event(TEMP_PIN, CYHAL_GPIO_IRQ_FALL, 
                                 7u, true);                                 
}

/*******************************************************************************
* Function Name: timer_init
********************************************************************************
* Summary:
* This function creates and configures a Timer object. The timer ticks
* continuously and produces a periodic interrupt on every terminal count
* event. The period is defined by the 'period' and 'compare_value' of the
* timer configuration structure 'led_blink_timer_cfg'. Without any changes,
* this application is designed to produce an interrupt every 1 second.
*
* Parameters:
*  none
*
* Return :
*  void
*
*******************************************************************************/
 void timer_init(void)
 {
    cy_rslt_t result;

    const cyhal_timer_cfg_t led_blink_timer_cfg =
    {
        .compare_value = 0,                 /* Timer compare value, not used */
        .period = LED_BLINK_TIMER_PERIOD,   /* Defines the timer period */
        .direction = CYHAL_TIMER_DIR_UP,    /* Timer counts up */
        .is_compare = false,                /* Don't use compare mode */
        .is_continuous = true,              /* Run timer indefinitely */
        .value = 0                          /* Initial value of counter */
    };

    const cyhal_timer_cfg_t pump_timer_cfg =
    {
        .compare_value = 0,                 /* Timer compare value, not used */
        .period = PUMP_TIMER_PERIOD,   /* Defines the timer period */
        .direction = CYHAL_TIMER_DIR_UP,    /* Timer counts up */
        .is_compare = false,                /* Don't use compare mode */
        .is_continuous = true,              /* Run timer indefinitely */
        .value = 0                          /* Initial value of counter */
    };

    const cyhal_timer_cfg_t wire_timer_cfg =
    {
        .compare_value = 0,                 /* Timer compare value, not used */
        .period = WIRE_TIMER_PERIOD,   /* Defines the timer period */
        .direction = CYHAL_TIMER_DIR_UP,    /* Timer counts up */
        .is_compare = false,                /* Don't use compare mode */
        .is_continuous = false,              /* Run timer indefinitely */
        .value = 0                          /* Initial value of counter */
    };

    /* Initialize the timer object. Does not use input pin ('pin' is NC) and
     * does not use a pre-configured clock source ('clk' is NULL). */
    result = cyhal_timer_init(&led_blink_timer, NC, NULL);

    /* timer init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Initialize the timer object. Does not use input pin ('pin' is NC) and
     * does not use a pre-configured clock source ('clk' is NULL). */
    result = cyhal_timer_init(&pump_timer, NC, NULL);

    /* timer init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Initialize the timer object. Does not use input pin ('pin' is NC) and
     * does not use a pre-configured clock source ('clk' is NULL). */
    result = cyhal_timer_init(&wire_timer, NC, NULL);
    

    /* timer init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Configure timer period and operation mode such as count direction,
       duration */
    cyhal_timer_configure(&led_blink_timer, &led_blink_timer_cfg);
    cyhal_timer_configure(&pump_timer, &pump_timer_cfg);
    cyhal_timer_configure(&wire_timer, &wire_timer_cfg);

    /* Set the frequency of timer's clock source */
    cyhal_timer_set_frequency(&led_blink_timer, LED_BLINK_TIMER_CLOCK_HZ);
    cyhal_timer_set_frequency(&pump_timer, PUMP_TIMER_CLOCK_HZ);
    cyhal_timer_set_frequency(&wire_timer, WIRE_TIMER_CLOCK_HZ);
    
    /* Assign the ISR to execute on timer interrupt */
    cyhal_timer_register_callback(&led_blink_timer, isr_timer, NULL);
    cyhal_timer_register_callback(&pump_timer, isr_pump_timer, NULL);
    cyhal_timer_register_callback(&wire_timer, isr_wire_timer, NULL);

    /* Set the event on which timer interrupt occurs and enable it */
    cyhal_timer_enable_event(&led_blink_timer, CYHAL_TIMER_IRQ_TERMINAL_COUNT,
                              7, true);
    cyhal_timer_enable_event(&pump_timer, CYHAL_TIMER_IRQ_TERMINAL_COUNT,
                              7, true);
    cyhal_timer_enable_event(&wire_timer, CYHAL_TIMER_IRQ_TERMINAL_COUNT,
                              7, true);    

    /* Start the timer with the configured settings */
    cyhal_timer_start(&led_blink_timer);
 //   cyhal_timer_start(&pump_timer);
 }


/*******************************************************************************
* Function Name: isr_timer
********************************************************************************
* Summary:
* This is the interrupt handler function for the timer interrupt.
*
* Parameters:
*    callback_arg    Arguments passed to the interrupt callback
*    event            Timer/counter interrupt triggers
*
* Return:
*  void
*******************************************************************************/
static void isr_timer(void *callback_arg, cyhal_timer_event_t event)
{
    (void) callback_arg;
    (void) event;

    /* Set the interrupt flag and process it from the main while(1) loop */
    timer_interrupt_flag = true;

}

static void isr_pump_timer(void *callback_arg, cyhal_timer_event_t event)
{
    (void) callback_arg;
    (void) event;

    /* Set the interrupt flag and process it from the main while(1) loop */
    pump_timer_interrupt_flag = true;

}

static void isr_counter(void *callback_arg, cyhal_gpio_event_t event)
{
    (void) callback_arg;
    (void) event;

    flow_count++;
    
}

void isr_wire_timer(void *callback_arg, cyhal_timer_event_t event)
{
    (void) callback_arg;
    (void) event;

    /* Set the interrupt flag and process it from the main while(1) loop */
    cyhal_gpio_write(TEMP_PIN, 1);

    for (int i = 0; i < RING_BUFFER_SIZE; i++){
        printf("%u", ringBuffer[i]);
    }

    printf("\r\nWire Timer\r\n");
}


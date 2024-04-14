#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "macros.h"
#include "functions.h"
#include "publisher_task.h"


/* Timer objects*/
cyhal_timer_t led_blink_timer;
cyhal_timer_t pump_timer;
cyhal_timer_t wire_timer;
cyhal_timer_t write_timer;
cyhal_timer_t read_timer;

bool timer_interrupt_flag = false;
bool pump_timer_interrupt_flag = false;
bool led_blink_active_flag = true;
bool wire_initialized = false;
bool wire_busy = false;

extern int timerCount;
extern volatile unsigned flow_count;
extern volatile transaction_t transaction;
extern volatile unsigned ringBuffer[RING_BUFFER_SIZE];
extern unsigned volatile ringHead;
extern unsigned volatile ringTail;


// static void isr_timer(void *callback_arg, cyhal_timer_event_t event);
static void isr_pump_timer(void *callback_arg, cyhal_timer_event_t event);
static void isr_wire_timer(void *callback_arg, cyhal_timer_event_t event);
static void isr_write_timer(void *callback_arg, cyhal_timer_event_t event);
static void isr_read_timer(void *callback_arg, cyhal_timer_event_t event);
static void publish_timer(void *callback_arg, cyhal_timer_event_t event);

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
        .compare_value = 0,                 
        .period = WIRE_TIMER_PERIOD,   /* Defines the timer period */
        .direction = CYHAL_TIMER_DIR_UP,    /* Timer counts up */
        .is_compare = false,                /* Don't use compare mode */
        .is_continuous = false,              /* Run timer indefinitely */
        .value = 0                          /* Initial value of counter */
    };

        const cyhal_timer_cfg_t write_timer_cfg =
    {
        .compare_value = 0,                 /* Timer compare value */
        .period = WRITE_TIMER_PERIOD,       /* Defines the timer period */
        .direction = CYHAL_TIMER_DIR_UP,    /* Timer counts up */
        .is_compare = false,                /*  use compare mode */
        .is_continuous = false,              
        .value = 0                          /* Initial value of counter */
    };

        const cyhal_timer_cfg_t read_timer_cfg =
    {
        .compare_value =READ_TIMER_SLOT,                 /* Timer compare value */
        .period = READ_TIMER_PERIOD,       /* Defines the timer period */
        .direction = CYHAL_TIMER_DIR_UP,    /* Timer counts up */
        .is_compare = true,                /*  use compare mode */
        .is_continuous = false,              
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

    /* Initialize the timer object. Does not use input pin ('pin' is NC) and
     * does not use a pre-configured clock source ('clk' is NULL). */
    result = cyhal_timer_init(&write_timer, NC, NULL);
    

    /* timer init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

        /* Initialize the timer object. Does not use input pin ('pin' is NC) and
     * does not use a pre-configured clock source ('clk' is NULL). */
    result = cyhal_timer_init(&read_timer, NC, NULL);
    

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
    cyhal_timer_configure(&write_timer, &write_timer_cfg);
    cyhal_timer_configure(&read_timer, &read_timer_cfg);

    /* Set the frequency of timer's clock source */
    cyhal_timer_set_frequency(&led_blink_timer, LED_BLINK_TIMER_CLOCK_HZ);
    cyhal_timer_set_frequency(&pump_timer, PUMP_TIMER_CLOCK_HZ);
    cyhal_timer_set_frequency(&wire_timer, WIRE_TIMER_CLOCK_HZ);
    cyhal_timer_set_frequency(&write_timer, WRITE_TIMER_CLOCK_HZ);
    cyhal_timer_set_frequency(&read_timer, READ_TIMER_CLOCK_HZ);
    
    /* Assign the ISR to execute on timer interrupt */
    cyhal_timer_register_callback(&led_blink_timer, publish_timer, NULL);
    cyhal_timer_register_callback(&pump_timer, isr_pump_timer, NULL);
    cyhal_timer_register_callback(&wire_timer, isr_wire_timer, NULL);
    cyhal_timer_register_callback(&write_timer, isr_write_timer, NULL);
    cyhal_timer_register_callback(&read_timer, isr_read_timer, NULL);

    /* Set the event on which timer interrupt occurs and enable it */
    cyhal_timer_enable_event(&led_blink_timer, CYHAL_TIMER_IRQ_TERMINAL_COUNT, 7, true);
    cyhal_timer_enable_event(&pump_timer, CYHAL_TIMER_IRQ_TERMINAL_COUNT, 7, true);
    cyhal_timer_enable_event(&wire_timer, CYHAL_TIMER_IRQ_TERMINAL_COUNT, 7, true); 
    cyhal_timer_enable_event(&write_timer, CYHAL_TIMER_IRQ_ALL, 7, true);
    cyhal_timer_enable_event(&read_timer, CYHAL_TIMER_IRQ_ALL, 7, true); 

    /* Start the timer with the configured settings */
    // cyhal_timer_start(&led_blink_timer);
 //   cyhal_timer_start(&pump_timer);
 }

/******************************************************************************
 * Function Name: timer_init
 ******************************************************************************
 * Summary:
 *  Timer initialization, uses led_blinker_timer configuration
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *
 ******************************************************************************/


static void isr_pump_timer(void *callback_arg, cyhal_timer_event_t event)
{
    (void) callback_arg;
    (void) event;

    /* Set the interrupt flag and process it from the main while(1) loop */
    pump_timer_interrupt_flag = true;

}


void isr_wire_timer(void *callback_arg, cyhal_timer_event_t event)
{
    (void) callback_arg;
    (void) event;
    // cy_rslt_t result;
    
    cyhal_gpio_write(TEMP_PIN, 1);
    wire_busy = false;
    // if (transaction == RESET || wire_initialized){
    //     transaction++;
    // }
    
}

void isr_write_timer(void *callback_arg, cyhal_timer_event_t event)
{
    (void) callback_arg;
    (void) event;
    
    

    switch (event)
    {
    case CYHAL_TIMER_IRQ_CAPTURE_COMPARE:       //Read
        // cyhal_gpio_write(TEMP_PIN, 1);
        // wire_busy = false;
        break;
    case CYHAL_TIMER_IRQ_TERMINAL_COUNT:        //Write
        cyhal_gpio_write(TEMP_PIN, 1);
        wire_busy = false;
        // printf("%lu\r\n", cyhal_timer_read(&write_timer));
        break;
    default:
        // cyhal_gpio_toggle(TEMP_PIN);
        
        break;
    }

}

void isr_read_timer(void *callback_arg, cyhal_timer_event_t event)
{
    (void) callback_arg;
    (void) event;
    
    

    switch (event)
    {
    case CYHAL_TIMER_IRQ_CAPTURE_COMPARE:
        ringBuffer[ringHead++] = cyhal_gpio_read(TEMP_PIN);
        if (ringHead >= RING_BUFFER_SIZE){ringHead = 0;}
        break;

    case CYHAL_TIMER_IRQ_TERMINAL_COUNT:
        wire_busy = false;
        break;
        
    default:
        break;
    }
}

/******************************************************************************
 * Function Name: publish_timer
 ******************************************************************************
 * Summary:
 *  Timer ISR. Sends publish command to message queue and increment timerCount
 *  variable used to control EC and pH switching in publisher_task()
 *
 * Parameters:
 *  void *callback_arg : pointer to variable passed to the ISR (unused)
 *  cyhal_gpio_event_t event : GPIO event type (unused)
 *
 * Return:
 *  void
 *
 ******************************************************************************/

static void publish_timer(void *callback_arg, cyhal_timer_event_t event)
{
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	publisher_data_t publisher_q_data;

	/* To avoid compiler warnings */
	(void) callback_arg;
	(void) event;

	/* Assign the publish command to be sent to the publisher task. */
	// this is how the while loop knows which function to access
	publisher_q_data.cmd = PUBLISH_MQTT_MSG;
	timerCount++;

	/* Send the command and data to publisher task over the queue */
	xQueueSendFromISR(publisher_task_q, &publisher_q_data, &xHigherPriorityTaskWoken);
} // end of publish_timer function

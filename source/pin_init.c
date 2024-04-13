#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "functions.h"
#include "macros.h"

cyhal_gpio_callback_data_t gpio_flow_pin_callback_data;
cyhal_gpio_callback_data_t gpio_temp_pin_callback_data;

//GPIO 1-Wire ISR
static void isr_wire(void *callback_arg, cyhal_gpio_event_t event);

extern volatile transaction_t transaction;
extern bool wire_initialized;


void gpio_init()
{
	// GPIO INITIALIZATION
	cy_rslt_t result;
	result = cyhal_gpio_init(PH_FET, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, true);
	if(result != CY_RSLT_SUCCESS)
	{
	        printf("GPIO initialization failed. Error: %ld\n", (long unsigned int)result);
	        CY_ASSERT(0);
	}
	result = cyhal_gpio_init(EC_FET, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, false);
	if(result != CY_RSLT_SUCCESS)
	{
	    	printf("ADC initialization failed. Error: %ld\n", (long unsigned int)result);
	        CY_ASSERT(0);
	}
	result = cyhal_gpio_init(PUMP_ONE, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, false);
	if(result != CY_RSLT_SUCCESS)
	{
	    	printf("ADC initialization failed. Error: %ld\n", (long unsigned int)result);
	        CY_ASSERT(0);
	}

	    /* Initialize the User LED */
    result = cyhal_gpio_init(CYBSP_USER_LED, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);
    /* GPIO init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

     /* Initialize GPIO Flow Sensor Input */
    result = cyhal_gpio_init(FLOW_PIN, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_NONE, 0);
    /* GPIO init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

     /* Initialize GPIO Temperature Sensor Input/Output */
    result = cyhal_gpio_init(TEMP_PIN, CYHAL_GPIO_DIR_BIDIRECTIONAL, CYHAL_GPIO_DRIVE_PULLUP, 1);
    /* GPIO init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Configure GPIO interrupt */
    // gpio_flow_pin_callback_data.callback = isr_counter;
    gpio_temp_pin_callback_data.callback = isr_wire;
    
    // cyhal_gpio_register_callback(FLOW_PIN, &gpio_flow_pin_callback_data);
    // cyhal_gpio_enable_event(FLOW_PIN, CYHAL_GPIO_IRQ_RISE, 7u, true);

    cyhal_gpio_register_callback(TEMP_PIN, &gpio_temp_pin_callback_data);
    cyhal_gpio_enable_event(TEMP_PIN, CYHAL_GPIO_IRQ_BOTH, 7u, true);
}

//isr_wire
//gpio interrupt service routine for 1-wire temperature sensor pin.
void isr_wire(void *callback_arg, cyhal_gpio_event_t event)
{
    switch (event){
        case CYHAL_GPIO_IRQ_RISE:
            if (transaction == PRESENSE){
                // ringBuffer[1] = cyhal_timer_read(&wire_timer);
                // cyhal_gpio_enable_event(TEMP_PIN, CYHAL_GPIO_IRQ_BOTH, 7u, false);
            }
        break;
        case CYHAL_GPIO_IRQ_FALL:
            if (transaction == PRESENSE){
                wire_initialized = true;
            }
            // printf("Wire Fall\r\n");
        break;
            default:
        break;
    }
    (void) callback_arg;
    (void) event;
}
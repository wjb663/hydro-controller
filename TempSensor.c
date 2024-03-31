#include "macros.h"
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"


volatile unsigned ringBuffer[RING_BUFFER_SIZE];
unsigned ringHead = 0;
unsigned ringTail = 0;

extern cyhal_timer_t wire_timer;


int initialize_wire(void){
    cyhal_timer_start(&wire_timer);
    cyhal_gpio_write(TEMP_PIN, 0);
    return 0;
}


void reset_pulse(void){

}


void listen(void){

}

void isr_wire(void *callback_arg, cyhal_timer_event_t event)
{
    (void) callback_arg;
    (void) event;

    /* Set the interrupt flag and process it from the main while(1) loop */
    ringBuffer[ringHead++] = cyhal_gpio_read(TEMP_PIN);
    if (ringBuffer[ringTail++] == 0){
        printf("0\r\n");
    }
    else
    {
        printf("Wire Timer Read\r\n");
    }
}
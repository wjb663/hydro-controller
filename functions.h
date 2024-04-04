// #include "cybsp.h"
// #include "cyhal.h"


/*******************************************************************************
* Function Prototypes
*******************************************************************************/
void timer_init(void);
void gpio_init(void);


/* Single channel initialization function*/
void adc_single_channel_init(void);

/* Function to read input voltage from channel 0 */
void adc_single_channel_process(void);

//Temp Sensor Functions
int initialize_wire(void);
void wire_process(void);
void write_wire(uint8_t b);
void write_wire_byte(uint8_t data);
void read_wire(void);
void print_wire(void);

//GPIO 1-Wire ISR
//It was static, but is "connected"? inside main.c while defined inside tempsensor.c
void isr_wire(void *callback_arg, cyhal_gpio_event_t event);

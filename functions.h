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
void write_1(void);
void write_0(void);
void read(void);


#include "cybsp.h"
#include "cyhal.h"


#ifndef SOURCE_SENSOR_FUNCTIONS_H_
#define SOURCE_SENSOR_FUNCTIONS_H_

#define VPLUS_CHANNEL_0  (P10_0)
#define VPLUS_CHANNEL_1  (P10_3)
#define VREF_CHANNEL_1   (P10_5)

/* Number of scans every time ADC read is initiated */
#define NUM_SCAN                    (1)

/* Conversion factor */
#define MICRO_TO_MILLI_CONV_RATIO        (1000u)

/* Acquistion time in nanosecond */
#define ACQUISITION_TIME_NS              (1000u)

/* ADC Scan delay in millisecond */
#define ADC_SCAN_DELAY_MS                (200u)
#define NUM_READINGS					 (10)

/* Number of scans every time ADC read is initiated */
#define NUM_SCAN                         (1)

//Pin Macros
#define PH_FET                          (P11_4)
#define EC_FET                          (P12_3)
#define PUMP_ONE                        (P9_5)

void gpio_init();

void adc_multi_channel_init(void);
cy_rslt_t result_return(void);
int32_t channel0_return(void);
int32_t channel1_return(void);

#endif /* SOURCE_SENSOR_FUNCTIONS_H_ */

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
void timer_init(void);


/* Single channel initialization function*/
void adc_single_channel_init(void);

/* Function to read input voltage from channel 0 */
void adc_single_channel_process(void);

//Temp Sensor Functions
void initialize_wire(void);
void wire_process(void *pvParameters);
void write_wire_byte(uint8_t data);
void read_wire(void);
void print_wire(void);


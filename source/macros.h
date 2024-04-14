//Macros for program
#include "cyhal.h"


typedef enum transaction{
    RESET,
    PRESENSE,
    SKIP_ROM,
    CONVERT_T,
    POLL,
    SCRATCH,
    PARSE,
    DONE,
    ERROR,
} transaction_t;



/*******************************************************************************
* Macros
*******************************************************************************/
/* LED blink timer clock value in Hz  */
#define LED_BLINK_TIMER_CLOCK_HZ          (10000)

/* LED blink timer period value */
#define LED_BLINK_TIMER_PERIOD            (9999)

#define PUMP_TIMER_CLOCK_HZ               (10000)
#define PUMP_TIMER_PERIOD                 (9999)

#define WIRE_TIMER_CLOCK_HZ               (100000)
#define WIRE_TIMER_PERIOD                 (50)

#define WRITE_TIMER_CLOCK_HZ               (1000000)
#define WRITE_TIMER_PERIOD                 (100)  

#define READ_TIMER_CLOCK_HZ               (1000000)
#define READ_TIMER_PERIOD                 (15) 

#define WRITE_TIMER_SLOT                 (12)     

#define INIT_RETRIES                    (10)
#define REG_SIZE                        (16)
#define TEMP_CONVERSION                 (0.0625)

#define FLOW_PIN                        P9_1
#define TEMP_PIN                        P9_0

#define CIRC_PUMP_IN

#define DOSE_PUMP_A                     P9_2

#define PHFET                           P11_4
#define TDSFET                          P12_3
#define PH_CHANNEL                       P10_0
#define EC_CHANNEL                       P10_3

/* Conversion factor */
#define MICRO_TO_MILLI_CONV_RATIO        (1000u)

/* Acquistion time in nanosecond */
#define ACQUISITION_TIME_NS              (1000u)

/* ADC Scan delay in millisecond */
#define ADC_SCAN_DELAY_MS                (200u)

#define RING_BUFFER_SIZE                (16)
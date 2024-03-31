//Macros for program
#include "cyhal.h"

/*******************************************************************************
* Macros
*******************************************************************************/
/* LED blink timer clock value in Hz  */
#define LED_BLINK_TIMER_CLOCK_HZ          (10000)

/* LED blink timer period value */
#define LED_BLINK_TIMER_PERIOD            (9999)

#define PUMP_TIMER_CLOCK_HZ               (10000)
#define PUMP_TIMER_PERIOD                 (9999)

#define WIRE_TIMER_CLOCK_HZ               (10000)
#define WIRE_TIMER_PERIOD                 (5)

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
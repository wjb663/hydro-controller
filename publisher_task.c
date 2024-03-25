/******************************************************************************
* File Name:   publisher_task.c
*
* Description: This file contains the task that sets up the user button GPIO 
*              for the publisher and publishes MQTT messages on the topic
*              'MQTT_PUB_TOPIC' to control a device that is actuated by the
*              subscriber task. The file also contains the ISR that notifies
*              the publisher task about the new device state to be published.
*
* Related Document: See README.md
*
*
*******************************************************************************
* Copyright 2020-2023, Cypress Semiconductor Corporation (an Infineon company) or
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

#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"
#include "FreeRTOS.h"

/* Task header files */
#include "publisher_task.h"
#include "mqtt_task.h"
#include "subscriber_task.h"

/* Configuration file for MQTT client */
#include "mqtt_client_config.h"

/* Middleware libraries */
#include "cy_mqtt_api.h"
#include "cy_retarget_io.h"

/******************************************************************************
* Macros
******************************************************************************/
/* Interrupt priority for User Button Input. */
#define USER_BTN_INTR_PRIORITY          (3)

/* The maximum number of times each PUBLISH in this example will be retried. */
#define PUBLISH_RETRY_LIMIT             (10)

/* A PUBLISH message is retried if no response is received within this 
 * time (in milliseconds).
 */
#define PUBLISH_RETRY_MS                (1000)

/* Queue length of a message queue that is used to communicate with the 
 * publisher task.
 */
#define PUBLISHER_TASK_QUEUE_LENGTH     (3u)

/* Macro for ADC Channel configuration*/
#define SINGLE_CHANNEL 1
#define MULTI_CHANNEL  2

#define ADC_EXAMPLE_MODE SINGLE_CHANNEL

#if defined(CY_DEVICE_PSOC6A512K)  /* if the target is CY8CPROTO-062S3-4343W or CY8CPROTO-064B0S3*/
/* Channel 0 input pin */
#define VPLUS_CHANNEL_0  (P10_3)
#else
#define VPLUS_CHANNEL_0  (P10_0)

#define VPLUS_CHANNEL_1  (P10_4)
#define VREF_CHANNEL_1   (P10_5)
#endif

#if ADC_EXAMPLE_MODE == MULTI_CHANNEL

#if defined(CY_DEVICE_PSOC6A256K)  /* if the target is CY8CKIT-062S4 */
/* Channel 1 VPLUS input pin */)
/* Channel 1 VREF input pin */
#define VREF_CHANNEL_1   (P10_2)
#else
/* Channel 1 VPLUS input pin */
#define VPLUS_CHANNEL_1  (P10_4)
/* Channel 1 VREF input pin */
#define VREF_CHANNEL_1   (P10_5)
#endif

/* Number of scans every time ADC read is initiated */
#define NUM_SCAN                    (1)

#endif /* ADC_EXAMPLE_MODE == MULTI_CHANNEL */

/* Conversion factor */
#define MICRO_TO_MILLI_CONV_RATIO        (1000u)

/* Acquistion time in nanosecond */
#define ACQUISITION_TIME_NS              (1000u)

/* ADC Scan delay in millisecond */
#define ADC_SCAN_DELAY_MS                (200u)
#define NUM_READINGS					 (10)

/* Number of scans every time ADC read is initiated */
#define NUM_SCAN                         (1)

/* LED blink timer clock value in Hz  */
#define LED_BLINK_TIMER_CLOCK_HZ         (5000)
#define PUMP_TIMER_HZ					 (10000)

/* LED blink timer period value */
#define LED_BLINK_TIMER_PERIOD           (9999)

/*******************************************************************************
*       Enumerated Types
*******************************************************************************/
/* ADC Channel constants*/
enum ADC_CHANNELS
{
  CHANNEL_0 = 0,
  CHANNEL_1,
  NUM_CHANNELS
} adc_channel;

/******************************************************************************
* Function Prototypes
*******************************************************************************/
static void publisher_init(void);
static void publisher_deinit(void);
void print_heap_usage(char *msg);

void timer_init(void);
/* Multichannel initialization function */
void adc_multi_channel_init(void);




/* ADC Event Handler */
static void adc_event_handler(void* arg, cyhal_adc_event_t event);


/******************************************************************************
* Global Variables
*******************************************************************************/


/* FreeRTOS task handle for this task. */
TaskHandle_t publisher_task_handle;

/* Handle of the queue holding the commands for the publisher task */
QueueHandle_t publisher_task_q;

/* Structure to store publish message information. */
cy_mqtt_publish_info_t publish_info =
{
    .qos = (cy_mqtt_qos_t) MQTT_MESSAGES_QOS,
    .topic = MQTT_PUB_TOPIC,
    .topic_len = (sizeof(MQTT_PUB_TOPIC) - 1),
    .retain = false,
    .dup = false
};

/* Structure that stores the callback data for the GPIO interrupt event. */
cyhal_gpio_callback_data_t cb_data =
{
    .callback = isr_button_press,
    .callback_arg = NULL
};

/* ADC Object */
cyhal_adc_t adc_obj;

/* ADC Channel 0 Object */
cyhal_adc_channel_t adc_chan_0_obj;
cyhal_adc_channel_t adc_chan_1_obj;

/* Default ADC configuration */
const cyhal_adc_config_t adc_config = {
        .continuous_scanning=false, // Continuous Scanning is disabled
        .average_count=1,           // Average count disabled
        .vref=CYHAL_ADC_REF_VDDA,   // VREF for Single ended channel set to VDDA
        .vneg=CYHAL_ADC_VNEG_VSSA,  // VNEG for Single ended channel set to VSSA
        .resolution = 12u,          // 12-bit resolution
        .ext_vref = NC,             // No connection
        .bypass_pin = NC };       // No connection

/* Asynchronous read complete flag, used in Event Handler */
static bool async_read_complete = false;

/* Variable to store results from multiple channels during asynchronous read*/
int32_t result_arr[NUM_CHANNELS * NUM_SCAN] = {0};

bool timer_interrupt_flag = false;
bool led_blink_active_flag = true;

/* Variable for storing character read from terminal */
uint8_t uart_read_value;

/* Timer object used for blinking the LED */
cyhal_timer_t led_blink_timer;
cyhal_timer_t pump_timer;

bool EC_active = false;
bool pH_active = true;
int timerCount = 0;


int pumpCountUp = 0;



#if ADC_EXAMPLE_MODE == MULTI_CHANNEL

/* Asynchronous read complete flag, used in Event Handler */
static bool async_read_complete = false;

/* ADC Channel 1 Object */
cyhal_adc_channel_t adc_chan_1_obj;

/* Variable to store results from multiple channels during asynchronous read*/
int32_t result_arr[NUM_CHANNELS * NUM_SCAN] = {0};

#endif /* ADC_EXAMPLE_MODE == MULTI_CHANNEL */

/******************************************************************************
 * Function Name: publisher_task
 ******************************************************************************
 * Summary:
 *  Task that sets up the user button GPIO for the publisher and publishes 
 *  MQTT messages to the broker. The user button init and deinit operations,
 *  and the MQTT publish operation is performed based on commands sent by other
 *  tasks and callbacks over a message queue.
 *
 * Parameters:
 *  void *pvParameters : Task parameter defined during task creation (unused)
 *
 * Return:
 *  void
 *
 ******************************************************************************/
void publisher_task(void *pvParameters)
{
    /* Status variable */
    cy_rslt_t result;

    publisher_data_t publisher_q_data;

    /* Command to the MQTT client task */
    mqtt_task_cmd_t mqtt_task_cmd;

    /* To avoid compiler warnings */
    (void) pvParameters;

    /* Print message */
    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    //printf("\x1b[2J\x1b[;H");
    printf("-----------------------------------------------------------\r\n");
    printf("HAL: ADC using HAL\r\n");
    printf("-----------------------------------------------------------\r\n\n");

    /* Initialize and set-up the user button GPIO. */
    publisher_init();

    /* Update ADC configuration */
    result = cyhal_adc_configure(&adc_obj, &adc_config);
    if(result != CY_RSLT_SUCCESS)
    {
        printf("ADC configuration update failed. Error: %ld\n", (long unsigned int)result);
        CY_ASSERT(0);
    }
    result = cyhal_gpio_init(P12_0, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, true);
    if(result != CY_RSLT_SUCCESS)
    {
        printf("GPIO initialization failed. Error: %ld\n", (long unsigned int)result);
        CY_ASSERT(0);
    }
    result = cyhal_gpio_init(P12_1, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, false);
    if(result != CY_RSLT_SUCCESS)
    {
    	printf("ADC initialization failed. Error: %ld\n", (long unsigned int)result);
        CY_ASSERT(0);
    }
    result = cyhal_gpio_init(P13_4, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, false);
    if(result != CY_RSLT_SUCCESS)
    {
    	printf("ADC initialization failed. Error: %ld\n", (long unsigned int)result);
        CY_ASSERT(0);
    }

    /* Create a message queue to communicate with other tasks and callbacks. */
    publisher_task_q = xQueueCreate(PUBLISHER_TASK_QUEUE_LENGTH, sizeof(publisher_data_t));

    while (true)
    {
        /* Wait for commands from other tasks and callbacks. */
        if (pdTRUE == xQueueReceive(publisher_task_q, &publisher_q_data, portMAX_DELAY))
        {
            switch(publisher_q_data.cmd)
            {
                case PUBLISH_MQTT_MSG:
                {
                	// enables pH sensor if EC sensor has activated, pH is on by default.
                	// will only activate after the first run through, when EC would become active
                	if(timerCount < 6 && EC_active)
                	{
                		EC_active = false;
                		pH_active = true;
                		cyhal_gpio_write(P12_0, true);
                		cyhal_gpio_write(P12_1, false);
                		//printf("pH Initialized\n");
                		publish_info.topic = MQTT_PUB_TOPIC;
                		publish_info.topic_len = (sizeof(MQTT_PUB_TOPIC) - 1);
                	}
                	// enables EC sensor
                	// if 6 seconds have passed and pH is active start up the EC
                	else if (timerCount >= 6 && pH_active)
                	{
                		EC_active = true;
                		pH_active = false;
                		cyhal_gpio_write(P12_0, false);
                		cyhal_gpio_write(P12_1, true);
                		//printf("EC Initialized\n");
                		publish_info.topic = MQTT_PUB_TOPIC_TWO;
                		publish_info.topic_len = (sizeof(MQTT_PUB_TOPIC_TWO) - 1);
                	}

                	// reset timer count so timer can continue
                	if (timerCount >= 11)
                	{
                		timerCount = 0;
                	}
                	/* Variable to capture return value of functions */
                	cy_rslt_t result;

                    /* Variable to store ADC conversion result from channel 0 */
               	    int32_t adc_result_0 = 0;

               	    /* Variable to store ADC conversion result from channel 1 */
               	    int32_t adc_result_1 = 0;

               	    /* Initiate an asynchronous read operation. The event handler will be called
               	     * when it is complete. */
               	    result = cyhal_adc_read_async_uv(&adc_obj, NUM_SCAN, result_arr);
               	    if(result != CY_RSLT_SUCCESS)
               	    {
               	        printf("ADC async read failed. Error: %ld\n", (long unsigned int)result);
               	        CY_ASSERT(0);
               	    }

               	    if(pumpsOn == 1)
               	     {
               	     	cyhal_gpio_write(P13_4, true);
               	     	pumpCountUp++;
               	     	if(pumpCountUp == pumpSeconds)
               	     	{
               	     		pumpsOn = 0;
               	     		pumpCountUp = 0;
               	     		cyhal_gpio_write(P13_4, false);
               	     	}
               	     }

               	    /*
               	     * Read data from result list, input voltage in the result list is in
               	     * microvolts. Convert it millivolts and print input voltage
               	     *
               	     */
                    adc_result_0 = result_arr[CHANNEL_0] / MICRO_TO_MILLI_CONV_RATIO;
                	adc_result_1 = result_arr[CHANNEL_1] / MICRO_TO_MILLI_CONV_RATIO;
               	    /* Clear async read complete flag */
               	    async_read_complete = false;


                    /* Publish the data received over the message queue. */
                	//int32_t adc_result_0 = cyhal_adc_read_uv(&adc_chan_0_obj) / MICRO_TO_MILLI_CONV_RATIO;
                	char* buffer[20];
                	if(EC_active)
                	{
                		sprintf(buffer, "%d", adc_result_1);
                	}
                	else
                	{
                		sprintf(buffer, "%d", adc_result_0);
                	}
                    publish_info.payload = buffer;
                    publish_info.payload_len = strlen(publish_info.payload);

                    printf("\nPublisher: Publishing '%s' on the topic '%s'\n",
                           (char *) publish_info.payload, publish_info.topic);

                    // handle, publish info (type cy_mqtt_publish_info_t)
                    result = cy_mqtt_publish(mqtt_connection, &publish_info);

                    if (result != CY_RSLT_SUCCESS)
                    {
                        printf("  Publisher: MQTT Publish failed with error 0x%0X.\n\n", (int)result);

                        /* Communicate the publish failure with the the MQTT 
                         * client task.
                         */
                        mqtt_task_cmd = HANDLE_MQTT_PUBLISH_FAILURE;
                        xQueueSend(mqtt_task_q, &mqtt_task_cmd, portMAX_DELAY);
                    }

                    // is this the message that's printed after successful MQTT sending?
                    print_heap_usage("publisher_task: After publishing an MQTT message");
                    break;
                }
            }
        }
    }
}

/******************************************************************************
 * Function Name: publisher_init
 ******************************************************************************
 * Summary:
 *  Function that initializes and sets-up the user button GPIO pin along with  
 *  its interrupt.
 * 
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *
 ******************************************************************************/

/*//\
 * Change this to be adc initialization. Examine event to see if there will be any issues with a timer
 * acting as the enable/disable as opposed to button press.
 * CYBSP_USER_BTN is the user button
 *
 */
static void publisher_init(void)
{
	// Initialize channel 0
	adc_multi_channel_init();
	timer_init();

}

/******************************************************************************
 * Function Name: isr_button_press
 ******************************************************************************
 * Summary:
 *  GPIO interrupt service routine. This function detects button
 *  presses and sends the publish command along with the data to be published 
 *  to the publisher task over a message queue. Based on the current device 
 *  state, the publish data is set so that the device state gets toggled.
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
}


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

    /* Initialize the timer object. Does not use input pin ('pin' is NC) and
     * does not use a pre-configured clock source ('clk' is NULL). */
    result = cyhal_timer_init(&led_blink_timer, NC, NULL);

    /* timer init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Configure timer period and operation mode such as count direction,
       duration */
    cyhal_timer_configure(&led_blink_timer, &led_blink_timer_cfg);

    /* Set the frequency of timer's clock source */
    cyhal_timer_set_frequency(&led_blink_timer, LED_BLINK_TIMER_CLOCK_HZ);

    /* Assign the ISR to execute on timer interrupt */
    cyhal_timer_register_callback(&led_blink_timer, publish_timer, NULL);

    /* Set the event on which timer interrupt occurs and enable it */
    cyhal_timer_enable_event(&led_blink_timer, CYHAL_TIMER_IRQ_TERMINAL_COUNT,
                              7, true);

    /* Start the timer with the configured settings */
    cyhal_timer_start(&led_blink_timer);
    printf("Timer initializedV1.3\n");
}

void adc_multi_channel_init(void)
{
    /* Variable to capture return value of functions */
    cy_rslt_t result;

    /* Initialize ADC. The ADC block which can connect to the channel 0 input pin is selected */
    result = cyhal_adc_init(&adc_obj, VPLUS_CHANNEL_0, NULL);
    if(result != CY_RSLT_SUCCESS)
    {
        printf("ADC initialization failed. Error: %ld\n", (long unsigned int)result);
        CY_ASSERT(0);
    }

    /* ADC channel configuration */
    const cyhal_adc_channel_config_t channel_config = {
            .enable_averaging = false,  // Disable averaging for channel
            .min_acquisition_ns = ACQUISITION_TIME_NS, // Minimum acquisition time set to 1us
            .enabled = true };          // Sample this channel when ADC performs a scan

    /* Initialize a channel 0 and configure it to scan the channel 0 input pin in single ended mode. */
    result  = cyhal_adc_channel_init_diff(&adc_chan_0_obj, &adc_obj, VPLUS_CHANNEL_0,
                                          CYHAL_ADC_VNEG, &channel_config);
    if(result != CY_RSLT_SUCCESS)
    {
        printf("ADC first channel initialization failed. Error: %ld\n", (long unsigned int)result);
        CY_ASSERT(0);
    }

    /*
     * For multichannel configuration use to same channel configuration structure
     * "channel_config" to configure the second channel.
     * For second channel to be set to differential mode, two inputs from Pins
     * channel 1 input pin and channel 1 voltage reference pin are configured to be inputs.
     *
     */

    /* Initialize channel 1 in differential mode with VPLUS and VMINUS input pins */
    result = cyhal_adc_channel_init_diff(&adc_chan_1_obj, &adc_obj, VPLUS_CHANNEL_1,
                                         VREF_CHANNEL_1, &channel_config);
    if(result != CY_RSLT_SUCCESS)
    {
        printf("ADC second channel initialization failed. Error: %ld\n", (long unsigned int)result);
        CY_ASSERT(0);
    }

    /* Register a callback to handle asynchronous read completion */
     cyhal_adc_register_callback(&adc_obj, &adc_event_handler, result_arr);

     /* Subscribe to the async read complete event to process the results */
     cyhal_adc_enable_event(&adc_obj, CYHAL_ADC_ASYNC_READ_COMPLETE, CYHAL_ISR_PRIORITY_DEFAULT, true);

     printf("ADC is configured in multichannel configuration.\r\n\n");
     printf("Channel 0 is configured in single ended mode, connected to the \r\n");
     printf("channel 0 input pin. Provide input voltage at the channel 0 input pin \r\n");
     printf("Channel 1 is configured in differential mode, connected to the \r\n");
     printf("channel 1 input pin and channel 1 voltage reference pin. Provide input voltage at the channel 1 input pin and reference \r\n");
     printf("voltage at the Channel 1 voltage reference pin \r\n\n");
}

/*******************************************************************************
 * Function Name: adc_event_handler
 *******************************************************************************
 *
 * Summary:
 *  ADC event handler. This function handles the asynchronous read complete event
 *  and sets the async_read_complete flag to true.
 *
 * Parameters:
 *  void *arg : pointer to result list
 *  cyhal_adc_event_t event : ADC event type
 *
 * Return:
 *  void
 *
 *******************************************************************************/
static void adc_event_handler(void* arg, cyhal_adc_event_t event)
{
    if(0u != (event & CYHAL_ADC_ASYNC_READ_COMPLETE))
    {
        /* Set async read complete flag to true */
        async_read_complete = true;
    }
}







/* [] END OF FILE */

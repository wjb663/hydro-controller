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

#include "functions.h"

/******************************************************************************
* Macros
******************************************************************************/

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


/* LED blink timer clock value in Hz  */
#define LED_BLINK_TIMER_CLOCK_HZ          (5000)

/* LED blink timer period value */
#define LED_BLINK_TIMER_PERIOD            (9999)

/******************************************************************************
* Function Prototypes
*******************************************************************************/
static void publisher_init(void);
void print_heap_usage(char *msg);
void timer_init(void);
/* Multichannel initialization function */


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



/*******************************************************************************
* Global Variables
*******************************************************************************/

/* Variable for storing character read from terminal */
uint8_t uart_read_value;

/* Timer object used for blinking the LED */
cyhal_timer_t led_blink_timer;

/* Variable for storing character read from terminal */
uint8_t uart_read_value;

/* Timer object used for blinking the LED */
cyhal_timer_t led_blink_timer;
cyhal_timer_t pump_timer;

// FLAGS
bool EC_active = false;
bool pH_active = true;
bool timer_interrupt_flag = false;
bool led_blink_active_flag = true;

// timer increment variables
int timerCount = 0;
int pumpCountUp = 0;

int pumpsOn = 0;
int pumpSeconds = 0;

/******************************************************************************
 * Function Name: publisher_task
 ******************************************************************************
 * Summary:
 *  Initializes GPIO pins and calls publisher_init() which initializes ADC and timer.
 *  MQTT publish is performed based on timer sending a message to the RTOS queue
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

    /* Initialize and set-up the user button GPIO. */
    publisher_init();
    gpio_init();

    /* Create a message queue to communicate with other tasks and callbacks. */
    publisher_task_q = xQueueCreate(PUBLISHER_TASK_QUEUE_LENGTH, sizeof(publisher_data_t));

    // publisher_task is a task in scheduler, program execution doesn't get stuck in this loop
    // because it switches between this task and subscriber_task
    while (true)
    {
        /* Wait for commands from other tasks and callbacks. */
        if (pdTRUE == xQueueReceive(publisher_task_q, &publisher_q_data, portMAX_DELAY))
        {
            switch(publisher_q_data.cmd)
            {
                case PUBLISH_MQTT_MSG:
                {
                	//call function with timerCount as var

                	// enables pH sensor if EC sensor has activated, pH is on by default.
                	// will only activate after the first run through, at which point EC would become active
                	if(timerCount < 6 && EC_active)
                	{
                		EC_active = false;
                		pH_active = true;
                		cyhal_gpio_write(PH_FET, true);
                		cyhal_gpio_write(EC_FET, false);
                		publish_info.topic = MQTT_PUB_TOPIC;
                		publish_info.topic_len = (sizeof(MQTT_PUB_TOPIC) - 1);
                	}
                	// enables EC sensor
                	// if 6 seconds have passed and pH is active start up the EC
                	else if (timerCount >= 6 && pH_active)
                	{
                		EC_active = true;
                		pH_active = false;
                		cyhal_gpio_write(PH_FET, false);
                		cyhal_gpio_write(EC_FET, true);
                		publish_info.topic = MQTT_PUB_TOPIC_TWO;
                		publish_info.topic_len = (sizeof(MQTT_PUB_TOPIC_TWO) - 1);
                	}

                	// reset timer count so timer can continue
                	if (timerCount >= 11)
                	{
                		timerCount = 0;
                	}

                    /* Variable to store ADC conversion result from channel 0 */
               	    int32_t adc_result_0 = 0;

               	    /* Variable to store ADC conversion result from channel 1 */
               	    int32_t adc_result_1 = 0;

               	    /* Initiate an asynchronous read operation. The event handler will be called
               	     * when it is complete. */
               	    result = result_return();
               	    if(result != CY_RSLT_SUCCESS)
               	    {
               	        printf("ADC async read failed. Error: %ld\n", (long unsigned int)result);
               	        CY_ASSERT(0);
               	    }

               	    // if pumpSeconds topic has been written to, activate this block of code
               	    if(pumpsOn == 1)
               	     {
               	     	cyhal_gpio_write(PUMP_ONE, true);
               	     	pumpCountUp++;
               	     	if(pumpCountUp == pumpSeconds)
               	     	{
               	     		pumpsOn = 0;
               	     		pumpCountUp = 0;
               	     		cyhal_gpio_write(PUMP_ONE, false);
               	     	}
               	     }

               	    /*
               	     * Read data from result list, input voltage in the result list is in
               	     * microvolts. Convert it millivolts and print input voltage
               	     *
               	     */
                    adc_result_0 = channel0_return();
                	adc_result_1 = channel1_return();



                    /* Publish the data received over the message queue. */
                	//int32_t adc_result_0 = cyhal_adc_read_uv(&adc_chan_0_obj) / MICRO_TO_MILLI_CONV_RATIO;
                	char* buffer[20];

                	// Depending on flag a certain value is written
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
        } // end of if
    } // end of while loop
} // end of publisher_task function

/******************************************************************************
 * Function Name: publisher_init
 ******************************************************************************
 * Summary:
 *  Function that initializes and sets-up the ADC and timer
 * 
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *
 ******************************************************************************/

static void publisher_init(void)
{
	// Initialize channel 0
	adc_multi_channel_init();
	timer_init();
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
    printf("Timer initialized\n");
}  // end of timer_init function

/* [] END OF FILE */

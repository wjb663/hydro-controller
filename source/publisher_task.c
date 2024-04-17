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
#include "macros.h"

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
// #define LED_BLINK_TIMER_CLOCK_HZ          (5000)

/* LED blink timer period value */
#define LED_BLINK_TIMER_PERIOD            (9999)

/******************************************************************************
* Function Prototypes
*******************************************************************************/
// static void publisher_init(void);
void print_heap_usage(char *msg);
// void timer_init(void);
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
    .topic = PH_PUB_TOPIC,
    .topic_len = (sizeof(PH_PUB_TOPIC) - 1),
    .retain = false,
    .dup = false
};
/* Structure to store publish message information. */
cy_mqtt_publish_info_t pumpbusy_publish_info =
{
    .qos = (cy_mqtt_qos_t) MQTT_MESSAGES_QOS,
    .topic = PUMPBUSY_PUB_TOPIC,
    .topic_len = (sizeof(PUMPBUSY_PUB_TOPIC) - 1),
    .retain = false,
    .dup = false
};
/* Structure to store publish message information. */
cy_mqtt_publish_info_t pump1_publish_info =
{
    .qos = (cy_mqtt_qos_t) MQTT_MESSAGES_QOS,
    .topic = PUMP1_PUB_TOPIC,
    .topic_len = (sizeof(PUMP1_PUB_TOPIC) - 1),
    .retain = false,
    .dup = false
};


/*******************************************************************************
* Global Variables
*******************************************************************************/

/* Variable for storing character read from terminal */
uint8_t uart_read_value;

/* Variable for storing character read from terminal */
uint8_t uart_read_value;

/* Timer object used for blinking the LED */
extern cyhal_timer_t led_blink_timer;
extern cyhal_timer_t pump_timer;

// FLAGS
bool EC_active = false;
bool pH_active = true;
extern bool timer_interrupt_flag;
extern bool led_blink_active_flag;

extern volatile transaction_t transaction;

// timer increment variables
int timerCount = 0;
int pumpCountUp = 0;

extern float tempCelcius;

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

	cyhal_timer_start(&led_blink_timer);
    // publisher_init();    //Removed, inits below
    	// Initialize channel 0
	// adc_multi_channel_init();
	// timer_init();
    // gpio_init();

	//Should all be in the removed publisher init or something similar
	pumpbusy_publish_info.payload = "Ready";
    pumpbusy_publish_info.payload_len = strlen(pumpbusy_publish_info.payload);
    //Need to Implement - capture result and check for success
    cy_mqtt_publish(mqtt_connection, &pumpbusy_publish_info);

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
					/* Publish the data received over the message queue. */
                	//char* buffer[20];
					char buffer[20];

                	//call function with timerCount as var

                	// enables pH sensor if EC sensor has activated, pH is on by default.
                	// will only activate after the first run through, at which point EC would become active
                	if(timerCount < 6 && EC_active)
                	{
                		EC_active = false;
                		pH_active = true;
                		cyhal_gpio_write(PH_FET, true);
                		cyhal_gpio_write(EC_FET, false);
                		publish_info.topic = PH_PUB_TOPIC;
                		publish_info.topic_len = (sizeof(PH_PUB_TOPIC) - 1);
                	}
                	// enables EC sensor
                	// if 6 seconds have passed and pH is active start up the EC
                	else if (timerCount >= 6 && pH_active)
                	{
                		EC_active = true;
                		pH_active = false;
                		cyhal_gpio_write(PH_FET, false);
                		cyhal_gpio_write(EC_FET, true);
                		publish_info.topic = EC_PUB_TOPIC_TWO;
                		publish_info.topic_len = (sizeof(EC_PUB_TOPIC_TWO) - 1);
                	}

                	// reset timer count so timer can continue
                	if (timerCount >= 11)
                	{
                		timerCount = 0;
						publish_info.topic = TS_PUB_TOPIC;
                		publish_info.topic_len = (sizeof(TS_PUB_TOPIC) - 1);
						transaction = RESET;	//Reset temperature sensor
						sprintf(buffer, "%2.2f", tempCelcius);
						publish_info.payload = buffer;
                    	publish_info.payload_len = strlen(publish_info.payload);

                    	printf("\nPublisher: Publishing '%s' on the topic '%s'\n", (char *) publish_info.payload, publish_info.topic);

                    	// handle, publish info (type cy_mqtt_publish_info_t)
                    	result = cy_mqtt_publish(mqtt_connection, &publish_info);

						if (result != CY_RSLT_SUCCESS)
						{
							printf("  Publisher: MQTT Publish failed with error 0x%0X.\n\n", (int)result);

							/* Communicate the publish failure with the the MQTT client task.*/
							mqtt_task_cmd = HANDLE_MQTT_PUBLISH_FAILURE;
							xQueueSend(mqtt_task_q, &mqtt_task_cmd, portMAX_DELAY);
						}
						break;
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

               	    /*
               	     * Read data from result list, input voltage in the result list is in
               	     * microvolts. Convert it millivolts
               	     */
                    adc_result_0 = channel0_return();
                	adc_result_1 = channel1_return();

                	// Depending on flag a certain value is written
                	if(EC_active)
                	{
                		sprintf(buffer, "%d", (int)adc_result_1);
                	}
                	else
                	{
						adc_result_0 *= PH_MULT;
						adc_result_0 += PH_OFFSET;
                		sprintf(buffer, "%2.2f", (float)adc_result_0 / MICRO_TO_MILLI_CONV_RATIO);
                	}
                    publish_info.payload = buffer;
                    publish_info.payload_len = strlen(publish_info.payload);

                    printf("\nPublisher: Publishing '%s' on the topic '%s'\n", (char *) publish_info.payload, publish_info.topic);

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
				default: break;
            }
        } // end of if
    } // end of while loop
} // end of publisher_task function


/* [] END OF FILE */

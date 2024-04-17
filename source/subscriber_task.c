/******************************************************************************
* File Name:   subscriber_task.c
*
* Description: This file contains the task that initializes the user LED GPIO,
*              subscribes to the topic 'MQTT_SUB_TOPIC', and actuates the user LED
*              based on the notifications received from the MQTT subscriber
*              callback.
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

#include "cyhal.h"
#include "cybsp.h"
#include "string.h"
#include "FreeRTOS.h"

/* Task header files */
#include "subscriber_task.h"
#include "mqtt_task.h"

/* Configuration file for MQTT client */
#include "mqtt_client_config.h"

/* Middleware libraries */
#include "cy_mqtt_api.h"
#include "cy_retarget_io.h"

#include "functions.h"

/******************************************************************************
* Macros
******************************************************************************/
/* Maximum number of retries for MQTT subscribe operation */
#define MAX_SUBSCRIBE_RETRIES                   (3u)

/* Time interval in milliseconds between MQTT subscribe retries. */
#define MQTT_SUBSCRIBE_RETRY_INTERVAL_MS        (1000)

/* The number of MQTT topics to be subscribed to. */
#define SUBSCRIPTION_COUNT                      (1)

/* Queue length of a message queue that is used to communicate with the 
 * subscriber task.
 */
#define SUBSCRIBER_TASK_QUEUE_LENGTH            (2u)

/******************************************************************************
* Global Variables
*******************************************************************************/
/* Task handle for this task. */
TaskHandle_t subscriber_task_handle;


/* Handle of the queue holding the commands for the subscriber task */
QueueHandle_t subscriber_task_q;

/* Variable to denote the current state of the user LED that is also used by
 * the publisher task.
 */
uint32_t current_device_state = DEVICE_OFF_STATE;




/* Configure the subscription information structure. */
static cy_mqtt_subscribe_info_t ph_subscribe_info =
{
    .qos = (cy_mqtt_qos_t) MQTT_MESSAGES_QOS,
    .topic = MQTT_SUB_TOPIC,
    .topic_len = (sizeof(MQTT_SUB_TOPIC) - 1)
};

/* Configure the subscription information structure. */
static cy_mqtt_subscribe_info_t ec_subscribe_info =
{
    .qos = (cy_mqtt_qos_t) MQTT_MESSAGES_QOS,
    .topic = MQTT_SUB_TOPIC_TWO,
    .topic_len = (sizeof(MQTT_SUB_TOPIC_TWO) - 1)
};

/* Configure the subscription information structure. */
static cy_mqtt_subscribe_info_t p1_subscribe_info =
{
    .qos = (cy_mqtt_qos_t) MQTT_MESSAGES_QOS,
    .topic = PUMP1_SUB_TOPIC,
    .topic_len = (sizeof(PUMP1_SUB_TOPIC) - 1)
};

/* Configure the subscription information structure. */
static cy_mqtt_subscribe_info_t p2_subscribe_info =
{
    .qos = (cy_mqtt_qos_t) MQTT_MESSAGES_QOS,
    .topic = PUMP2_SUB_TOPIC,
    .topic_len = (sizeof(PUMP2_SUB_TOPIC) - 1)
};

/* Configure the subscription information structure. */
static cy_mqtt_subscribe_info_t p3_subscribe_info =
{
    .qos = (cy_mqtt_qos_t) MQTT_MESSAGES_QOS,
    .topic = PUMP3_SUB_TOPIC,
    .topic_len = (sizeof(PUMP3_SUB_TOPIC) - 1)
};

/******************************************************************************
* Function Prototypes
*******************************************************************************/
static void subscribe_to_topic(cy_mqtt_subscribe_info_t subscribe_info);
void print_heap_usage(char *msg);



extern int pumpSeconds;
extern int pumpsOn;
extern cyhal_timer_t pump_timer;
volatile bool pumpsBusy = false;
extern volatile bool pump_timer_interrupt_flag;
extern volatile unsigned pumpCounter;



/******************************************************************************
 * Function Name: subscriber_task
 ******************************************************************************
 * Summary:
 *  Calls function to subscribe to MQTT Topics. Contain the message queue
 *
 * Parameters:
 *  void *pvParameters : Task parameter defined during task creation (unused)
 *
 * Return:
 *  void
 *
 ******************************************************************************/
void subscriber_task(void *pvParameters)
{

    subscriber_data_t subscriber_q_data;
    /* To avoid compiler warnings */
    (void) pvParameters;

    /* Subscribe to the specified MQTT topic. */
    // subscribe_to_topic(ph_subscribe_info);
    // subscribe_to_topic(ec_subscribe_info);
    subscribe_to_topic(p1_subscribe_info);
    subscribe_to_topic(p2_subscribe_info);
    subscribe_to_topic(p3_subscribe_info);

    /* Create a message queue to communicate with other tasks and callbacks. */
    subscriber_task_q = xQueueCreate(SUBSCRIBER_TASK_QUEUE_LENGTH, sizeof(subscriber_data_t));

    while (true)
    {
        // if (pumpsBusy){break;}
        // if (pump_timer_interrupt_flag){
        //     pump_timer_interrupt_flag = false;
        //     cyhal_gpio_write(PUMP_ONE, 0);
        // }

        if (pdTRUE == xQueueReceive(subscriber_task_q, &subscriber_q_data, portMAX_DELAY))
        {
            switch(subscriber_q_data.cmd)
            {
                case UPDATE_DEVICE_STATE:
                {
                    print_heap_usage("subscriber_task: After updating LED state");
                    break;
                }
                // case PUMP1_SUB_TOPIC:
                // //Command sent with zero, turn off pumps
                //     if (subscriber_q_data.data == 0){
                //         cyhal_gpio_write(PUMP_ONE, 0);
                //     }
                //     else{
                //         pumpsBusy = true;
                //         cyhal_timer_start(&pump_timer);
                //         cyhal_gpio_write(PUMP_ONE, 1);
                //     }
                //     break;
                // case PUMP2_SUB_TOPIC:
                // //Command sent with zero, turn off pumps
                //     if (subscriber_q_data.data == 0){
                //         cyhal_gpio_write(PUMP_ONE, 0);
                //     }
                //     break;
                // case PUMP3_SUB_TOPIC:
                // //Command sent with zero, turn off pumps
                //     if (subscriber_q_data.data == 0){
                //         cyhal_gpio_write(PUMP_ONE, 0);
                //     }                
                //     break;
                // case PUMP4_SUB_TOPIC:
                // //Command sent with zero, turn off pumps
                //     if (subscriber_q_data.data == 0){
                //         cyhal_gpio_write(PUMP_ONE, 0);
                //     }                
                //     break;
                // case PUMP5_SUB_TOPIC:
                // //Command sent with zero, turn off pumps
                //     if (subscriber_q_data.data == 0){
                //         cyhal_gpio_write(PUMP_ONE, 0);
                //     }                
                //     break;
                                        
                default:
                    break;
            }
        }
    }

} // end of subscriber_task function

/******************************************************************************
 * Function Name: subscribe_to_topic
 ******************************************************************************
 * Summary:
 *  Function that subscribes to the MQTT topic specified by the macro 
 *  'MQTT_SUB_TOPIC'. This operation is retried a maximum of 
 *  'MAX_SUBSCRIBE_RETRIES' times with interval of 
 *  'MQTT_SUBSCRIBE_RETRY_INTERVAL_MS' milliseconds.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *
 ******************************************************************************/
static void subscribe_to_topic(cy_mqtt_subscribe_info_t subscribe_info)
{
    /* Status variable */
    cy_rslt_t result = CY_RSLT_SUCCESS;

    /* Command to the MQTT client task */
    mqtt_task_cmd_t mqtt_task_cmd;

    /* Subscribe with the configured parameters. */
    // subscribes to pH reading first, then EC_Reading, then pumpSeconds
    for (uint32_t retry_count = 0; retry_count < MAX_SUBSCRIBE_RETRIES; retry_count++)
    {
        result = cy_mqtt_subscribe(mqtt_connection, &subscribe_info, SUBSCRIPTION_COUNT);
        if (result == CY_RSLT_SUCCESS)
        {
            printf("\nMQTT client subscribed to the topic '%.*s' successfully.\n", subscribe_info.topic_len, subscribe_info.topic);
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(MQTT_SUBSCRIBE_RETRY_INTERVAL_MS));
    }
    

    if (result != CY_RSLT_SUCCESS)
    {
        printf("\nMQTT Subscribe failed with error 0x%0X after %d retries...\n\n", 
               (int)result, MAX_SUBSCRIBE_RETRIES);

        /* Notify the MQTT client task about the subscription failure */
        mqtt_task_cmd = HANDLE_MQTT_SUBSCRIBE_FAILURE;
        xQueueSend(mqtt_task_q, &mqtt_task_cmd, portMAX_DELAY);
    }
} // end of subscribe_to_topic function

/******************************************************************************
 * Function Name: mqtt_subscription_callback
 ******************************************************************************
 * Summary:
 *  Callback to handle incoming MQTT messages. This callback prints the 
 *  contents of the incoming message and changes variable values used in
 *  publisher_task() in publisher_task.c which activate the GPIO pin for
 *  the specific number of seconds
 *
 * Parameters:
 *  cy_mqtt_publish_info_t *received_msg_info : Information structure of the 
 *                                              received MQTT message
 *
 * Return:
 *  void
 *
 ******************************************************************************/
void mqtt_subscription_callback(cy_mqtt_publish_info_t *received_msg_info)
{
    subscriber_data_t subscriber_q_data;

    printf("  \nSubsciber: Incoming MQTT message received:\n"
           "    Publish topic name: %.*s\n"
           "    Publish QoS: %d\n"
           "    Publish payload: %.*s\n",
           received_msg_info->topic_len, received_msg_info->topic,
           (int) received_msg_info->qos,
           (int) received_msg_info->payload_len, (const char *)received_msg_info->payload);

    // if there's an upload to pump seconds topic, convert the payload to int and pass it to extern var payloadTimer
    // payloadTimer is used in publisher_task.c publisher_task() switch statement
    // if (strncmp(received_msg_info->topic, MQTT_SUB_TOPIC_THREE, received_msg_info->topic_len) == 0)
    // {
    // 	int payloadTimer = atoi(received_msg_info->payload);
    // 	pumpSeconds = payloadTimer;
    // 	pumpsOn = 1;
    // 	printf("\nPUMPING NOW");
    // 	printf("\nPump for %d seconds.", pumpSeconds);
    // }

    int payloadInteger = atoi(received_msg_info->payload);
    //Command sent with zero, turn off pumps
    if (payloadInteger == 0){
        pumpsBusy = false;
        cyhal_gpio_write(PUMP_ONE, 0);
        cyhal_gpio_write(PUMP_TWO, 0);
        cyhal_gpio_write(PUMP_THREE, 0);
        return;
    }
    else if (pumpsBusy){return;}

    if (strncmp(received_msg_info->topic, PUMP1_SUB_TOPIC, received_msg_info->topic_len) == 0){
        cyhal_gpio_write(PUMP_ONE, 1);
    }

    else if (strncmp(received_msg_info->topic, PUMP2_SUB_TOPIC, received_msg_info->topic_len) == 0){
        cyhal_gpio_write(PUMP_TWO, 1);

    }

    else if (strncmp(received_msg_info->topic, PUMP3_SUB_TOPIC, received_msg_info->topic_len) == 0){
        cyhal_gpio_write(PUMP_THREE, 1);
    }

    else if (strncmp(received_msg_info->topic, PUMP4_SUB_TOPIC, received_msg_info->topic_len) == 0){
        cyhal_gpio_write(PUMP_FOUR, 1);

    }

    else if (strncmp(received_msg_info->topic, PUMP5_SUB_TOPIC, received_msg_info->topic_len) == 0){
        cyhal_gpio_write(PUMP_FIVE, 1);

    }

    pumpsBusy = true;
    pumpCounter = payloadInteger;
    cyhal_timer_reset(&pump_timer);
    cyhal_timer_start(&pump_timer);

    //Populate struct to pass to subscriber task queue.
    // subscriber_q_data.cmd = received_msg_info->topic;
    // subscriber_q_data.data = atoi(received_msg_info->payload);

    print_heap_usage("MQTT subscription callback");

    /* Send the command and data to subscriber task queue */
    xQueueSend(subscriber_task_q, &subscriber_q_data, portMAX_DELAY);
} // end of mqtt_subscription_callback function


/* [] END OF FILE */

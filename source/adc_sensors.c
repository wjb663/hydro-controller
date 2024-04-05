
#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "cyhal_adc.h"
#include "functions.h"

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
int32_t result_arr[2 * NUM_SCAN] = {0};

static void adc_event_handler(void* arg, cyhal_adc_event_t event);



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
 * Function Name: adc_multi_channel_init
 ******************************************************************************
 * Summary:
 *  ADC Initialization code
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *
 ******************************************************************************/

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
                                         CYHAL_ADC_VNEG, &channel_config);
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

     result = cyhal_adc_configure(&adc_obj, &adc_config);
     if(result != CY_RSLT_SUCCESS)
     {
             printf("ADC configuration update failed. Error: %ld\n", (long unsigned int)result);
             CY_ASSERT(0);
     }
}	// end of adc_multi_channel_init function

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
} // end of adc_event_handler function


/*
 * Following functions are used in publisher_task() of publisher_task.c to compute adc output
 */
cy_rslt_t result_return(void)
{
	return cyhal_adc_read_async_uv(&adc_obj, NUM_SCAN, result_arr);
}

int32_t channel0_return(void)
{
	return result_arr[CHANNEL_0] / MICRO_TO_MILLI_CONV_RATIO;
}

int32_t channel1_return(void)
{
	async_read_complete = false;
	return result_arr[CHANNEL_1] / MICRO_TO_MILLI_CONV_RATIO;
}



#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "sensor_functions.h"

void gpio_init()
{
	// GPIO INITIALIZATION
	cy_rslt_t result;
	result = cyhal_gpio_init(PH_FET, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, true);
	if(result != CY_RSLT_SUCCESS)
	{
	        printf("GPIO initialization failed. Error: %ld\n", (long unsigned int)result);
	        CY_ASSERT(0);
	}
	result = cyhal_gpio_init(EC_FET, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, false);
	if(result != CY_RSLT_SUCCESS)
	{
	    	printf("ADC initialization failed. Error: %ld\n", (long unsigned int)result);
	        CY_ASSERT(0);
	}
	result = cyhal_gpio_init(PUMP_ONE, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, false);
	if(result != CY_RSLT_SUCCESS)
	{
	    	printf("ADC initialization failed. Error: %ld\n", (long unsigned int)result);
	        CY_ASSERT(0);
	}
}

#include "macros.h"
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "functions.h"

/* FreeRTOS header files */
#include "FreeRTOS.h"
#include "task.h"


volatile unsigned ringBuffer[RING_BUFFER_SIZE];
unsigned char skip = 0xCC;      //Skip ROM
unsigned char convert = 0x44;   //Start conversion
unsigned char read = 0xBE;   //Read scratchpad

unsigned volatile ringHead = 0;
unsigned volatile ringTail = 0;

extern cyhal_timer_t wire_timer;
extern cyhal_timer_t write_timer;
extern cyhal_timer_t read_timer;
extern bool wire_busy;
extern bool wire_initialized;


bool skip_rom = true;
bool conversionComplete = false;

volatile transaction_t transaction;

uint8_t bit = 0;
uint8_t timeoutCounter = INIT_RETRIES;
uint16_t binaryTemp = 0;
float tempCelcius = 0;



void initialize_wire(void){
    //Reset pulse
    cyhal_gpio_write(TEMP_PIN, 0);
    cyhal_timer_start(&wire_timer);
    wire_busy = true;
}

void write_wire_byte(uint8_t data){
    unsigned char temp;
    temp = data>>bit++;
    temp &= 0x01;
    cyhal_gpio_write(TEMP_PIN, 0);
    cyhal_gpio_write(TEMP_PIN, temp);
    cyhal_timer_start(&write_timer);
    wire_busy = true;
    if (bit >= 8){
        bit = 0;
        transaction++;
    }

}

void read_wire(void){
    switch (transaction)
    {
    case PRESENSE:

    case POLL:
        ringBuffer[ringHead++] = cyhal_gpio_read(TEMP_PIN);
        if (ringHead >= RING_BUFFER_SIZE) ringHead = 0;
        break;
    default:
        break;
    }

}

void print_wire(void){
    for (int i = 0; i < RING_BUFFER_SIZE; i++){
        printf("%u,", ringBuffer[i]);
    }
    printf("\r\nWire\r\n");
}

//wire_process
//function to be called in main loop
void wire_process(void *pvParameters){
    
    (void) pvParameters;

    for (;;){

        switch (transaction)
        {
        case RESET:
            if (wire_busy){break;}
            initialize_wire();
            transaction++;
            break;
        case PRESENSE:
            if (wire_busy){break;}
            if (timeoutCounter-- <= 0){
                transaction = ERROR;
                break;
            }
            cyhal_timer_start(&wire_timer);
            wire_busy = true;
            break;
        case SKIP_ROM:
            if (wire_busy){break;}
            if (wire_initialized){
                printf("Wire Initialized\r\n");
                wire_initialized = false;
            }
            write_wire_byte(skip);
            break;
        case CONVERT_T:
            if (conversionComplete){        //Happens after transaction gets reset after temp conversion
                transaction = SCRATCH;
                break;
            }
            if (wire_busy){break;}
            else{write_wire_byte(convert);}
            break;
        case POLL:
            if (wire_busy){break;}
            if (ringTail >= RING_BUFFER_SIZE){ringTail = 0;}
            if (ringTail != ringHead){
                if (ringBuffer[ringTail++] == 1){
                    printf("Temperature Conversion Complete\r\n");
                    ringHead = 0;
                    ringTail = 0;
                    transaction = RESET;
                    conversionComplete = true;
                    break;
                }
            }
            cyhal_gpio_write(TEMP_PIN, 0);
            cyhal_gpio_write(TEMP_PIN, 1);
            wire_busy = true;
            cyhal_timer_start(&read_timer);
            break;
        case SCRATCH:
            if (wire_busy){break;}
            write_wire_byte(read);
            break;
        case PARSE:
            if (wire_busy){break;}
            cyhal_gpio_write(TEMP_PIN, 0);
            cyhal_gpio_write(TEMP_PIN, 1);
            wire_busy = true;
            cyhal_timer_start(&read_timer);

            if (ringTail >= RING_BUFFER_SIZE){ringTail = 0;}
            if (ringTail != ringHead){
                binaryTemp += ringBuffer[ringTail++] << bit++;
                if (bit >= REG_SIZE){
                    bit = 0;
                    tempCelcius = binaryTemp * TEMP_CONVERSION;
                    binaryTemp = 0;
                    transaction = DONE;
                    break;
                }
            }
            break;
        case DONE:

            printf("Temperature: %f\r\n", tempCelcius);
            transaction = -1;
            // transaction = RESET;
            conversionComplete = false;
            
            //vTaskDelete(NULL);
            break;
        case ERROR:
            printf("Wire Initialization Failed\r\n");
            transaction = -1;
            break;
        default:
            break;
        }
    }
        
}


#include "macros.h"
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "functions.h"


volatile unsigned ringBuffer[RING_BUFFER_SIZE];
unsigned char skip = 0xCC;
unsigned char convert = 0x44;

unsigned ringHead = 0;
unsigned ringTail = 0;

extern cyhal_timer_t wire_timer;
extern cyhal_timer_t write_timer;
extern bool wire_busy;
extern bool wire_initialized;

bool skip_rom = true;
bool conversion = true;

volatile enum {
    RESET,
    PRESENSE,
    SKIP_ROM,
    CONVERT_T,
    POLL,
} transaction;

uint8_t bit = 0;



int initialize_wire(void){
    for (int i = 0; i < RING_BUFFER_SIZE; i++){ringBuffer[i] = 1;}
    cyhal_gpio_write(TEMP_PIN, 0);
    cyhal_timer_start(&wire_timer);
    wire_busy = true;
    return 0;
}


void write_wire(u_int8_t b){
    cyhal_gpio_write(TEMP_PIN, 0);
    cyhal_gpio_write(TEMP_PIN, b);
    cyhal_timer_start(&write_timer);
    wire_busy = true;
}

void write_wire_byte(uint8_t b){
    unsigned char temp;
    temp = b>>bit++;
    temp &= 0x01;
    write_wire(temp);
    if (bit >= 8){
        bit = 0;
        transaction++;
    }
    // ringBuffer[b] = temp;
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
        printf("%u", ringBuffer[i]);
    }
    printf("\r\nWire\r\n");
}

void wire_process(void){

    switch (transaction)
    {
    case RESET:
        if (wire_busy){break;}
        initialize_wire();
        break;
    case PRESENSE:
        if(ringTail != ringHead){
            if (ringTail >= RING_BUFFER_SIZE){ringTail = 0;}
            if (ringBuffer[1] == 0){
                print_wire();
                break;
            }

            if (!wire_busy){
                wire_busy = true;
                cyhal_timer_start(&wire_timer);
            }
            // if (ringBuffer[ringTail++] == 0){
            //     transaction++;
            //     printf("Wire Initialized\r\n");
            // }
            // else if (!wire_busy){
            //     // cyhal_timer_reset(&write_timer);
            //     // cyhal_timer_start(&write_timer);
            // }
        }
        break;
    case SKIP_ROM:
        if (wire_busy){return;}
        write_wire_byte(skip);
        break;
    case CONVERT_T:
        if (wire_busy){return;}
        write_wire_byte(convert);
        break;
    case POLL:
        if (ringTail == ringHead){
            printf("RingBufferTailFault\r\n");
            break;
        }
        if (ringTail >= RING_BUFFER_SIZE){ringTail = 0;}
        if (ringBuffer[ringTail++] == 1){
            transaction++;
            break;
        }

        if (wire_busy){return;}
        cyhal_gpio_write(TEMP_PIN, 0);
        cyhal_gpio_write(TEMP_PIN, 1);
        cyhal_timer_start(&write_timer);
        wire_busy = true;
        break;
    default:
        break;
    }

    
}

void isr_wire(void *callback_arg, cyhal_gpio_event_t event)
{
    switch (event){
        case CYHAL_GPIO_IRQ_RISE:
            printf("Wire Rise\r\n");
        break;
        case CYHAL_GPIO_IRQ_FALL:
            printf("Wire Fall\r\n");
        break;
            default:
        break;
    }
    (void) callback_arg;
    (void) event;

    /* Set the interrupt flag and process it from the main while(1) loop */
    // ringBuffer[ringHead++] = cyhal_gpio_read(TEMP_PIN);
    // if (ringHead >= RING_BUFFER_SIZE) ringHead = 0;
    // if (ringBuffer[ringTail++] == 0){
    //     printf("0\r\n");
    // }
    // else
    // {
    //     printf("Wire Timer Read\r\n");
    // }
}
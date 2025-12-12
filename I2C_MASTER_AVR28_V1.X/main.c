#include "mcc_generated_files/system/system.h"
#include <stdio.h>
#include <util/delay.h>
#define F_CPU (4000000UL)

#define SLAVE_ADDR1 0x1
#define SLAVE_ADDR2 0x2
#define SLAVE_ADDR3 0x3

void masterSend(uint8_t slaveAddr, uint8_t value)
{
    TWI0_Write(slaveAddr, &value, 1);
    while(TWI0_IsBusy());
    printf("Master stuurt naar 0x%X: %u\n", slaveAddr, value);
}

uint8_t masterReceive(uint8_t slaveAddr)
{
    uint8_t value = 0;
    TWI0_Read(slaveAddr, &value, 1);
    while(TWI0_IsBusy());
    printf("Master ontvangt van 0x%X: %u\n", slaveAddr, value);
    return value;
}

void main(void)
{
    SYSTEM_Initialize();
    printf("Master gestart\n");

    while(1)
    {
        // Eventueel master kan data naar slaves sturen
        if (SW0_GetValue() == 0) // knop ingedrukt
        {
            masterSend(SLAVE_ADDR1, 42);
         //   masterSend(SLAVE_ADDR2, 42);
            _delay_ms(200);
        }

        // Poll beide slaves
        uint8_t slaveData1 = masterReceive(SLAVE_ADDR1);
        uint8_t slaveData2 = masterReceive(SLAVE_ADDR2);
        uint8_t slaveData3 = masterReceive(SLAVE_ADDR3);
        
        // Lampje gaat branden als beide slaves een waarde > 0 teruggeven
        if (slaveData1 > 0) 
        {
            DEBUG_LED_SetLow();
            _delay_ms(500);
            DEBUG_LED_SetHigh();
            _delay_ms(500);
            printf("Beide slaves actief: LED aan\n");
        }
    }
}

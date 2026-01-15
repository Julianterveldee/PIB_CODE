#include "mcc_generated_files/system/system.h"
#define F_CPU (4000000UL)
#include <util/delay.h>
#include <stdio.h>

// ---------------------- I2C ----------------------
#define HOME_ADDR     0x04
#define I2C_TIMEOUT   5000

// ---------------------- Home Assist Commands ----------------------
#define HA_CMD_IDLE        0
#define HA_CMD_START       1
#define HA_CMD_STOP        2
#define HA_CMD_OPLADEN     3
#define HA_CMD_NOOD        10

// ---------------------- Home Assist Status ----------------------
#define HA_STATUS_IDLE     0
#define HA_STATUS_START    1
#define HA_STATUS_STOP     2
#define HA_STATUS_OPLADEN  3
#define HA_STATUS_RESET    4
#define HA_STATUS_NOOD     10

// ---------------------- State machine ----------------------
typedef enum { 
    S_IDLE, 
    S_STOFZUIGEN, 
    S_OPLADEN, 
    S_NOOD, 
    S_ERROR 
} states;

typedef enum { 
    E_NONE, 
    E_START, 
    E_STOP, 
    E_START_OPLADEN, 
    E_NOOD, 
    E_RESET 
} events;

states currentState = S_IDLE;
states nextState    = S_IDLE;
events currentEvent = E_NONE;

uint8_t homeStatus = 0;

// ---------------------- Prototypes ----------------------
void setEvent(events e);
void resetEvent(void);

void sendHomeCommand(uint8_t cmd);
uint8_t readHomeStatus(void);

// ---------------------- MAIN ----------------------
int main(void)
{
    SYSTEM_Initialize();
    printf("Master gestart (alleen Home Assist)\n");

    while(1)
    {
        // ----------- Lees Home Assist status -----------
        homeStatus = readHomeStatus();

        // ----------- Events bepalen -----------
        if(homeStatus == HA_STATUS_NOOD)
            setEvent(E_NOOD);
        else if(homeStatus == HA_STATUS_START)
            setEvent(E_START);
        else if(homeStatus == HA_STATUS_STOP)
            setEvent(E_STOP);
        else if(homeStatus == HA_STATUS_OPLADEN)
            setEvent(E_START_OPLADEN);
        else if(homeStatus == HA_STATUS_RESET)
            setEvent(E_RESET);

        // ----------- State machine -----------
        switch(currentState)
        {
            case S_IDLE:
                if(currentEvent == E_START) {
                    nextState = S_STOFZUIGEN;
                }
                else if(currentEvent == E_START_OPLADEN) {
                    nextState = S_OPLADEN;
                }
                else if(currentEvent == E_NOOD) {
                    nextState = S_NOOD;
                }
                sendHomeCommand(HA_CMD_IDLE);
                break;

            case S_STOFZUIGEN:
                if(currentEvent == E_STOP) {
                    nextState = S_IDLE;
                }
                else if(currentEvent == E_START_OPLADEN) {
                    nextState = S_OPLADEN;
                }
                else if(currentEvent == E_NOOD) {
                    nextState = S_NOOD;
                }
                sendHomeCommand(HA_CMD_START);
                break;

            case S_OPLADEN:
                if(currentEvent == E_START) {
                    nextState = S_STOFZUIGEN;
                }
                else if(currentEvent == E_STOP) {
                    nextState = S_IDLE;
                }
                else if(currentEvent == E_NOOD) {
                    nextState = S_NOOD;
                }
                sendHomeCommand(HA_CMD_OPLADEN);
                break;

            case S_NOOD:
                sendHomeCommand(HA_CMD_NOOD);
                break;

            case S_ERROR:
                break;
        }

        currentState = nextState;
        resetEvent();
        _delay_ms(200);
    }
}

// ---------------------- Event helpers ----------------------
void setEvent(events e) { currentEvent = e; }
void resetEvent(void)   { currentEvent = E_NONE; }

// ---------------------- I2C functies (simpel) ----------------------
void sendHomeCommand(uint8_t cmd)
{
    uint16_t timeout = I2C_TIMEOUT;
    TWI0_Write(HOME_ADDR, &cmd, 1);
    while(TWI0_IsBusy() && timeout--);

    if(timeout == 0)
        printf("I2C timeout bij sturen cmd=%u\n", cmd);
}

uint8_t readHomeStatus(void)
{
    uint8_t value = 0;
    uint16_t timeout = I2C_TIMEOUT;

    TWI0_Read(HOME_ADDR, &value, 1);
    while(TWI0_IsBusy() && timeout--);

    if(timeout == 0) {
        printf("I2C timeout bij lezen status\n");
        return 0xFF;
    }

    return value;
}

#include "mcc_generated_files/system/system.h"
#define F_CPU (4000000UL)
#include <util/delay.h>
#include <stdio.h>

// ---------------------- I2C adressen ----------------------
#define ACCU_ADDR        0x01
#define STOFZUIGER_ADDR  0x02
#define NAV_ADDR         0x03
#define HOME_ADDR        0x04
#define I2C_TIMEOUT      5000

// ---------------------- Commands ----------------------
#define CMD_accu_nood           10
#define CMD_accu_stop_laden     1 
#define CMD_accu_start_laden    2

#define CMD_sz_aan              1
#define CMD_sz_uit              2

#define CMD_nav_start           1
#define CMD_nav_stop            2

#define CMD_ha_nood             10
#define CMD_ha_rijden           1
#define CMD_ha_stil             2
#define CMD_ha_vast             3
#define CMD_ha_sz_aan           4
#define CMD_ha_sz_uit           5
#define CMD_ha_laden_wel        6
#define CMD_ha_laden_niet       7
#define CMD_ha_error            8

// ---------------------- Registers per module ----------------------
// ACCU registers
typedef enum {
    ACCU_STATUS     = 0x00, // 10=nood, 1=accu laadt op, 2=accu laadt niet op
    ACCU_COMMAND    = 0x01, // 10=nood, 1=start laden, 2=stop laden
} AccuReg;

// STOFZUIGER registers
typedef enum {
    SZ_STATUS       = 0x00, // 10=nood, 1=stfozuiger aan, 2=stofzuiger uit
    SZ_COMMAND      = 0x01, // 1=stofzuiger aan, 2= stofzuiger uit
} StofzuigerReg;

// NAVIGATIE registers
typedef enum {
    NAV_STATUS      = 0x00, // 10=nood, 1=rijden, 2=stil, 3=vast/opgepakt
    NAV_COMMAND     = 0x01, // 1=starten met rijden, 2=stoppen met rijden
} NaviReg;

// HOME ASSIST registers
typedef enum {
    HOME_STATUS     = 0x00, // 10=nood, 1=start, 2=stop, 3=opladen, 4=reset
    HOME_COMMAND    = 0x01, // 1=stofzuiger rijdt, 2=stofzuiger rijdt niet, 3=stofzuiger zit vast, 4=stofzuiger aan, 5=stofzuiger uit, 6=stofzuiger laadt op, 7=stofzuiger laaft niet op
} HomeReg;

uint8_t accuStatus        = 0;
uint8_t stofzuigerStatus  = 0;
uint8_t navStatus         = 0;
uint8_t homeStatus        = 0;

typedef enum { S_IDLE, S_STOFZUIGEN, S_OPLADEN, S_NOOD, S_ERROR } states;
typedef enum { E_NONE, E_START, E_STOP, E_START_OPLADEN, E_STOP_OPLADEN, E_ERROR, E_NOOD, E_RESET } events;
typedef enum{F_ENTRY, F_ACTIVITY, F_EXIT} flow;

states currentState = S_IDLE;
events currentEvent = E_NONE;
flow currentFlow = F_ENTRY;
states nextState = S_IDLE;

void setEvent(events newEvent);
void resetEvent();

// ---------------------- ENTRY functies ----------------------
void entry_S_IDLE(void);
void entry_S_STOFZUIGEN(void);
void entry_S_OPLADEN(void);
void entry_S_ERROR (void);
void entry_S_NOOD(void);

// ---------------------- ACTIVITY functies ----------------------
void activity_S_IDLE(void);
void activity_S_STOFZUIGEN(void);
void activity_S_OPLADEN(void);
void activity_S_ERROR(void);
void activity_S_NOOD(void);


// ---------------------- I2C functies ----------------------
void i2cWrite(uint8_t slaveAddr, uint8_t reg, uint8_t value);
uint8_t i2cRead(uint8_t slaveAddr, uint8_t reg);

// ---------------------- I2C extra controles ----------------------
bool i2cCheckSlave(uint8_t slaveAddr);
void i2cCheckBus(void);

int main(void)
{
    SYSTEM_Initialize();
    uC_LED_SetHigh();
    while(1)
    {
        // ---------------------- I2C controle ----------------------
        i2cCheckBus();
        i2cCheckSlave(ACCU_ADDR);
        i2cCheckSlave(STOFZUIGER_ADDR);
        i2cCheckSlave(NAV_ADDR);
        i2cCheckSlave(HOME_ADDR);

        // ------------------------POLL MODULES------------------------
        accuStatus        = i2cRead(ACCU_ADDR, ACCU_STATUS);
        stofzuigerStatus  = i2cRead(STOFZUIGER_ADDR, SZ_STATUS);
        navStatus         = i2cRead(NAV_ADDR, NAV_STATUS);
        homeStatus        = i2cRead(HOME_ADDR, HOME_STATUS);

        //---------------------SET EVENTS---------------------------------
        if(accuStatus == 10 || navStatus == 10 || stofzuigerStatus == 10 || homeStatus == 10) {
            setEvent(E_NOOD);
        }
        else {
            if(homeStatus == 1) setEvent(E_START);
            if(homeStatus == 2 || navStatus == 3) setEvent(E_STOP);
            if(homeStatus == 6 || accuStatus == 1) setEvent(E_START_OPLADEN);
            if(accuStatus == 2) setEvent(E_STOP_OPLADEN);
            if(homeStatus == 4) setEvent(E_RESET);
        }

        //-------------------STATE MACHINE----------------------------------
        switch(currentState) {
            case S_IDLE:
                switch(currentEvent) {
                    case E_START: 
                        nextState = S_STOFZUIGEN; 
                        resetEvent(); 
                        break;
                    case E_START_OPLADEN: 
                        nextState = S_OPLADEN; 
                        resetEvent(); 
                        break;
                    case E_ERROR: 
                        nextState = S_ERROR; 
                        resetEvent(); 
                        break;
                    case E_NOOD: 
                        nextState = S_NOOD; 
                        resetEvent(); 
                        break;
                    default: 
                        break;
                }
                switch(currentFlow){
                    case F_ENTRY: 
                        entry_S_IDLE(); 
                        currentFlow = F_ACTIVITY;
                        
                    case F_ACTIVITY: 
                        activity_S_IDLE();
                        if(nextState == currentState){
                            break;
                        } else{
                            currentFlow = F_EXIT;
                        }
                    case F_EXIT: 
                        currentFlow = F_ENTRY; 
                        break;
                }
                break;
            case S_STOFZUIGEN:
                switch(currentEvent) {
                    case E_STOP: 
                        nextState = S_IDLE; 
                        resetEvent(); 
                        break;
                    case E_START_OPLADEN: 
                        nextState = S_OPLADEN; 
                        resetEvent(); 
                        break;
                    case E_ERROR: 
                        nextState = S_ERROR; 
                        resetEvent(); 
                        break;
                    case E_NOOD: 
                        nextState = S_NOOD; 
                        resetEvent(); 
                        break;            
                    default: 
                        break;
                }
                switch(currentFlow){
                    case F_ENTRY: 
                        entry_S_STOFZUIGEN(); 
                        currentFlow = F_ACTIVITY;
                    case F_ACTIVITY:
                        activity_S_STOFZUIGEN();
                        if(nextState == currentState){
                            break;
                        } else{
                            currentFlow = F_EXIT;
                        }
                    case F_EXIT: 
                        currentFlow = F_ENTRY; 
                        break;
                }
                break;
            case S_OPLADEN:
                switch(currentEvent) {
                    case E_START: 
                        nextState = S_STOFZUIGEN; 
                        resetEvent(); 
                        break;
                    case E_STOP_OPLADEN: 
                        nextState = S_IDLE; 
                        resetEvent(); 
                        break;
                    case E_ERROR: 
                        nextState = S_ERROR; 
                        resetEvent(); 
                        break;
                    case E_NOOD: 
                        nextState = S_NOOD; 
                        resetEvent(); 
                        break;
                    default: 
                        break;
                }
                switch(currentFlow){
                    case F_ENTRY: 
                        entry_S_OPLADEN(); 
                        currentFlow = F_ACTIVITY;
                    case F_ACTIVITY: 
                        activity_S_OPLADEN();
                        if(nextState == currentState){
                            break;
                        } else{
                            currentFlow = F_EXIT;
                        }
                    case F_EXIT: 
                        currentFlow = F_ENTRY; 
                        break;
                }
                break;
                case S_ERROR:
                switch(currentEvent) {
                    case E_RESET: 
                        nextState = S_IDLE; 
                        resetEvent(); 
                        break;
                    default: 
                        break;
                }
                switch(currentFlow){
                    case F_ENTRY: 
                        entry_S_ERROR(); 
                        currentFlow = F_ACTIVITY;
                    case F_ACTIVITY: 
                        activity_S_ERROR();
                        if(nextState == currentState){
                            break;
                        } else{
                            currentFlow = F_EXIT;
                        }
                    case F_EXIT: 
                        currentFlow = F_ENTRY; 
                        break;
                }
                break;
            case S_NOOD:
                switch(currentFlow){
                    case F_ENTRY: 
                        entry_S_NOOD(); 
                        currentFlow = F_ACTIVITY;
                    case F_ACTIVITY: 
                        activity_S_NOOD();
                        if(nextState == currentState){
                            break;
                        } else{
                            currentFlow = F_EXIT;
                        }
                    case F_EXIT: 
                        currentFlow = F_ENTRY; 
                        break;
                }
                break;
        }
        currentState = nextState;
        _delay_ms (100);
    }    
}
void setEvent(events newEvent){ currentEvent = newEvent; }
void resetEvent(){ currentEvent = E_NONE; }

void entry_S_IDLE () {
    i2cWrite(STOFZUIGER_ADDR, SZ_COMMAND, CMD_sz_uit);
    i2cWrite(NAV_ADDR, NAV_COMMAND, CMD_nav_stop);
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_stil);
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_sz_uit);
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_laden_niet);
    if (navStatus == 3) {
        i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_vast);
    }
}
void entry_S_STOFZUIGEN () {
    i2cWrite(STOFZUIGER_ADDR, SZ_COMMAND, CMD_sz_aan);
    i2cWrite(NAV_ADDR, NAV_COMMAND, CMD_nav_start);
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_rijden);
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_sz_aan);
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_laden_niet);
}
void entry_S_OPLADEN () {
    i2cWrite(STOFZUIGER_ADDR, SZ_COMMAND, CMD_sz_uit);
    i2cWrite(NAV_ADDR, NAV_COMMAND, CMD_nav_stop);
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_stil);
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_sz_uit);
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_laden_wel);
} 

void entry_S_ERROR () {

}

void entry_S_NOOD () {
    i2cWrite(ACCU_ADDR, ACCU_COMMAND, CMD_accu_nood);
    i2cWrite(STOFZUIGER_ADDR, SZ_COMMAND, CMD_sz_uit);
    i2cWrite(NAV_ADDR, NAV_COMMAND, CMD_nav_stop);
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_stil);
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_nood);
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_sz_uit);
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_laden_niet);
}

void activity_S_IDLE(void){
    if (navStatus == 1) {
        i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_rijden);
        i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_error);
        setEvent(E_ERROR);
    }
    if (stofzuigerStatus == 1) {
        i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_sz_aan);
        i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_error);
        setEvent(E_ERROR);
    }
}

void activity_S_STOFZUIGEN(void){
    if (navStatus == 2) {
        i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_stil);
        i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_error);
        setEvent(E_ERROR);
    }
    if (stofzuigerStatus == 2) {
        i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_sz_uit);
        i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_error);
        setEvent(E_ERROR);
    }
}

void activity_S_OPLADEN(void){
    if (navStatus == 1) {
        i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_rijden);
        i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_error);
        setEvent(E_ERROR);
    }
    if (stofzuigerStatus == 1) {
        i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_sz_aan);
        i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_error);
        setEvent(E_ERROR);
    }
}

void activity_S_ERROR(void){
    DEBUG_LED_SetHigh();
    _delay_ms (500);
    DEBUG_LED_SetLow();
    _delay_ms (500);
}

void activity_S_NOOD(void){
    DEBUG_LED_SetHigh();
}


// ---------------------- I2C functies met controle ----------------------
void i2cWrite(uint8_t slaveAddr, uint8_t reg, uint8_t value){
    uint8_t packet[2] = {reg, value};
    uint16_t timeout = I2C_TIMEOUT;

    printf("I2C WRITE -> addr=0x%02X reg=0x%02X val=%u\n", slaveAddr, reg, value);

    TWI0_Write(slaveAddr, packet, 2);
    while(TWI0_IsBusy() && timeout--);

    i2c_host_error_t err = TWI0_ErrorGet();
    if(err != I2C_ERROR_NONE){
        printf("I2C WRITE ERROR addr=0x%02X reg=0x%02X error=%d\n", slaveAddr, reg, err);
        setEvent(E_ERROR);
    }
    if(timeout == 0) {
        printf("I2C write timeout (addr=0x%02X)\n", slaveAddr);
    }
}

uint8_t i2cRead(uint8_t slaveAddr, uint8_t reg){
    uint16_t timeout = I2C_TIMEOUT;
    uint8_t value = 0;

    printf("I2C READ  -> addr=0x%02X reg=0x%02X\n", slaveAddr, reg);

    TWI0_WriteRead(slaveAddr, &reg, 1, &value, 1);
    while(TWI0_IsBusy() && timeout--);

    i2c_host_error_t err = TWI0_ErrorGet();
    if(err != I2C_ERROR_NONE){
        printf("I2C READ ERROR addr=0x%02X reg=0x%02X error=%d\n", slaveAddr, reg, err);
        setEvent(E_ERROR);
        return 0xFF;
    }

    if(timeout == 0) {
        printf("I2C read timeout (addr=0x%02X)\n", slaveAddr);
    }
    else {
        printf("I2C READ  <- addr=0x%02X reg=0x%02X val=%u\n", slaveAddr, reg, value);
    }
    return value;
}

// ---------------------- Slave detectie ----------------------
bool i2cCheckSlave(uint8_t slaveAddr){
    uint8_t dummy = 0;
    TWI0_Write(slaveAddr, &dummy, 0);
    uint16_t timeout = I2C_TIMEOUT;
    while(TWI0_IsBusy() && timeout--);

    i2c_host_error_t err = TWI0_ErrorGet();
    if(err == I2C_ERROR_ADDR_NACK){
        printf("I2C SLAVE NOT FOUND at addr=0x%02X\n", slaveAddr);
        setEvent(E_ERROR);
        return false;
    }
    if(err != I2C_ERROR_NONE){
        printf("I2C SLAVE ERROR addr=0x%02X error=%d\n", slaveAddr, err);
        setEvent(E_ERROR);
        return false;
    }
    printf("I2C SLAVE FOUND at addr=0x%02X\n", slaveAddr);
    return true;
}

// ---------------------- Busfout check ----------------------
void i2cCheckBus(void){
    i2c_host_error_t err = TWI0_ErrorGet();
    if(err == I2C_ERROR_BUS_COLLISION){
        printf("I2C BUS ERROR: kortsluiting of meerdere masters!\n");
        setEvent(E_ERROR);
    }
}

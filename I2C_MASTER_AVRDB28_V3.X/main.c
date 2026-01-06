/*
#include "mcc_generated_files/system/system.h"
#include <util/delay.h>
#include <stdio.h>
#define F_CPU (4000000UL) 

// ---------------------- I2C adressen ----------------------
#define ACCU_ADDR        0x01
#define STOFZUIGER_ADDR  0x02
#define NAV_ADDR         0x03
#define HOME_ADDR        0x04

// ---------------------- Commands ----------------------
#define CMD_accu_nood           0
#define CMD_accu_stop_laden     1
#define CMD_accu_start_laden    2

#define CMD_sz_uit              0
#define CMD_sz_aan              1

#define CMD_nav_start           1
#define CMD_nav_stop            0

#define CMD_ha_nood             0
#define CMD_ha_rijden           1
#define CMD_ha_stil             2
#define CMD_ha_vast             3
#define CMD_ha_sz_aan           4
#define CMD_ha_sz_uit           5
#define CMD_ha_laden_wel        6
#define CMD_ha_laden_niet       7

// ---------------------- Registers per module ----------------------

// ACCU registers
typedef enum {
    ACCU_STATUS     = 0x00, // 0=nood, 1=accu aan, 2=accu uit, 3=gestart met laden, 4=gestopt met laden
    ACCU_COMMAND    = 0x01, // 0=nood, 1=stop laden, 2=start laden
    ACCU_DATA_IN    = 0x02, // accu percentage
    ACCU_DATA_OUT   = 0x03
} AccuReg;

// STOFZUIGER registers
typedef enum {
    SZ_STATUS       = 0x00, // 0=nood, 1=aan, 2=uit
    SZ_COMMAND      = 0x01, // 0=uit, 1=aan
    SZ_DATA_IN      = 0x02, // huidige stofzuiger percentage
    SZ_DATA_OUT     = 0x03  // gewenst stofzuiger percentage
} StofzuigerReg;

// NAVIGATIE registers
typedef enum {
    NAV_STATUS      = 0x00, // 0=nood, 1=rijden, 2=stil, 3=vast/opgepakt
    NAV_COMMAND     = 0x01, // 0=stoppen, 1=starten
    NAV_DATA_IN     = 0x02,
    NAV_DATA_OUT    = 0x03
} NaviReg;

// HOME ASSIST registers
typedef enum {
    HOME_STATUS     = 0x00, // 0=nood, 1=start, 2=stop, 3=opladen, 4=reset
    HOME_COMMAND    = 0x01, // 1=rijden, 4=stofzuiger aan, 5=stofzuiger uit...
    HOME_DATA_IN    = 0x02, // gewenst stofzuiger percentage
    HOME_DATA_OUT   = 0x03  // accu percentage
} HomeReg;

// ---------------------- State machine ----------------------
typedef enum { S_IDLE, S_STOFZUIGEN, S_OPLADEN, S_NOOD, S_ERROR } states;
typedef enum { E_NONE, E_START, E_STOP, E_START_OPLADEN, E_STOP_OPLADEN, E_NOOD, E_ERROR, E_RESET } events;

states currentState = S_IDLE;
states nextState = S_IDLE;
events currentEvent = E_NONE;

void setEvent( events newEvent){
    currentEvent = newEvent; 
}

void resetEvent(){
    currentEvent = E_NONE;
}

// ---------------------- I2C functies ----------------------
void i2cWrite(uint8_t slaveAddr, uint8_t reg, uint8_t value){
    uint8_t packet[2] = {reg, value};
    TWI0_Write(slaveAddr, packet, 2);
    while(TWI0_IsBusy());
}

uint8_t i2cRead(uint8_t slaveAddr, uint8_t reg){
    TWI0_Write(slaveAddr, &reg, 1);
    while(TWI0_IsBusy());

    uint8_t value = 0;
    TWI0_Read(slaveAddr, &value, 1);
    while(TWI0_IsBusy());

    return value;
}

void actionStartStofzuigen() {
    i2cWrite(STOFZUIGER_ADDR, SZ_COMMAND, CMD_sz_aan);      // motor aan
    i2cWrite(NAV_ADDR, NAV_COMMAND, CMD_nav_start);         // starten met rijden
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_rijden);       // HA stofzuiger rijdt
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_sz_aan);       // HA stofzuiger staat aan
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_laden_niet);   // HA stofzuiger laadt niet op
}
void actionStopStofzuigen() {
    i2cWrite(STOFZUIGER_ADDR, SZ_COMMAND, CMD_sz_uit);      // stofzuiger uit
    i2cWrite(NAV_ADDR, NAV_COMMAND, CMD_nav_stop);          // stoppen met rijden
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_stil);         // HA stofzuiger rijdt niet
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_sz_uit);       // HA stofzuiger staat uit
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_laden_niet);   // HA stofzuiger laadt niet op
}
void actionStartOpladen() {
    i2cWrite(STOFZUIGER_ADDR, SZ_COMMAND, CMD_sz_uit);      // stofzuiger uit
    i2cWrite(NAV_ADDR, NAV_COMMAND, CMD_nav_stop);          // stoppen met rijden
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_stil);         // HA stofzuiger rijdt niet
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_sz_uit);       // HA stofzuiger staat uit
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_laden_wel);    // HA stofzuiger laadt op
}
void actionStopOpladen() {
    i2cWrite(STOFZUIGER_ADDR, SZ_COMMAND, CMD_sz_uit);      // stofzuiger uit
    i2cWrite(NAV_ADDR, NAV_COMMAND, CMD_nav_stop);          // stoppen met rijden
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_stil);         // HA stofzuiger rijdt niet
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_sz_uit);       // HA stofzuiger staat uit
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_laden_niet);   // HA stofzuiger laadt niet op
}
void actionNood() {
    i2cWrite(ACCU_ADDR, ACCU_COMMAND, CMD_accu_nood);       // accu loskoppelen
    i2cWrite(STOFZUIGER_ADDR, SZ_COMMAND, CMD_sz_uit);      // stofzuiger uit
    i2cWrite(NAV_ADDR, NAV_COMMAND, CMD_nav_stop);          // stoppen met rijden
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_stil);         // HA stofzuiger rijdt niet
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_nood);         // HA stofzuiger nood
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_sz_uit);       // HA stofzuiger staat uit
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_laden_niet);   // HA stofzuiger laadt niet op
}
void actionError() {
    i2cWrite(STOFZUIGER_ADDR, SZ_COMMAND, CMD_sz_uit);      // stofzuiger uit
    i2cWrite(NAV_ADDR, NAV_COMMAND, CMD_nav_stop);          // stoppen met rijden
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_stil);         // HA stofzuiger rijdt niet
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_sz_uit);       // HA stofzuiger staat uit
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_laden_niet);   // HA stofzuiger laadt niet op
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_vast);         // HA stofzuiger zit vast/opgepakt
}




// ---------------------- MAIN ----------------------
int main(void){
    SYSTEM_Initialize();
    printf("Master gestart\n");

    while(1){
uC_LED_SetHigh();
        // ---------------------- Poll modules ----------------------
        uint8_t accuStatus        = i2cRead(ACCU_ADDR, ACCU_STATUS);
        uint8_t accuVal           = i2cRead(ACCU_ADDR, ACCU_DATA_IN);
        uint8_t stofzuigerStatus  = i2cRead(STOFZUIGER_ADDR, SZ_STATUS);
        uint8_t stofzuigerDataIn  = i2cRead(STOFZUIGER_ADDR, SZ_DATA_IN);
        uint8_t stofzuigerDataOut = i2cRead(STOFZUIGER_ADDR, SZ_DATA_OUT);
        uint8_t navStatus         = i2cRead(NAV_ADDR, NAV_STATUS);
        uint8_t homeStatus        = i2cRead(HOME_ADDR, HOME_STATUS);

        // ---------------------- Zet events ----------------------
        if(accuStatus == 0) setEvent(E_NOOD);
        if(accuStatus == 3) setEvent(E_START_OPLADEN);
        if(accuStatus == 4) setEvent(E_STOP_OPLADEN);
        
        if(stofzuigerStatus == 0) setEvent(E_NOOD);
        if(navStatus == 0) setEvent(E_NOOD);
        
        if(homeStatus == 0) setEvent(E_ERROR);
        if(homeStatus == 1) setEvent(E_START);
        if(homeStatus == 2) setEvent(E_STOP);
        if(homeStatus == 3) setEvent(E_START_OPLADEN);
        if(homeStatus == 4) setEvent(E_RESET);

        // ---------------------- State machine ----------------------
        switch(currentState){
            
            case S_IDLE:
                DEBUG_LED_SetLow();
                //i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_rijden); 
                printf("STATE: IDLE\n");
               // i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_laden_niet);
                if(currentEvent == E_START){
                    DEBUG_LED_SetHigh();
                    _delay_ms (200);
                    actionStartStofzuigen();
                    nextState = S_STOFZUIGEN;
                }
                else if(currentEvent == E_START_OPLADEN){
                    actionStartOpladen();
                    nextState = S_OPLADEN;
                }
                else if(currentEvent == E_NOOD){
                    actionNood();
                    nextState = S_NOOD;
                }
                break;

            case S_STOFZUIGEN:
                printf("STATE: VACUUM\n");
                DEBUG_LED_SetLow();
                //i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_nood);
                if(currentEvent == E_STOP){
                    actionStopStofzuigen();

                    nextState = S_IDLE;
                }
                else if(currentEvent == E_START_OPLADEN){
                    actionStartOpladen();
                    nextState = S_OPLADEN;
                }
                else if(currentEvent == E_ERROR){
                    actionError();
                    nextState = S_ERROR;
                }
                else if(currentEvent == E_NOOD){
                    actionNood();
                    nextState = S_NOOD;
                }
                break;

            case S_OPLADEN:
                printf("STATE: OPLADEN\n");

                if(currentEvent == E_STOP_OPLADEN){
                    actionStopOpladen();
                    nextState = S_IDLE;
                }
                else if(currentEvent == E_START){
                    actionStartStofzuigen();
                    nextState = S_STOFZUIGEN;
                }
                else if(currentEvent == E_NOOD){
                    actionNood();
                    nextState = S_NOOD;
                }
                break;

            case S_ERROR: 
               printf("STATE: ERROR\n");

                if(currentEvent == E_RESET){
                    actionStopStofzuigen();
                    nextState = S_IDLE;
                }
                break;

            case S_NOOD:
                printf("STATE: NOOD\n");
                break;
        }

        currentState = nextState;
        resetEvent();
        _delay_ms(150);
    }
}
*/
//------------------------------------------------------------------
/*
#include "mcc_generated_files/system/system.h"
#define F_CPU (4000000UL)
#include <util/delay.h>
#include <stdio.h>

// ---------------------- I2C adressen ----------------------
#define ACCU_ADDR        0x01
#define STOFZUIGER_ADDR  0x02
#define NAV_ADDR         0x03
#define HOME_ADDR        0x04
#define I2C_TIMEOUT 5000

// ---------------------- Commands ----------------------
#define CMD_accu_nood           10
#define CMD_accu_stop_laden     1 
#define CMD_accu_start_laden    2

#define CMD_sz_uit              0
#define CMD_sz_aan              1

#define CMD_nav_start           1
#define CMD_nav_stop            0

#define CMD_ha_nood             10
#define CMD_ha_rijden           1
#define CMD_ha_stil             2
#define CMD_ha_vast             3
#define CMD_ha_sz_aan           4
#define CMD_ha_sz_uit           5
#define CMD_ha_laden_wel        6
#define CMD_ha_laden_niet       7

// ---------------------- Registers per module ----------------------
// ACCU registers
typedef enum {
    ACCU_STATUS     = 0x00, // 0=nood, 1=accu aan, 2=accu uit, 3=gestart met laden, 4=gestopt met laden
    ACCU_COMMAND    = 0x01, // 0=nood, 1=stop laden, 2=start laden
    ACCU_DATA_IN    = 0x02, // accu percentage
    ACCU_DATA_OUT   = 0x03
} AccuReg;

// STOFZUIGER registers
typedef enum {
    SZ_STATUS       = 0x00, // 0=nood, 1=aan, 2=uit
    SZ_COMMAND      = 0x01, // 0=uit, 1=aan
    SZ_DATA_IN      = 0x02, // huidige stofzuiger percentage
    SZ_DATA_OUT     = 0x03  // gewenst stofzuiger percentage
} StofzuigerReg;

// NAVIGATIE registers
typedef enum {
    NAV_STATUS      = 0x00, // 0=nood, 1=rijden, 2=stil, 3=vast/opgepakt
    NAV_COMMAND     = 0x01, // 0=stoppen, 1=starten
    NAV_DATA_IN     = 0x02,
    NAV_DATA_OUT    = 0x03
} NaviReg;

// HOME ASSIST registers
typedef enum {
    HOME_STATUS     = 0x00, // 0=nood, 1=start, 2=stop, 3=opladen, 4=reset
    HOME_COMMAND    = 0x01, // 1=rijden, 4=stofzuiger aan, 5=stofzuiger uit...
    HOME_DATA_IN    = 0x02, // gewenst stofzuiger percentage
    HOME_DATA_OUT   = 0x03  // accu percentage
} HomeReg;

uint8_t accuStatus        = 0;
uint8_t accuVal           = 0;
uint8_t stofzuigerStatus  = 0;
uint8_t stofzuigerDataIn  = 0;
uint8_t stofzuigerDataOut = 0;
uint8_t navStatus         = 0;
uint8_t homeStatus        = 0;

typedef enum { S_IDLE, S_STOFZUIGEN, S_OPLADEN, S_NOOD, S_ERROR } states;
typedef enum { E_NONE, E_START, E_STOP, E_START_OPLADEN, E_STOP_OPLADEN, E_NOOD, E_ERROR, E_RESET } events;
typedef enum{F_ENTRY, F_ACTIVITY, F_EXIT} flow;

states currentState = S_IDLE;
events currentEvent = E_NONE;
flow currentFlow = F_ENTRY;
states nextState = S_IDLE;



void setEvent(events newEvent); // Set currentEvent naar newEvent
void resetEvent(); // Reset currentEvent naar E_NOPRESS
void entry_S_IDLE (); 
void entry_S_STOFZUIGEN (); 
void entry_S_OPLADEN (); 
void entry_S_NOOD ();
void entry_S_ERROR ();

// ---------------------- I2C functies ----------------------
void i2cWrite(uint8_t slaveAddr, uint8_t reg, uint8_t value);
uint8_t i2cRead(uint8_t slaveAddr, uint8_t reg);


int main(void)
{
    SYSTEM_Initialize();
    //uC_LED_SetLow();
    //DEBUG_LED_SetLow();
    printf("master gestart\n");
    while(1)
    {
   
        uC_LED_SetHigh();
        //DEBUG_LED_SetHigh();
        // ------------------------POLL MODULES------------------------
        accuStatus        = i2cRead(ACCU_ADDR, ACCU_STATUS);
        accuVal           = i2cRead(ACCU_ADDR, ACCU_DATA_IN);
        stofzuigerStatus  = i2cRead(STOFZUIGER_ADDR, SZ_STATUS);
        stofzuigerDataIn  = i2cRead(STOFZUIGER_ADDR, SZ_DATA_IN);
        stofzuigerDataOut = i2cRead(STOFZUIGER_ADDR, SZ_DATA_OUT);
        navStatus         = i2cRead(NAV_ADDR, NAV_STATUS);
        homeStatus        = i2cRead(HOME_ADDR, HOME_STATUS);
        
        //---------------------SET EVENTS---------------------------------
        //---NOOD---
        if(accuStatus == 10 || navStatus == 10 || stofzuigerStatus == 10 || homeStatus == 10) {
            setEvent(E_NOOD);
        }
        else {
            //---START---
            if(homeStatus == 1) setEvent(E_START);
            //---STOP---
            if(homeStatus == 2 || navStatus == 3) setEvent(E_STOP);
            //---START LADEN---
            if(homeStatus == 3 || accuStatus == 3) setEvent(E_START_OPLADEN);
            //---STOP LADEN---
            if(accuStatus == 4) setEvent(E_STOP_OPLADEN);
            //---ERROR---
        
            //--RESET---
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
                    case E_NOOD:
                        nextState = S_NOOD;
                        resetEvent();
                        break;
                    default:
                        break;
                }
                switch(currentFlow){
                    case F_ENTRY:
                        printf("idle entry\n");
                        entry_S_IDLE();
                        currentFlow = F_ACTIVITY;
                    case F_ACTIVITY:
                        
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
                        printf("stofzuigen entry\n");
                        entry_S_STOFZUIGEN();
                        currentFlow = F_ACTIVITY;
                    case F_ACTIVITY:
                        
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
                    case E_NOOD:
                        nextState = S_NOOD;
                        resetEvent();
                        break;
                    default:
                        break;
                }
                switch(currentFlow){
                    case F_ENTRY:
                        printf("opladen entry\n");
                        entry_S_OPLADEN();
                        currentFlow = F_ACTIVITY;
                    case F_ACTIVITY:
                        
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
                        printf("error entry\n");
                        entry_S_ERROR();
                        currentFlow = F_ACTIVITY;
                    case F_ACTIVITY:
                        DEBUG_LED_SetHigh();
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
                switch(currentEvent) {
                    default:
                        break;
                }
                switch(currentFlow){
                    case F_ENTRY:
                        printf("nood entry\n");
                        entry_S_NOOD();
                        currentFlow = F_ACTIVITY;
                    case F_ACTIVITY:
                        DEBUG_LED_SetHigh();
                        _delay_ms (500);
                        DEBUG_LED_SetLow();
                        _delay_ms (500);
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

void setEvent(events newEvent){
    currentEvent = newEvent;
}

void resetEvent(){
    currentEvent = E_NONE;
}
void entry_S_IDLE () {
    i2cWrite(STOFZUIGER_ADDR, SZ_COMMAND, CMD_sz_uit);      // stofzuiger uit
    i2cWrite(NAV_ADDR, NAV_COMMAND, CMD_nav_stop);          // stoppen met rijden
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_stil);         // HA stofzuiger rijdt niet
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_sz_uit);       // HA stofzuiger staat uit
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_laden_niet);   // HA stofzuiger laadt niet op
    
    if (navStatus == 3) {
        DEBUG_LED_SetHigh();
        i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_vast);         // HA stofzuiger zit vast/opgepakt
    }  
}
void entry_S_STOFZUIGEN () {
    i2cWrite(STOFZUIGER_ADDR, SZ_COMMAND, CMD_sz_aan);      // stofzuiger aan
    i2cWrite(NAV_ADDR, NAV_COMMAND, CMD_nav_start);         // starten met rijden
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_rijden);       // HA stofzuiger rijdt
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_sz_aan);       // HA stofzuiger staat aan
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_laden_niet);   // HA stofzuiger laadt niet op
}
void entry_S_OPLADEN () {
    i2cWrite(STOFZUIGER_ADDR, SZ_COMMAND, CMD_sz_uit);      // stofzuiger uit
    i2cWrite(NAV_ADDR, NAV_COMMAND, CMD_nav_stop);          // stoppen met rijden
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_stil);         // HA stofzuiger rijdt niet
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_sz_uit);       // HA stofzuiger staat uit
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_laden_wel);    // HA stofzuiger laadt op
} 
void entry_S_NOOD () {
    i2cWrite(ACCU_ADDR, ACCU_COMMAND, CMD_accu_nood);       // accu loskoppelen
    i2cWrite(STOFZUIGER_ADDR, SZ_COMMAND, CMD_sz_uit);      // stofzuiger uit
    i2cWrite(NAV_ADDR, NAV_COMMAND, CMD_nav_stop);          // stoppen met rijden
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_stil);         // HA stofzuiger rijdt niet
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_nood);         // HA stofzuiger nood
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_sz_uit);       // HA stofzuiger staat uit
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_laden_niet);   // HA stofzuiger laadt niet op
}
void entry_S_ERROR () {
    
}

// ---------------------- I2C functies ----------------------
void i2cWrite(uint8_t slaveAddr, uint8_t reg, uint8_t value){
    uint8_t packet[2] = {reg, value};
    uint16_t timeout = I2C_TIMEOUT;

    printf("I2C WRITE -> addr=0x%02X reg=0x%02X val=%u\n",
           slaveAddr, reg, value);

    TWI0_Write(slaveAddr, packet, 2);
    while(TWI0_IsBusy() && timeout--);

    if (timeout == 0)
        printf("I2C write timeout (addr=0x%02X)\n", slaveAddr);
}


uint8_t i2cRead(uint8_t slaveAddr, uint8_t reg){
    uint16_t timeout = I2C_TIMEOUT;
    uint8_t value = 0;

    printf("I2C READ  -> addr=0x%02X reg=0x%02X\n",
           slaveAddr, reg);

    TWI0_Write(slaveAddr, &reg, 1);
    while(TWI0_IsBusy() && timeout--);

    if (timeout == 0) {
        printf("I2C reg write timeout (addr=0x%02X)\n", slaveAddr);
        return value;
    }

    timeout = I2C_TIMEOUT;
    TWI0_Read(slaveAddr, &value, 1);
    while(TWI0_IsBusy() && timeout--);

    if (timeout == 0)
        printf("I2C read timeout (addr=0x%02X)\n", slaveAddr);
    else
        printf("I2C READ  <- addr=0x%02X reg=0x%02X val=%u\n",
               slaveAddr, reg, value);

    return value;
}
*/
//-------------------------------------------------------------------
#include "mcc_generated_files/system/system.h"
#define F_CPU (4000000UL)
#include <util/delay.h>
#include <stdio.h>

// ---------------------- I2C adressen ----------------------
#define ACCU_ADDR        0x01
#define STOFZUIGER_ADDR  0x02
#define NAV_ADDR         0x03
#define HOME_ADDR        0x04
#define I2C_TIMEOUT 5000

// ---------------------- Commands ----------------------
#define CMD_accu_nood           10
#define CMD_accu_stop_laden     1 
#define CMD_accu_start_laden    2

#define CMD_sz_uit              0
#define CMD_sz_aan              1

#define CMD_nav_start           1
#define CMD_nav_stop            0

#define CMD_ha_nood             10
#define CMD_ha_rijden           1
#define CMD_ha_stil             2
#define CMD_ha_vast             3
#define CMD_ha_sz_aan           4
#define CMD_ha_sz_uit           5
#define CMD_ha_laden_wel        6
#define CMD_ha_laden_niet       7

// ---------------------- Registers per module ----------------------
// ACCU registers
typedef enum {
    ACCU_STATUS     = 0x00, // 0=nood, 1=accu aan, 2=accu uit, 3=gestart met laden, 4=gestopt met laden
    ACCU_COMMAND    = 0x01, // 0=nood, 1=stop laden, 2=start laden
    ACCU_DATA_IN    = 0x02, // accu percentage
    ACCU_DATA_OUT   = 0x03
} AccuReg;

// STOFZUIGER registers
typedef enum {
    SZ_STATUS       = 0x00, // 0=nood, 1=aan, 2=uit
    SZ_COMMAND      = 0x01, // 0=uit, 1=aan
    SZ_DATA_IN      = 0x02, // huidige stofzuiger percentage
    SZ_DATA_OUT     = 0x03  // gewenst stofzuiger percentage
} StofzuigerReg;

// NAVIGATIE registers
typedef enum {
    NAV_STATUS      = 0x00, // 0=nood, 1=rijden, 2=stil, 3=vast/opgepakt
    NAV_COMMAND     = 0x01, // 0=stoppen, 1=starten
    NAV_DATA_IN     = 0x02,
    NAV_DATA_OUT    = 0x03
} NaviReg;

// HOME ASSIST registers
typedef enum {
    HOME_STATUS     = 0x00, // 0=nood, 1=start, 2=stop, 3=opladen, 4=reset
    HOME_COMMAND    = 0x01, // 1=rijden, 4=stofzuiger aan, 5=stofzuiger uit...
    HOME_DATA_IN    = 0x02, // gewenst stofzuiger percentage
    HOME_DATA_OUT   = 0x03  // accu percentage
} HomeReg;

uint8_t accuStatus        = 0;
uint8_t accuVal           = 0;
uint8_t stofzuigerStatus  = 0;
uint8_t stofzuigerDataIn  = 0;
uint8_t stofzuigerDataOut = 0;
uint8_t navStatus         = 0;
uint8_t homeStatus        = 0;

typedef enum { S_IDLE, S_STOFZUIGEN, S_OPLADEN, S_NOOD, S_ERROR } states;
typedef enum { E_NONE, E_START, E_STOP, E_START_OPLADEN, E_STOP_OPLADEN, E_NOOD, E_ERROR, E_RESET } events;
typedef enum{F_ENTRY, F_ACTIVITY, F_EXIT} flow;

states currentState = S_IDLE;
events currentEvent = E_NONE;
flow currentFlow = F_ENTRY;
states nextState = S_IDLE;

void setEvent(events newEvent);
void resetEvent();
void entry_S_IDLE (); 
void entry_S_STOFZUIGEN (); 
void entry_S_OPLADEN (); 
void entry_S_NOOD ();
void entry_S_ERROR ();

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
    printf("master gestart\n");
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
        accuVal           = i2cRead(ACCU_ADDR, ACCU_DATA_IN);
        stofzuigerStatus  = i2cRead(STOFZUIGER_ADDR, SZ_STATUS);
        stofzuigerDataIn  = i2cRead(STOFZUIGER_ADDR, SZ_DATA_IN);
        stofzuigerDataOut = i2cRead(STOFZUIGER_ADDR, SZ_DATA_OUT);
        navStatus         = i2cRead(NAV_ADDR, NAV_STATUS);
        homeStatus        = i2cRead(HOME_ADDR, HOME_STATUS);

        //---------------------SET EVENTS---------------------------------
        if(accuStatus == 10 || navStatus == 10 || stofzuigerStatus == 10 || homeStatus == 10) {
            setEvent(E_NOOD);
        }
        else {
            if(homeStatus == 1) setEvent(E_START);
            if(homeStatus == 2 || navStatus == 3) setEvent(E_STOP);
            if(homeStatus == 3 || accuStatus == 3) setEvent(E_START_OPLADEN);
            if(accuStatus == 4) setEvent(E_STOP_OPLADEN);
            if(homeStatus == 4) setEvent(E_RESET);
        }

        //-------------------STATE MACHINE----------------------------------
        switch(currentState) {
            case S_IDLE:
                switch(currentEvent) {
                    case E_START: nextState = S_STOFZUIGEN; resetEvent(); break;
                    case E_START_OPLADEN: nextState = S_OPLADEN; resetEvent(); break;
                    case E_NOOD: nextState = S_NOOD; resetEvent(); break;
                    default: break;
                }
                switch(currentFlow){
                    case F_ENTRY: printf("idle entry\n"); entry_S_IDLE(); currentFlow = F_ACTIVITY;
                    case F_ACTIVITY: if(nextState != currentState) currentFlow = F_EXIT; break;
                    case F_EXIT: currentFlow = F_ENTRY; break;
                }
                break;
            case S_STOFZUIGEN:
                switch(currentEvent) {
                    case E_STOP: nextState = S_IDLE; resetEvent(); break;
                    case E_START_OPLADEN: nextState = S_OPLADEN; resetEvent(); break;
                    case E_ERROR: nextState = S_ERROR; resetEvent(); break;
                    case E_NOOD: nextState = S_NOOD; resetEvent(); break;            
                    default: break;
                }
                switch(currentFlow){
                    case F_ENTRY: printf("stofzuigen entry\n"); entry_S_STOFZUIGEN(); currentFlow = F_ACTIVITY;
                    case F_ACTIVITY: if(nextState != currentState) currentFlow = F_EXIT; break;
                    case F_EXIT: currentFlow = F_ENTRY; break;
                }
                break;
            case S_OPLADEN:
                switch(currentEvent) {
                    case E_START: nextState = S_STOFZUIGEN; resetEvent(); break;
                    case E_STOP_OPLADEN: nextState = S_IDLE; resetEvent(); break;
                    case E_NOOD: nextState = S_NOOD; resetEvent(); break;
                    default: break;
                }
                switch(currentFlow){
                    case F_ENTRY: printf("opladen entry\n"); entry_S_OPLADEN(); currentFlow = F_ACTIVITY;
                    case F_ACTIVITY: if(nextState != currentState) currentFlow = F_EXIT; break;
                    case F_EXIT: currentFlow = F_ENTRY; break;
                }
                break;
            case S_ERROR:
                switch(currentEvent) {
                    case E_RESET: nextState = S_IDLE; resetEvent(); break;
                    default: break;
                }
                switch(currentFlow){
                    case F_ENTRY: printf("error entry\n"); entry_S_ERROR(); currentFlow = F_ACTIVITY;
                    case F_ACTIVITY: if(nextState != currentState) currentFlow = F_EXIT; break;
                    case F_EXIT: currentFlow = F_ENTRY; break;
                }
                break;
            case S_NOOD:
                switch(currentFlow){
                    case F_ENTRY: printf("nood entry\n"); entry_S_NOOD(); currentFlow = F_ACTIVITY;
                    case F_ACTIVITY: _delay_ms (500); _delay_ms (500); if(nextState != currentState) currentFlow = F_EXIT; break;
                    case F_EXIT: currentFlow = F_ENTRY; break;
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
    if (navStatus == 3) i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_vast);
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
void entry_S_NOOD () {
    i2cWrite(ACCU_ADDR, ACCU_COMMAND, CMD_accu_nood);
    i2cWrite(STOFZUIGER_ADDR, SZ_COMMAND, CMD_sz_uit);
    i2cWrite(NAV_ADDR, NAV_COMMAND, CMD_nav_stop);
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_stil);
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_nood);
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_sz_uit);
    i2cWrite(HOME_ADDR, HOME_COMMAND, CMD_ha_laden_niet);
}
void entry_S_ERROR () {}

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
    }
    if(timeout == 0)
        printf("I2C write timeout (addr=0x%02X)\n", slaveAddr);
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
        return 0xFF;
    }

    if(timeout == 0)
        printf("I2C read timeout (addr=0x%02X)\n", slaveAddr);
    else
        printf("I2C READ  <- addr=0x%02X reg=0x%02X val=%u\n", slaveAddr, reg, value);

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
        return false;
    }
    if(err != I2C_ERROR_NONE){
        printf("I2C SLAVE ERROR addr=0x%02X error=%d\n", slaveAddr, err);
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

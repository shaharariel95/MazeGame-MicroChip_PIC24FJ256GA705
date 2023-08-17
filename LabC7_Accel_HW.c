/*
 * File:   LabC5.c
 * Author: Amit
 *
 * Created on May 21, 2022
 * Fixed Jan 2023
 */

#include <stdlib.h>
#include <stdio.h>

#include "System/system.h"
#include "System/delay.h"
#include "oledDriver/oledC.h"
#include "oledDriver/oledC_colors.h"
#include "oledDriver/oledC_shapes.h"


#include "i2cDriver/i2c1_driver.h"
#include "Accel_i2c.h"


void User_Initialize(void)
{
}


void errorStop(char *msg)
{
    oledC_DrawString(0, 20, 2, 2, msg, OLEDC_COLOR_DARKRED);

    for (;;)
        ;
}

/*
                         Main application
 */
int main(void)
{
    unsigned char id = 0;
    I2Cerror rc;
    int x, y, z;
    char xx[]="     ";
    char yy[]="     ";
    char zz[]="     ";
    unsigned char xyz[6] = {0};
    
    SYSTEM_Initialize();

     oledC_setBackground(OLEDC_COLOR_SKYBLUE);
     oledC_clearScreen();

    i2c1_driver_driver_close();
    i2c1_open();

    rc = i2cReadSlaveRegister(0x3A, 0, &id);

    if (rc == OK)
        if(id==0xE5)
            oledC_DrawString(10, 10, 2, 2, "ADXL345", OLEDC_COLOR_BLACK);
        else
            errorStop("Acc!Found");
    else
        errorStop("I2C Error");

    rc = i2cWriteSlave(0x3A, 0x2D, 8);
    
    
    oledC_DrawString(2, 30, 2, 2, "X:", OLEDC_COLOR_BLACK);
    oledC_DrawString(2, 50, 2, 2, "Y:", OLEDC_COLOR_BLACK);
    oledC_DrawString(2, 70, 2, 2, "Z:", OLEDC_COLOR_BLACK);
    
    for (;;) {
        int i;
        for (i=0 ; i<6 ; ++i) {
            rc=i2cReadSlaveRegister(0x3A, 0x32+i, &xyz[i]);
            DELAY_microseconds(5);            
        }
        
        x = xyz[0]+xyz[1]*256;  //2xbytes ==> word
        y = xyz[2]+xyz[3]*256;
        z = xyz[4]+xyz[5]*256;
        
        sprintf(xx, "%d", x);   //Make it a string
        sprintf(yy, "%d", y);
        sprintf(zz, "%d", z);
        
        //  === Display Axes Acceleration   ====================
        oledC_DrawString(26, 30, 2, 2, xx, OLEDC_COLOR_BLACK);
        oledC_DrawString(26, 50, 2, 2, yy, OLEDC_COLOR_BLACK);
        oledC_DrawString(26, 70, 2, 2, zz, OLEDC_COLOR_BLACK);
        DELAY_milliseconds(500);
        
        //  === Erase Axes Acceleration   ====================
        oledC_DrawString(26, 30, 2, 2, xx, OLEDC_COLOR_SKYBLUE);
        oledC_DrawString(26, 50, 2, 2, yy, OLEDC_COLOR_SKYBLUE);
        oledC_DrawString(26, 70, 2, 2, zz, OLEDC_COLOR_SKYBLUE);        
    }
}
/**
 End of File
*/
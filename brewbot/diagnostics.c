///////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2007, Matthew Pratt, All Rights Reserved.
//
// Authors: Matthew Pratt
//
// Date:  9 Feb 2011
//
///////////////////////////////////////////////////////////////////////////////
#include <stdlib.h>
#include "menu.h"
#include "heat.h"
#include "buttons.h"
#include "brewbot.h"
#include "lcd.h"
#include "crane.h"

static float heat_target = 66.0f;
static int   heat_duty   = 50;

//----------------------------------------------------------------------------
static void diag_heat(int initializing)
{
    if (initializing)
    {
	lcd_text(0, 1, "Heat diagnostic");
	lcd_text(0, 2, "/\\ \\/ = target temp");
	lcd_text(0, 3, "< > = duty cycle");
	setHeatTargetTemperature(heat_target);
	setHeatDutyCycle(heat_duty);
	startHeatTask();
    }
    else
    {
	stopHeatTask();
    }
}

static int diag_heat_key(unsigned char key)
{
    if ((key & KEY_PRESSED) == 0)
	return 1;

    if (key & KEY_UP)
    {
	heat_target += 0.5;
    }
    else if (key & KEY_DOWN)
    {
	heat_target -= 0.5;
    }
    else if (key & KEY_LEFT)
    {
	if (heat_duty <= 10)
	    heat_duty--;
	else
	    heat_duty -= 10;
    }
    else if (key & KEY_RIGHT)
    {
	if (heat_duty < 10)
	    heat_duty++;
	else
	    heat_duty += 10;
    }
    setHeatTargetTemperature(heat_target);
    setHeatDutyCycle(heat_duty);
    return 1; // signal that we consume the left key, double click needed to exit
}
//----------------------------------------------------------------------------
static void diag_crane(int initializing)
{
    if (initializing)
    {
	lcd_text(0, 1, "Crane diagnostic");
	lcd_text(0, 2, "/\\ \\/ = up down");
	lcd_text(0, 3, "< > = left right");
    }
    else
    {
	craneStop();
    }
}

static int diag_crane_key(unsigned char key)
{
    if (upKeyPressed(key))
    {
	craneMove(DIRECTION_UP);
    }
    else if (downKeyPressed(key))
    {
	craneMove(DIRECTION_DOWN);
    }
    else if (leftKeyPressed(key))
    {
	craneMove(DIRECTION_LEFT);
    }
    else if (rightKeyPressed(key))
    {
	craneMove(DIRECTION_RIGHT);
    }

    return 1;
}
//----------------------------------------------------------------------------
static void diag_mash(int initializing)
{
    if (initializing)
    {
	STIRRER_DDR = 1;
	lcd_text(0, 1, "Stirrer diagnostic");
	lcd_text(0, 2, "/\\ \\/ = on / off");
	lcd_text(0, 3, "< > = Back / pulse");
    }
}

static int diag_mash_key(unsigned char key)
{
    if (upKeyPressed(key))
    {
	outputOn(STIRRER);
    }
    else if (downKeyPressed(key))
    {
	outputOff(STIRRER);
    }
    else if (leftKeyPressed(key))
    {
	outputOff(STIRRER);
    }
    else if (key & KEY_RIGHT)
    {
	if (key & KEY_PRESSED)
	    outputOn(STIRRER);
	else
	    outputOff(STIRRER);
    }

    return 1;
}
//----------------------------------------------------------------------------
static void solenoid_pulse(int initializing)
{
    SOLENOID_DDR = 1;
}

static int solenoid_pulse_key(unsigned char key)
{
    if (upKeyPressed(key))
    {
	outputOn(SOLENOID);
    }
    else if (downKeyPressed(key))
    {
	outputOff(SOLENOID);
    }
    else if (leftKeyPressed(key))
    {
	outputOff(SOLENOID);
    }
    else if (key & KEY_RIGHT)
    {
	if (key & KEY_PRESSED)
	    outputOn(SOLENOID);
	else
	    outputOff(SOLENOID);
    }
    return 1;
}
//----------------------------------------------------------------------------

struct menu diag_menu[] =
 {
     {"Heat",           NULL,              diag_heat,      diag_heat_key},
     {"Crane",          NULL,              diag_crane,     diag_crane_key},
     {"Mash stirrer",   NULL,              diag_mash,      diag_mash_key},
     {"Solenoid",       NULL,              solenoid_pulse, solenoid_pulse_key},
#if 0
    {"Heat basic",      diag_heat_basic,     NULL,       diag_key},
    {"Level sensors",   NULL,                diag_adc,   diag_key},
    {"Hops",            NULL,                diag_hops,  NULL},
#endif
    {NULL, NULL, NULL, NULL}
};



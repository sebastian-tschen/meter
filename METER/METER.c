/*
 METER.c
 2016-02-10
 Public Domain
 */

#include <stdio.h>
#include <stdlib.h>

#include <pigpiod_if2.h>

#include "METER.h"

/* PRIVATE ---------------------------------------------------------------- */

struct _METER_s
{
    int pi;
    int meterGPIO;
    METER_CB_t cb;
    int cb_id;
    int lev;
    int oldState;
    uint32_t meter_value;
    uint32_t last_tick;
    unsigned glitch;
};


static void _cb(
                int pi, unsigned gpio, unsigned level, uint32_t tick, void *user)
{
    METER_t *self=user;
    
    if (level != PI_TIMEOUT)
    {
        self->meter_value++;
        (self->cb)(self->meter_value,tick);
    }
}

/* PUBLIC ----------------------------------------------------------------- */

METER_t *METER(int pi, int meterGPIO, uint32_t start_meter_value, unsigned glitch, METER_CB_t cb_func)
{
    METER_t *self;
    
    self = malloc(sizeof(METER_t));
    
    if (!self) return NULL;
    
    self->pi = pi;
    self->meterGPIO = meterGPIO;
    self->meter_value = start_meter_value;
    self->cb = cb_func;
    self->lev=0;
    self->last_tick=0;
    self->glitch=glitch;
    
    set_mode(pi, meterGPIO, PI_INPUT);
    
    /* pull up is needed as encoder common is grounded */
    
    set_pull_up_down(pi, meterGPIO, PI_PUD_OFF);
    
    set_glitch_filter(pi, meterGPIO, self->glitch);
    
    /* monitor encoder level changes */
    
    self->cb_id = callback_ex(pi, meterGPIO, RISING_EDGE, _cb, self);
    
    return self;
}

void METER_cancel(METER_t *self)
{
    if (self)
    {
        
        if (self->cb_id >= 0)
        {
            callback_cancel(self->cb_id);
            self->cb_id = -1;
        }
        set_glitch_filter(self->pi, self->meterGPIO, 0);
        
        free(self);
    }
}

uint32_t METER_get_position(METER_t *self)
{
    return self->meter_value;
}

void METER_set_position(METER_t *self, uint32_t value)
{
    self->meter_value=value;
}

void METER_set_glitch_filter(METER_t *self, int glitch)
{
    if (glitch >= 0)
    {
        if (self->glitch != glitch)
        {
            self->glitch = glitch;
            set_glitch_filter(self->pi, self->meterGPIO, glitch);
        }
    }
}

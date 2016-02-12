/*
 METER.h
 2015-11-18
 Public Domain
 */

#ifndef METER_H
#define METER_H

typedef void (*METER_CB_t)(uint32_t,uint32_t);

struct _METER_s;

typedef struct _METER_s METER_t;

#define METER_MODE_DETENT 0
#define METER_MODE_STEP   1

/*
 
 METER starts a rotary encoder on Pi pi with GPIO gpioA,
 GPIO gpioB, mode mode, and callback cb_func.  The mode
 determines whether the four steps in each detent are
 reported as changes or just the detents.
 
 If cb_func in not null it will be called at each position
 change with the new position.
 
 The current position can be read with METER_get_position and
 set with METER_set_position.
 
 Mechanical encoders may suffer from switch bounce.
 METER_set_glitch_filter may be used to filter out edges
 shorter than glitch microseconds.  By default a glitch
 filter of 1000 microseconds is used.
 
 At program end the rotary encoder should be cancelled using
 METER_cancel.  This releases system resources.
 
 */

METER_t *METER                   (int pi,
                                  int gpioB,
                                  uint32_t start_meter_value,
                                  uint32_t min_tick_difference,
                                  METER_CB_t cb_func);

void   METER_cancel            (METER_t *renc);

void   METER_set_glitch_filter (METER_t *renc, int glitch);

void   METER_set_position      (METER_t *renc, uint32_t position);

uint32_t    METER_get_position (METER_t *renc);

#endif

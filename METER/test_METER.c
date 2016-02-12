/*
 test_METER.c
 2015-11-18
 Public Domain
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include <pigpiod_if2.h>

#include "METER.h"

#include <rrd.h>

/*
 
 REQUIRES
 
 A rotary encoder contacts A and B connected to separate GPIO and
 the common contact connected to Pi ground.
 
 TO BUILD
 
 gcc -Wall -pthread -o METER test_METER.c METER.c -lpigpiod_if2
 
 TO RUN
 
 sudo pigpiod # if the daemon is not already running
 
 ./METER -aGPIO -bGPIO
 
 For option help
 
 ./METER -?
 
 */

void fatal(char *fmt, ...)
{
    char buf[128];
    va_list ap;
    
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    
    fprintf(stderr, "%s\n", buf);
    
    fflush(stderr);
    
    exit(EXIT_FAILURE);
}

void usage()
{
    fprintf(stderr, "\n" \
            "Usage: METER [OPTION] ...\n" \
            "   -a value, gpio A, 0-31,                  default None\n" \
            "   -v value, startValue, uint32,            default 0\n" \
            "   -g value, glitch filter setting, 0-5000, default 1\n" \
            "   -s value, run seconds, >=0 (0=forever),  default 0\n" \
            "   -t value, seconds between rrd-writes     default 10\n"\
            "   -f value, rrd file to write to           default NULL\n"\
            "   -h string, host name,                    default NULL\n" \
            "   -p value, socket port, 1024-32000,       default 8888\n" \
            "EXAMPLE\n" \
            "METER -a10 -b12\n" \
            "   Read a rotary encoder connected to GPIO 10/12.\n\n");
}

int optGpio = -1;
int optGlitch = 1;
uint32_t optStartMeterValue=0;
int optSeconds = 0;
int optRRDSeconds = 10000;
char *optRRDFile = NULL;
char *optHost   = NULL;
char *optPort   = NULL;

uint32_t lastTick=0;

static uint64_t getNum(char *str, int *err)
{
    uint64_t val;
    char *endptr;
    
    *err = 0;
    val = strtoll(str, &endptr, 0);
    if (*endptr) {*err = 1; val = -1;}
    return val;
}

static void initOpts(int argc, char *argv[])
{
    int opt, err, i;
    
    while ((opt = getopt(argc, argv, "a:v:f:g:t:s:h:p:")) != -1)
    {
        switch (opt)
        {
            case 'a':
                i = getNum(optarg, &err);
                if ((i >= 0) && (i <= 31)) optGpio = i;
                else fatal("invalid -a option (%s)", optarg);
                break;
                
            case 'v':
                optStartMeterValue = getNum(optarg, &err);
                break;
                
            case 'g':
                i = getNum(optarg, &err);
                if ((i >= 0) && (i <= 5000)) optGlitch = i;
                else fatal("invalid -g option (%s)", optarg);
                break;
                
            case 's':
                i = getNum(optarg, &err);
                if (i >= 0) optSeconds = (i*1000);
                else fatal("invalid -s option (%s)", optarg);
                break;
            case 'f':
                optRRDFile = malloc(sizeof(optarg) +1);
                if (optRRDFile) strcpy(optRRDFile, optarg);
                break;
            case 't':
                i = getNum(optarg, &err);
                if (i >= 0) optRRDSeconds = i;
                else fatal("invalid -t option (%s)", optarg);
                break;
                
            case 'h':
                optHost = malloc(sizeof(optarg)+1);
                if (optHost) strcpy(optHost, optarg);
                break;
                
            case 'p':
                optPort = malloc(sizeof(optarg)+1);
                if (optPort) strcpy(optPort, optarg);
                break;
                
            default: /* '?' */
                usage();
                exit(-1);
        }
    }
}
void write_rrd(uint32_t pos, uint32_t tick){
    
    char *str = malloc(sizeof(char) * 1024);
    sprintf(str, "N:%2uid", pos);
    char *data=malloc(strlen(str)+1);
    strcpy(data,str);
    
    char *updateparams[] = {
        "rrdupdate",
        optRRDFile,
        data,
        NULL
    };
    
    optind = opterr = 0;
    rrd_clear_error();
    rrd_update(3, updateparams);
    
    
}


void cbf(uint32_t pos, uint32_t tick)
{
    if (tick<lastTick || (tick - lastTick)<optRRDSeconds){
        
        write_rrd(pos,tick);
    }
    lastTick = tick;
    printf("%d\n", pos);
}

int main(int argc, char *argv[])
{
    int pi;
    METER_t *renc;
    
    initOpts(argc, argv);
    
    if ((optGpio < 0) )
    {
        fprintf(stderr, "You must specify a gpio.\n");
        exit(0);
    }
    
    pi = pigpio_start(optHost, optPort); /* Connect to Pi. */
    
    if (pi >= 0)
    {
        renc = METER(pi, optGpio, optStartMeterValue, optGlitch, cbf);
        
        if (optSeconds) sleep(optSeconds);
        else while(1) {
            sleep(60);
            fflush(stdout);
        }
        
        METER_cancel(renc);
        
        pigpio_stop(pi);
    }
    return 0;
}


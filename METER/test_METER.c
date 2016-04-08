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
#include <sys/time.h>

#include <netdb.h>
#include <netinet/in.h>

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
            "   -t value, seconds between rrd-writes     default 60\n"\
            "   -f value, rrd file to write to           default NULL\n"\
            "   -d value, seconds between db-writes      default 3600\n"\
            "   -m mysql-connection-string               default NULL\n"\
            "   -h string, host name,                    default NULL\n" \
            "   -p value, socket port, 1024-32000,       default 8888\n" \
            "   -r string, rrd server host name,                    default NULL\n" \
            "   -b value, rrd socket port, 1024-32000,       default 13900\n" \
            "EXAMPLE\n" \
            "METER -a10 -b12\n" \
            "   Read a rotary encoder connected to GPIO 10/12.\n\n");
}

int optGpio = -1;
int optGlitch = 1;
uint32_t optStartMeterValue=0;
int optSeconds = 0;
int optRRDSeconds = 60;
int optDBSeconds = 3600;
char *optRRDFile = NULL;
char *optMyConnectionString = NULL;
char *optHost   = NULL;
char *optPort   = "8888";
char *optRRDHost   = NULL;
char *optRRDPort   = "13900";

int64_t lastRRDTick =0;
int64_t lastDBTick =0;

struct timeval te;

void write_db(uint32_t value);

uint32_t currentMeterValue;





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
    
    while ((opt = getopt(argc, argv, "a:b:r:v:f:g:t:d:m:s:h:p:")) != -1)
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
            case 't':
                i = getNum(optarg, &err);
                if (i >= 0) optRRDSeconds = (i);
                else fatal("invalid -t option (%s)", optarg);
                break;
            case 'd':
                i = getNum(optarg, &err);
                if (i >= 0) optDBSeconds = (i);
                else fatal("invalid -t option (%s)", optarg);
                break;

            case 'm':
                optMyConnectionString = malloc(strlen(optarg) + 1);
                if (optMyConnectionString) strcpy(optMyConnectionString, optarg);
                break;
            case 'f':
                optRRDFile = malloc(strlen(optarg) +1);
                if (optRRDFile) strcpy(optRRDFile, optarg);
                break;
            case 'h':
                optHost = malloc(strlen(optarg)+1);
                if (optHost) strcpy(optHost, optarg);
                break;
                
            case 'p':
                optPort = malloc(strlen(optarg)+1);
                if (optPort) strcpy(optPort, optarg);
                break;

            case 'r':
                optRRDHost = malloc(strlen(optarg)+1);
                if (optRRDHost) strcpy(optRRDHost, optarg);
                break;

            case 'b':
                optRRDPort = malloc(strlen(optarg)+1);
                if (optRRDPort) strcpy(optRRDPort, optarg);
                break;

            default: /* '?' */
                usage();
                exit(-1);
        }
    }
}

void write_rrd(uint32_t pos){
    
    char *str = malloc(sizeof(char) * 1024);
    sprintf(str, "N:%d", pos);
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
    
    printf("writing %d to rdd\n",pos);
    
}

void write_rrd_socket(uint32_t pos){

    printf("start writing rdd\n");
    fflush(stdout);

    char *str = malloc(sizeof(char) * 1024);
    sprintf(str, "update %s N:%d", optRRDFile, pos);
    char *data=malloc(strlen(str)+1);
    strcpy(data,str);


    int sockfd,portno,n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[256];
    portno = atoi(optRRDPort);

    /* Create a socket point */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);



    if (sockfd < 0) {
        printf("ERROR opening socket");
        return;
    }

    server = gethostbyname(optRRDHost);

    if (server == NULL) {
        printf("ERROR, no such host\n");
        return;
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    printf("trying to connect to server %s : %d", optRRDHost, portno);

    /* Now connect to the server */
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("ERROR connecting");
        close(sockfd);
        return;
    }

    /* Now ask for a message from the user, this message
       * will be read by server
    */



    /* Send message to the server */
    n=write(sockfd, data, strlen(data));

    if (n < 0) {
        printf("ERROR writing to socket");
        close(sockfd);
        return;
    }

    /* Now read server response */
    bzero(buffer,256);
    n = read(sockfd, buffer, 255);

    if (n < 0) {
        printf("ERROR reading from socket");
        close(sockfd);
        return;
    }


    printf("%s\n",buffer);
    close(sockfd);
    fflush(stdout);
    return;



}



void cbf(uint32_t pos, uint32_t tick)
{
    currentMeterValue=pos;
    printf("%1d @ %2d\n", pos,tick);
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
            sleep(optRRDSeconds+5);
            gettimeofday(&te, NULL);
            int64_t tick_sec = te.tv_sec;

            int64_t rrdTickDiff = tick_sec - lastRRDTick;
            if (tick_sec < lastRRDTick || (rrdTickDiff) > optRRDSeconds){
                write_rrd_socket(currentMeterValue);
                lastRRDTick = tick_sec;
            }

            int64_t dbTickDiff = tick_sec - lastDBTick;
            if (tick_sec < lastDBTick || (dbTickDiff) > optDBSeconds){
                write_db(currentMeterValue);
                lastDBTick = tick_sec;
            }


            fflush(stdout);
        }
        
        METER_cancel(renc);
        
        pigpio_stop(pi);
    }
    return 0;
}

void write_db(uint32_t value) {
//TODO
    printf("not writing to db\n");
}


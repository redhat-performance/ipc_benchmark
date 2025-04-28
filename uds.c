#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <sys/resource.h>

#define SECONDS_PER_MINUTE 60
#define SIZE_OF_USED_BUFFER 24    /* we put a sequence number and a timestamp at the beginning
                                  of the buffer */
#define  ARRAY_BOOST 20000        /* The size of the array initially and how much to increase it's
                                  size as it grows */   


/* Author:   Matt Currier
 *
 * This code base is from :
 * https://openbenchmarking.org/test/pts/ipc-benchmark ->
 * https://github.com/detailyang/ipc_benchmark
 *
 * MIT License
 *
 * Copyright (c) 2017 detailyang
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * To find out what cpus share a core (e.g. L1, L2 caches), on intel,
 * grep . /sys/devices/system/cpu/cpu*\/topology/thread_siblings_list
 * Stay away from a core-pair that has cpu0 in it.
 *
 */

int populate_buf(char *, int);
double getdetlatimeofday(struct timeval *, struct timeval *);
double time_diff(struct timeval , struct timeval );

typedef struct  {
      double start_time;
      double end_time;
} latencies_arr;

double
getdetlatimeofday(struct timeval *begin, struct timeval *end)
{
    return (end->tv_sec + end->tv_usec * 1.0 / 1000000) -
           (begin->tv_sec + begin->tv_usec * 1.0 / 1000000);
}
double
time_diff(struct timeval x , struct timeval y)
{
    double x_ms , y_ms , diff;

    x_ms = (double)x.tv_sec*1000000 + (double)x.tv_usec;
    y_ms = (double)y.tv_sec*1000000 + (double)y.tv_usec;

    diff = (double)y_ms - (double)x_ms;
    return diff;
}

/* sort the array of doubles (latencies) */
int compare_double( const void* a, const void* b )
{
    if( *(double*)a == *(double*)b ) return 0;
    return *(double*)a < *(double*)b ? -1 : 1;
}

/* create a buffer of random upper case letters */
int populate_buf(char *buffer, int length) {

    int i = length - 1;
    int r = rand() % 26;

    buffer[i] = '\0';

    while ( SIZE_OF_USED_BUFFER != i) {
        i = i - 1;
        buffer[i] = 'A' + r;
        r++;
        if (r==26)
           r = 0;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    int fd, nfd;
    long msgtot, size, sum, n;
    short mins;
    long ctr;
    short runnum;

    size_t len;

    struct timeval begin, end;
    struct sockaddr_un un;

    struct mybuf {
            int seq;
            struct timeval tv_time_sent;
            char *rest;
    } *buf;

    struct timeval tv_post_read;
    double time_elapsed;
    double avg_elapsed, tot_elapsed;

    int ninetyninth, ninetyfifth;  // 95th% and 99th% latencies

    time_t start;
 
    char latencies_file[50];
    FILE *outfile_latencies;

    if (argc != 4)
    {
	printf("usage: ./uds <message size> <mins-to-run> <run_number\n");
        return 1;
    }

    size = atoi(argv[1]);
    mins = atoi(argv[2]);
    runnum = atoi(argv[3]);

    buf = malloc(size);

    int new_size = ARRAY_BOOST;
    
    double *latencies = (double*) malloc(ARRAY_BOOST * sizeof(double));
    if (latencies == NULL) {
        printf("Latencies memory not allocated.\n");
        return 1;
    }
    latencies_arr *lat = (latencies_arr*) malloc(ARRAY_BOOST * sizeof(latencies_arr));
    if (lat == NULL) {
        printf("Latencies array memory not allocated.\n");
        return 1;
    }

    snprintf(latencies_file,50,"uds_latencies_%d_%hi.json", (int) size, runnum);
    outfile_latencies = fopen(latencies_file,"w");

    memset(&un, 0, sizeof(un));
    if (fork() == 0)   /* parent */
    {
        fd = socket(AF_UNIX, SOCK_STREAM, 0);
        unlink("./uds-ipc");
        un.sun_family = AF_UNIX;
        strcpy(un.sun_path, "./uds-ipc");
        len = offsetof(struct sockaddr_un, sun_path) + strlen("./uds-ipc");

        if (bind(fd, (struct sockaddr *)&un, len) == -1)
        {
            perror("bind");
            unlink("./uds-ipc");
            return 1;
        }

        if (listen(fd, 128) == -1)
        {
            perror("listen");
            unlink("./uds-ipc");
            return 1;
        }

        if ((nfd = accept(fd, NULL, NULL)) == -1)
        {
            perror("accept");
            unlink("./uds-ipc");
            return 1;
        }
        sum = 0;
	
        msgtot=0;     // sequence of each message sent

        start = time(NULL);
        while (time(NULL) - start < (time_t) ( mins * (SECONDS_PER_MINUTE )))  {     // run for "mins" minutes
        
            n = read(nfd, buf, size);
            if (n == 0)
            {
                break;
            }
            else if (n == -1)
            {
                perror("read");
                unlink("./uds-ipc");
                return 1;
            }
            if  (msgtot == new_size)  {
                new_size += ARRAY_BOOST;
                        double *latencies_tmp = realloc(latencies, new_size * sizeof(double)); 
                if (latencies_tmp == NULL)  {
                printf("Size increase of latencies array failed.\n");
                        return 1;
                }
    
                latencies_arr *lat_tmp = realloc(lat, new_size * sizeof(latencies_arr));
                if (lat_tmp == NULL)  {
                printf("Size increase of lat structure array failed.\n");
                        return 1;
                }
                latencies = latencies_tmp;
                lat = lat_tmp;
             }
             
             if (msgtot == buf->seq) {                      // verify messages in sync
                gettimeofday(&tv_post_read, NULL);         // timestamp after message sent 
		        //printf("buf1=%s\n", (char*) buf+SIZE_OF_USED_BUFFER);
                time_elapsed = time_diff(buf->tv_time_sent, tv_post_read);     // calc latency
                lat[msgtot].start_time = ((double) buf->tv_time_sent.tv_sec*1000000 + (double)buf->tv_time_sent.tv_usec);
                latencies[msgtot] = time_elapsed;                             
                lat[msgtot].end_time = ((double) tv_post_read.tv_sec*1000000 + (double)tv_post_read.tv_usec);
                tot_elapsed+=time_elapsed;
            }
            else {
                    printf("We did not match a sequence.  %ld    %d\n", msgtot, buf->seq);
                    unlink("./uds-ipc");
                    exit(1);
            }
            msgtot++;
            sum += n;
        }

        if (sum != msgtot * size)     // if not *all* the data is sent/received...
        {
            fprintf(stderr, "sum error: %ld != %ld\n", sum, msgtot * size);
            unlink("./uds-ipc");
            return 1;
        }

        avg_elapsed = tot_elapsed/msgtot;
        ninetyninth=(msgtot*0.99)-1;
        ninetyfifth=(msgtot*0.95)-1;
        
        fprintf(outfile_latencies, "{\n");     // write out latency data
        for (ctr = 0; ctr < msgtot-1; ctr++) {
            fprintf(outfile_latencies, "  {\"start-time\": %.0f, \"latency\": %.0f, \"end-time\": %.0f},\n ", lat[ctr].start_time, latencies[ctr], lat[ctr].end_time);
        }
        fprintf(outfile_latencies, "  {\"start-time\": %.0f, \"latency\": %.0f, \"end-time\": %.0f}\n}", lat[msgtot-1].start_time, latencies[msgtot-1], lat[msgtot-1].end_time);

        qsort(latencies, msgtot, sizeof(double), compare_double );

        printf("%.0fusMIN\n", latencies[0]);
        printf("%0.2fusAVG\n", avg_elapsed);
        printf("%.0fusMAX\n", latencies[msgtot-1]);
        printf("%.0fus95th\n", latencies[ninetyfifth]);
        printf("%.0fus99th\n", latencies[ninetyninth]);

        printf("The total messages sent to and from the child: %ld\n", msgtot);
    }
    else  /* child */
    {
        sleep(1);

        fd = socket(AF_UNIX, SOCK_STREAM, 0);
        un.sun_family = AF_UNIX;
        strcpy(un.sun_path, "./uds-ipc");
        len = offsetof(struct sockaddr_un, sun_path) + strlen("./uds-ipc");
        if (connect(fd, (struct sockaddr *)&un, len) == -1)
        {
            perror("connect");
            unlink("./uds-ipc");
            return 1;
        }

        gettimeofday(&begin, NULL);
        start = time(NULL);

        msgtot=0;
        while (time(NULL) - start < (time_t) ( mins * SECONDS_PER_MINUTE))  {
            buf->seq=msgtot;                          
            gettimeofday(&buf->tv_time_sent, NULL);   // timestamp before initial message sent
            populate_buf((char*)buf, size);           // populate the message buf
            //printf("buf1=%s\n", (char*) buf+SIZE_OF_USED_BUFFER);
            if (write(fd, buf, size) != size)         // send message to parent
            {
                perror("write");
                unlink("./uds-ipc");
                return 1;
            }
	    msgtot++;
        }

        printf("The total messages sent to and from the parent: %ld\n", msgtot);
        gettimeofday(&end, NULL);

        double tm = getdetlatimeofday(&begin, &end);
        printf("%.0fMB/s\n", msgtot * size * 1.0 / (tm * 1024 * 1024));
        printf("%.0fmsg/s\n", msgtot * 1.0 / tm);

        wait(NULL);
    }

    free(latencies);
    free(lat);
    unlink("./uds-ipc");
    fclose(outfile_latencies);
    return 0;
}

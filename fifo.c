#define __USE_GNU 1
#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/resource.h>

#define SECONDS_PER_MINUTE 60

/* This code base is from :
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

int compare_double( const void* a, const void* b )
{
    if( *(double*)a == *(double*)b ) return 0;
    return *(double*)a < *(double*)b ? -1 : 1;
}
typedef struct latencies
{
         double latency;
         struct latencies_data_t *next;
} latency_data_t;


/* ================================================================================================================================= */

int main(int argc, char *argv[]) {
    int              fd1, fd2;
    long             msgtot, size, array_size, sum, n;
    short 	     mins;
    long 	     ctr;
    short            runnum;

    struct mybuf {
	    int seq;
	    struct timeval tv_time_sent;
	    char *rest;
    } *buf;

    time_t start;

    char unsorted_file[50], sorted_file[50];
    FILE *outfile_unsorted, *outfile_sorted;

    struct timeval   begin, end;
    struct timeval  tv_post_read;

    double time_elapsed;
    double avg_elapsed, tot_elapsed;

    int ninetyninth, ninetyfifth;  // 95th% and 99th% latencies

    const rlim_t kStackSize = 512L * 1024L * 1024L;   // set stack size = 512 Mb
    struct rlimit rl;
    short result;

    if (argc != 5) {
        printf("usage: ./fifo <size> <array_size> <mins-to-run> <run_number\n");
        return 1;
    }

    result = getrlimit(RLIMIT_STACK, &rl);
    if (result == 0)
    {
        if (rl.rlim_cur < kStackSize)
        {
            rl.rlim_cur = kStackSize;
            result = setrlimit(RLIMIT_STACK, &rl);    // set stack size to higher limit to all creation
                                                      // of variable array of large size
            if (result != 0)
            {
	        perror("setrlimit");
                fprintf(stderr, "setrlimit returned result = %d\n", result);
            }
        }
    }


    size = atoi(argv[1]);
    array_size = atoi(argv[2]);
    mins = atoi(argv[3]);
    runnum = atoi(argv[4]);

    double latencies[array_size];    // size is passed in as 2nd parameter passed to program

    snprintf(unsorted_file,50,"fifo_latencies_unsorted_%d_%hi.csv", (int) size, runnum);
    snprintf(sorted_file,50,"fifo_latencies_sorted_%d_%hi.csv", (int) size, runnum);

    outfile_unsorted = fopen(unsorted_file,"w");
    outfile_sorted = fopen(sorted_file,"w");

    buf = malloc(size);
    if (buf == NULL) {
        perror("malloc");
        return 1;
    }

    unlink("./fifo-ipc1");
    unlink("./fifo-ipc2");
    if (mkfifo("./fifo-ipc1", 0700) == -1) {
        perror("mkfifo1");
        return 1;
    }
    if (mkfifo("./fifo-ipc2", 0700) == -1) {
        perror("mkfifo2");
        return 1;
    }

    fd1 = open("./fifo-ipc1", O_RDWR);
    if (fd1 == -1) {
        perror("open1");
        return 1;
    }
    fd2 = open("./fifo-ipc2", O_RDWR);
    if (fd2 == -1) {
        perror("open2");
        return 1;
    }

    if (fork() == 0) {   /* parent */

        sum = 0;
	msgtot=0;

	start = time(NULL);

	while (time(NULL) - start < (time_t) ( mins * SECONDS_PER_MINUTE))  {
            n = read(fd1, buf, size);     // receive message 1st time
            if (n == -1) {
                perror("first read");
    		unlink("./fifo-ipc1");
                unlink("./fifo-ipc2");
                return 1;
            }
	    if (msgtot == buf->seq) {   // ensure correct ordering
	        if (write(fd2, buf, size) != size) {       // send message back to parent
                     perror("second write");
                     unlink("./fifo-ipc1");
                     unlink("./fifo-ipc2");
		     exit(1);
                 }
	    }
	    else {
		    printf("We did not match the sequence on 1st send.\n");
    		    unlink("./fifo-ipc1");
		    unlink("./fifo-ipc2");
		    exit(1);
	    }
	    msgtot++;
            sum += n;
        }

        if (sum != msgtot * size) {
            fprintf(stderr, "sum error: %ld != %ld\n", sum, msgtot * size);
	    unlink("./fifo-ipc1");
            return 1;
        }
    
    } else {

        gettimeofday(&begin, NULL);

	start = time(NULL);

	msgtot=0;
        while (time(NULL) - start < (time_t) ( mins * SECONDS_PER_MINUTE))  {
	    buf->seq=msgtot;
	    gettimeofday(&buf->tv_time_sent, NULL);
            if (write(fd1, buf, size) != size) {       // send message 1st time to parent
                perror("first write");
		unlink("./fifo-ipc1");
                return 1;

            }
	    n = read(fd2, buf, size);      // receive message back from parent
            if (n == -1) {
                perror("read second");
		unlink("./fifo-ipc2");
                return 1;
            }
            if (msgtot == buf->seq) {      // ensure correct ordering
                gettimeofday(&tv_post_read, NULL);
		//printf("%.0f buf->tv_time_sent %d   second read \n", (double) buf->tv_time_sent.tv_usec, buf->seq); 
                time_elapsed = time_diff(buf->tv_time_sent, tv_post_read);
                latencies[msgtot] = time_elapsed;
                tot_elapsed += time_elapsed;
	    }
	    else {
                    printf("We did not match the sequence on return.\n");
                    unlink("./fifo-ipc1");
                    unlink("./fifo-ipc2");
                    exit(1);
            }
	    msgtot++;
        }

        gettimeofday(&end, NULL);

        double tm = getdetlatimeofday(&begin, &end);


        printf("%8.0fMB/s\n", msgtot * size * 1.0 / (tm * 1024 * 1024));
        printf("%8.0fmsg/s\n", msgtot * 1.0 / tm);

	avg_elapsed = tot_elapsed/msgtot;
        ninetyninth = (msgtot*0.99)-1;
        ninetyfifth = (msgtot*0.95)-1;
        for (ctr = 0; ctr < msgtot; ctr++) {
	    fprintf(outfile_unsorted, "%.0f,", latencies[ctr]);
        }
        fprintf(outfile_unsorted, "\n");

        qsort( latencies, msgtot, sizeof(double), compare_double );

        printf("%6.2fusMIN2nd\n", latencies[0]);
        printf("%6.2fusAVG2nd\n", avg_elapsed);
        printf("%6.2fusMAX2nd\n", latencies[msgtot-1]);
        printf("%6.2fus95th2nd\n", latencies[ninetyfifth]);
        printf("%6.2fus99th2nd\n", latencies[ninetyninth]);
    }

    printf("The total messages sent to and from the parent: %ld\n", msgtot); 

    for (ctr = 0; ctr < msgtot; ctr++) {
	    fprintf(outfile_sorted, "%.0f,", latencies[ctr]);
    }
    fprintf(outfile_sorted, "\n");
    fprintf(outfile_unsorted, "\n");

    unlink("./fifo-ipc1");
    unlink("./fifo-ipc2");
    fclose(outfile_unsorted);
    fclose(outfile_sorted);
    return 0;
}

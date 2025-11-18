#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "pf.h"


#define TEST_FILENAME "pf_testfile_seq"
#define BUFFER_SIZE 10   /* The buffer pool size to initialize */
#define NUM_PAGES 100     /* File size (must be > BUFFER_SIZE to test eviction) */
#define WORKLOAD_SIZE 10000 /* Total number of read/write operations */

#ifndef STRATEGY
#define STRATEGY PF_MRU
#endif



void check_error(int ec, const char *msg)
{
    if (ec != PFE_OK)
    {
        PF_PrintError((char*)msg);
        exit(EXIT_FAILURE);
    }
}


int main(int argc, char **argv)
{
    int fd;
    int i, pagenum, ratio;
    char *buf;
    char *str_read, *str_write;
    int read_ratio;

    /* Initialize random seed (for read/write mix) */
    srand(time(NULL));
    
    str_read = getenv("READ_RATIO");
    str_write = getenv("WRITE_RATIO");

    if (str_read == NULL || str_write == NULL)
    {
        fprintf(stderr, "Error: READ_RATIO and WRITE_RATIO env variables must be set.\n");
        return 1;
    }

    read_ratio = atoi(str_read);

    /* --- 2. Initialize PF Layer & Create Test File --- */
    PF_Init(BUFFER_SIZE);
    check_error(PF_CreateFile(TEST_FILENAME), "PF_CreateFile");

    fd = PF_OpenFile(TEST_FILENAME, STRATEGY);
    if (fd < 0) check_error(fd, "PF_OpenFile");


    for (i = 0; i < NUM_PAGES; i++)
    {
        check_error(PF_AllocPage(fd, &pagenum, &buf), "PF_AllocPage (prime)");
        sprintf(buf, "This is page %d", pagenum);
        check_error(PF_UnfixPage(fd, pagenum, TRUE), "PF_UnfixPage (prime)");
    }
    check_error(PF_CloseFile(fd), "PF_CloseFile (prime)");


    fd = PF_OpenFile(TEST_FILENAME, STRATEGY);
    if (fd < 0) check_error(fd, "PF_OpenFile (test)");

    
    PF_ResetStats();

    for (i = 0; i < WORKLOAD_SIZE; i++) {
        
        // CHANGE: Access pages sequentially (0, 1, ... 49, 0, ...)
        pagenum = i % NUM_PAGES;

        // Decide whether to read or write
        ratio = rand() % 100;

        if (ratio < read_ratio) {
            
            check_error(PF_GetThisPage(fd, pagenum, &buf), "PF_GetThisPage (read)");
            check_error(PF_UnfixPage(fd, pagenum, FALSE), "PF_UnfixPage (read)");
        }

        else {
            
            check_error(PF_GetThisPage(fd, pagenum, &buf), "PF_GetThisPage (write)");
            sprintf(buf, "Written at step %d", i);
            check_error(PF_UnfixPage(fd, pagenum, TRUE), "PF_UnfixPage (write)");
        }
    }

    // Clean Up and Report Stats 
    check_error(PF_CloseFile(fd), "PF_CloseFile (test)");

    
    printf("Logical I/Os: %ld\n", PF_GetLogicalIOs());
    printf("Physical I/Os: %ld\n", PF_GetPhysicalIOs());
    printf("Disk Reads: %ld\n", PF_GetDiskReads());
    printf("Disk Writes: %ld\n", PF_GetDiskWrites());

    check_error(PF_DestroyFile(TEST_FILENAME), "PF_DestroyFile");

    return 0;
}
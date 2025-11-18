#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "pf.h"

/* --- Test Configuration --- */
#define TEST_FILENAME "pf_testfile"
#define BUFFER_SIZE 10   /* The buffer pool size to initialize */
#define NUM_PAGES 100     /* File size (must be > BUFFER_SIZE to test eviction) */
#define WORKLOAD_SIZE 10000 /* Total number of read/write operations */
#define Strategy PF_MRU /*PF_MRU to test MRU strategy | PF_LRU to test LRU strategy */

/*
 * Helper function to check PF errors and exit
 */
void check_error(int ec, const char *msg)
{
    if (ec != PFE_OK)
    {
        PF_PrintError((char*)msg);
        exit(EXIT_FAILURE);
    }
}

/*
 * Main test function
 */
int main(int argc, char **argv)
{
    int fd;
    int i, pagenum, ratio;
    char *buf;
    char *str_read, *str_write;
    int read_ratio, write_ratio;

    /* Initialize random seed */
    srand(time(NULL));

    /* --- 1. Get Ratios from Environment --- */
    str_read = getenv("READ_RATIO");
    str_write = getenv("WRITE_RATIO");

	// str_read = "80";  // For testing purposes, set to 80% reads
	// str_write = "20"; // For testing purposes, set to 20% writes

    if (str_read == NULL || str_write == NULL)
    {
        fprintf(stderr, "Error: READ_RATIO and WRITE_RATIO env variables must be set.\n");
        return 1;
    }

    read_ratio = atoi(str_read);
    write_ratio = atoi(str_write); 

    /* --- 2. Initialize PF Layer & Create Test File --- */
    PF_Init(BUFFER_SIZE);
    check_error(PF_CreateFile(TEST_FILENAME), "PF_CreateFile");

    /* Open with LRU strategy. Change this to PF_MRU to test the other strategy. */
    fd = PF_OpenFile(TEST_FILENAME, Strategy);
    if (fd < 0) check_error(fd, "PF_OpenFile");

    /* --- 3. Prime the File with Data --- */
    /* We allocate NUM_PAGES and write to them to create the file on disk. */
    for (i = 0; i < NUM_PAGES; i++)
    {
        check_error(PF_AllocPage(fd, &pagenum, &buf), "PF_AllocPage (prime)");
        
        /* Write some dummy data */
        sprintf(buf, "This is page %d", pagenum);
        
        /* Unfix the page, marking it dirty */
        check_error(PF_UnfixPage(fd, pagenum, TRUE), "PF_UnfixPage (prime)");
    }
    
    /* Close the file to flush all pages from buffer */
    check_error(PF_CloseFile(fd), "PF_CloseFile (prime)");


    /* --- 4. Run the Workload --- */
    
    /* Re-open the file. The buffer is now empty. */
    fd = PF_OpenFile(TEST_FILENAME, Strategy);
    if (fd < 0) check_error(fd, "PF_OpenFile (test)");

    /* Reset statistics counters to zero */
    PF_ResetStats();

    for (i = 0; i < WORKLOAD_SIZE; i++)
    {
        /* Pick a random page to access */
        pagenum = rand() % NUM_PAGES;
        
        /* Decide whether to read or write */
        ratio = rand() % 100;

        if (ratio < read_ratio)
        {
            /* --- Read Operation --- */
            check_error(PF_GetThisPage(fd, pagenum, &buf), "PF_GetThisPage (read)");
            /* (We could read from buf here) */
            check_error(PF_UnfixPage(fd, pagenum, FALSE), "PF_UnfixPage (read)");
        }
        else
        {
            /* --- Write Operation --- */
            check_error(PF_GetThisPage(fd, pagenum, &buf), "PF_GetThisPage (write)");
            
            /* Write some new data */
            sprintf(buf, "Written at step %d", i);

            /* Unfix the page, marking it dirty */
            check_error(PF_UnfixPage(fd, pagenum, TRUE), "PF_UnfixPage (write)");
        }
    }

    /* --- 5. Clean Up and Report Stats --- */
    check_error(PF_CloseFile(fd), "PF_CloseFile (test)");

    /*
     * Print statistics in the EXACT format required by graphTest.py
     */
    printf("Logical I/Os: %ld\n", PF_GetLogicalIOs());
    printf("Physical I/Os: %ld\n", PF_GetPhysicalIOs());
    printf("Disk Reads: %ld\n", PF_GetDiskReads());
    printf("Disk Writes: %ld\n", PF_GetDiskWrites());

    /* Clean up the test file */
    check_error(PF_DestroyFile(TEST_FILENAME), "PF_DestroyFile");

    return 0;
}
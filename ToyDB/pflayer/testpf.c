/* testpf.c: test PF layer */
#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* for sprintf */
#include "pf.h"
#include <fcntl.h>

/* === DEFINE GLOBALS FROM pf.h === */
/* This file has main(), so it *defines* the globals */
int PF_BUFFER_SIZE = 20;          /* Max # of buffer pages */
int PF_REPLACEMENT_STRATEGY = 0;  /* 0=LRU, 1=MRU */

long PF_Logical_IO = 0;
long PF_Disk_Reads = 0;
long PF_Disk_Writes = 0;
/* === END OF GLOBALS === */


#define FILE1	"testfile1"
#define FILE2	"testfile2"

/* This is the function in buf.c to print the buffer state */
extern void PFbufPrint();


main()
{
    int i;
    int fd1;
    int pagenum;
    char *pagebuf;
    int err;

    /* === SET BUFFER CONFIGURATION === */
    PF_BUFFER_SIZE = 10;
    PF_REPLACEMENT_STRATEGY = 0; /* 0 for LRU, 1 for MRU */
    
    printf("Starting testpf.c...\n");
    printf("Buffer Size: %d\n", PF_BUFFER_SIZE);
    printf("Strategy: %s\n", PF_REPLACEMENT_STRATEGY == 0 ? "LRU" : "MRU");

    /* initialize the PF layer */
    PF_Init();

    /* create a new file */
    /* Check if file exists first to avoid errors if re-running */
    if (access(FILE1, F_OK) == 0) {
        printf("File %s already exists. Destroying it.\n", FILE1);
        if (PF_DestroyFile(FILE1) != PFE_OK) {
             PF_PrintError("PF_DestroyFile");
             exit(1);
        }
    }
    
    if (PF_CreateFile(FILE1) != PFE_OK) {
        PF_PrintError("PF_CreateFile");
        exit(1);
    }
    

    /* open the file */
    if ((fd1=PF_OpenFile(FILE1)) < 0) {
        PF_PrintError("PF_OpenFile");
        exit(1);
    }
    printf("Opened file %s with fd = %d\n", FILE1, fd1);

    /*
     * Allocate 15 pages in FILE1.
     * This will test buffer replacements if buffer size is < 15.
     */
    printf("\n--- Allocating 15 pages in FILE1 ---\n");
    for (i = 0; i < 15; i++) {
        if (PF_AllocPage(fd1, &pagenum, &pagebuf) != PFE_OK) {
            PF_PrintError("PF_AllocPage");
            exit(1);
        }
        
        /* Write page number into the page */
        sprintf(pagebuf, "Page %d", pagenum);
        printf("Allocated and wrote to page %d\n", pagenum);
        
        /* Mark dirty and unfix */
        if (PF_UnfixPage(fd1, pagenum, TRUE) != PFE_OK) {
            PF_PrintError("PF_UnfixPage");
            exit(1);
        }
    }

    /* Print buffer contents */
    PFbufPrint();

    /*
     * Re-read the first 10 pages.
     */
    printf("\n--- Re-reading first 10 pages ---\n");
    for (i = 0; i < 10; i++) {
        if (PF_GetThisPage(fd1, i, &pagebuf) != PFE_OK) {
            PF_PrintError("PF_GetThisPage");
            exit(1);
        }
        printf("Got page %d: %s\n", i, pagebuf);
        if (PF_UnfixPage(fd1, i, FALSE) != PFE_OK) {
            PF_PrintError("PF_UnfixPage");
            exit(1);
        }
    }
    
    /* Print buffer contents again to see changes */
    PFbufPrint();

    /* close the file */
    if (PF_CloseFile(fd1) != PFE_OK) {
        PF_PrintError("PF_CloseFile");
        exit(1);
    }

    /* destroy the file */
    if (PF_DestroyFile(FILE1) != PFE_OK) {
        PF_PrintError("PF_DestroyFile");
        exit(1);
    }

    /* === PRINT FINAL STATISTICS === */
    printf("\n--- Test Complete ---\n");
    printf("Logical I/Os:   %ld\n", PF_Logical_IO);
    printf("Disk Reads:     %ld\n", PF_Disk_Reads);
    printf("Disk Writes:    %ld\n", PF_Disk_Writes);
    printf("Hit Rate:       %.2f%%\n", 
        (PF_Logical_IO > 0) ? 
        (100.0 * (PF_Logical_IO - (PF_Disk_Reads + PF_Disk_Writes)) / PF_Logical_IO) : 0.0);
}
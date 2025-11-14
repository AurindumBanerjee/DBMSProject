#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "pf.h"
#include "pftypes.h"

/* === DEFINE GLOBALS FROM pf.h === */
/* This file is the main program, so we define the variables here */
int PF_BUFFER_SIZE = 20;          /* Max # of buffer pages */
int PF_REPLACEMENT_STRATEGY = 0;  /* 0=LRU, 1=MRU */
long PF_Logical_IO = 0;
long PF_Physical_IO = 0;
/* === END OF GLOBALS === */

#define FILE1 "testfile1"
#define FILE2 "testfile2"

void initialize_test_files(const char *filename) {
    int fd;
    PFhdr_str hdr;

    // Check if the file already exists
    if (access(filename, F_OK) == 0) {
        printf("File %s already exists, skipping creation.\n", filename);
        return;
    }

    // Create the file
    if ((fd = open(filename, O_CREAT | O_EXCL | O_WRONLY, 0644)) < 0) {
        perror("Failed to create file");
        exit(1);
    }

    // Initialize the header
    hdr.firstfree = PF_PAGE_LIST_END;
    hdr.numpages = 0;

    // Write the header to the file
    if (write(fd, &hdr, sizeof(PFhdr_str)) != sizeof(PFhdr_str)) {
        perror("Failed to write header");
        close(fd);
        exit(1);
    }

    close(fd);
    printf("Initialized file: %s\n", filename);
}

int main() {
    int i;
    int fd1, fd2;
    int pagenum;
    char *pagebuf;
    int err;

    /* === SET BUFFER CONFIGURATION === */
    /* You can change these values to test */
    PF_BUFFER_SIZE = 10;
    PF_REPLACEMENT_STRATEGY = 0; /* 0 for LRU, 1 for MRU */
    
    printf("Starting testpf.c...\n");
    printf("Buffer Size: %d\n", PF_BUFFER_SIZE);
    printf("Strategy: %s\n", PF_REPLACEMENT_STRATEGY == 0 ? "LRU" : "MRU");

    /* Initialize the PF layer */
    PF_Init();

    /* === CREATE TEST FILES === */
    printf("\n--- Creating test files ---\n");
    initialize_test_files(FILE1);
    initialize_test_files(FILE2);

    /* === OPEN FILE1 === */
    if ((fd1 = PF_OpenFile(FILE1)) < 0) {
        PF_PrintError("PF_OpenFile failed for FILE1");
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
            PF_PrintError("PF_AllocPage failed");
            exit(1);
        }

        printf("Allocated page number: %d\n", pagenum);

        /* Write page number into the page */
        sprintf(pagebuf, "Page %d", pagenum);
        printf("Allocated and wrote to page %d\n", pagenum);

        /* Validate page number before unfixing */
        if (pagenum < 0) {
            printf("Invalid page number: %d\n", pagenum);
            exit(1);
        }

        /* Mark dirty and unfix */
        if (PF_UnfixPage(fd1, pagenum, TRUE) != PFE_OK) {
            PF_PrintError("PF_UnfixPage failed");
            exit(1);
        }
    }

    /* Print buffer contents */
    printf("\n--- Buffer contents after allocation ---\n");
    PFbufPrint();

    /*
     * Re-read the first 10 pages.
     * With LRU, this should cause misses.
     * With MRU, this might cause hits.
     */
    printf("\n--- Re-reading first 10 pages ---\n");
    for (i = 0; i < 10; i++) {
        if (PF_GetThisPage(fd1, i, &pagebuf) != PFE_OK) {
            PF_PrintError("PF_GetThisPage failed");
            exit(1);
        }
        printf("Got page %d: %s\n", i, pagebuf);
        if (PF_UnfixPage(fd1, i, FALSE) != PFE_OK) {
            PF_PrintError("PF_UnfixPage failed");
            exit(1);
        }
    }
    
    /* Print buffer contents again to see changes */
    printf("\n--- Buffer contents after re-reading pages ---\n");
    PFbufPrint();

    /* === CLOSE AND DESTROY FILE1 === */
    if (PF_CloseFile(fd1) != PFE_OK) {
        PF_PrintError("PF_CloseFile failed for FILE1");
        exit(1);
    }
    printf("Closed file: %s\n", FILE1);

    if (PF_DestroyFile(FILE1) != PFE_OK) {
        PF_PrintError("PF_DestroyFile failed for FILE1");
        exit(1);
    }
    printf("Destroyed file: %s\n", FILE1);

    /* === PRINT FINAL STATISTICS === */
    printf("\n--- Test Complete ---\n");
    printf("Logical I/Os:   %ld\n", PF_Logical_IO);
    printf("Physical I/Os:  %ld\n", PF_Physical_IO);
    printf("Hit Rate:       %.2f%%\n", 
        (PF_Logical_IO > 0) ? 
        (100.0 * (PF_Logical_IO - PF_Physical_IO) / PF_Logical_IO) : 0.0);

    return 0;
}
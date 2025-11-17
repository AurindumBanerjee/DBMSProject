/*
 * test_rm.c: Test program for the Record Manager (RM) layer.
 *
 * This program reads the fixed-length student.txt file and simulates
 * variable-length data to test the slotted-page implementation.
 * It then generates the space utilization report required by the assignment.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h> // For ceil()
#include "pf.h"
#include "rm.h"
#include "pftypes.h" // To access PFftab for stats

#define STUDENT_DB_NAME "student_slotted.db"
#define STUDENT_DATA_FILE "../../data/student.txt"
#define MAX_LINE_LEN 256
#define MAX_TEST_NAME_LEN 100 // Max length for our simulated variable names

//
// This is the key function to simulate variable-length data.
//
char* create_variable_record(char* line, int* recordLen) {
    char id1[50], id2[50];
    char* recordBuf;
    int nameLen;
    
    // Parse the first two fields (IDs)
    char* token = strtok(line, ";");
    if (token) strncpy(id1, token, 50);
    
    token = strtok(NULL, ";");
    if (token) strncpy(id2, token, 50);
    
    // Create a variable-length "name" string.
    // We'll make it a random length between 10 and MAX_TEST_NAME_LEN.
    nameLen = 10 + (rand() % (MAX_TEST_NAME_LEN - 10));
    
    // Allocate buffer for "id1;id2;[variable_name]\0"
    *recordLen = strlen(id1) + 1 + strlen(id2) + 1 + nameLen + 1;
    recordBuf = (char*)malloc(*recordLen);
    if (!recordBuf) return NULL;
    
    // Create the record
    sprintf(recordBuf, "%s;%s;", id1, id2);
    // Append the variable part (just 'A's for simplicity)
    for(int i = 0; i < nameLen; i++) {
        strcat(recordBuf, "A");
    }

    return recordBuf; // The caller MUST free this
}


int main() {
    RM_FileHandle fh;
    RID rid;
    FILE* dataFile;
    char line[MAX_LINE_LEN];
    char* varRecord;
    int recordLen;
    long totalUsefulData = 0;
    long totalNumRecords = 0;

    // --- Part 1: Initialize and Populate the RM File ---
    
    // Init the PF layer (with a 20-page buffer pool)
    PF_Init(20);
    srand(0); // Use fixed seed for reproducible tests

    // Create the RM file
    RM_CreateFile(STUDENT_DB_NAME);
    if (RM_OpenFile(STUDENT_DB_NAME, PF_LRU, &fh) != RME_OK) {
        printf("Error opening RM file.\n");
        return 1;
    }

    // Open the raw student data file
    if ((dataFile = fopen(STUDENT_DATA_FILE, "r")) == NULL) {
        printf("Error: Could not open data file: %s\n", STUDENT_DATA_FILE);
        printf("Please check the path.\n");
        RM_CloseFile(&fh);
        return 1;
    }
    
    printf("Loading and simulating variable-length data...\n");

    // Read data file line by line
    while (fgets(line, MAX_LINE_LEN, dataFile)) {
        // Remove newline character
        line[strcspn(line, "\n")] = 0;

        // Simulate
        varRecord = create_variable_record(line, &recordLen);
        if (!varRecord) {
            printf("Error creating variable record.\n");
            continue;
        }

        // Insert into our slotted-page file
        if (RM_InsertRecord(&fh, varRecord, recordLen, &rid) != RME_OK) {
            printf("Error inserting record.\n");
        } else {
            totalUsefulData += recordLen;
            totalNumRecords++;
        }
        
        free(varRecord); // Free the simulated record buffer
    }

    fclose(dataFile);
    printf("...Loaded %ld records.\n", totalNumRecords);
    
    // --- Part 2: Calculate Statistics and Print Table ---

    printf("\n--- Performance Metrics (Objective 2) ---\n\n");
    
    // Get stats for our slotted-page file
    // PFftab is the global file table from the PF layer
    int totalPagesUsed_Slotted = PFftab[fh.pfFileDesc].hdr.numpages;
    long totalSpaceUsed_Slotted = (long)totalPagesUsed_Slotted * PF_PAGE_SIZE;
    double utilization_Slotted = (double)totalUsefulData / totalSpaceUsed_Slotted;

    // Close the file (flushes all pages)
    RM_CloseFile(&fh);

    // Print the header
    printf("| %-20s | %-20s | %-20s | %-20s | %-20s |\n",
           "Management Method", "Max Record Len (B)", "Total Space Used (B)", "Total Useful Data (B)", "Space Utilization (%)");
    printf("|----------------------|----------------------|----------------------|----------------------|----------------------|\n");

    // Print Slotted Page results
    printf("| %-20s | %-20s | %-20ld | %-20ld | %-20.2f |\n",
           "Slotted Page", "N/A", totalSpaceUsed_Slotted, totalUsefulData, utilization_Slotted * 100);


    // Calculate and Print Static Management results
    int staticRecordLengths[] = {150, 200, 250, 500};
    int num_static_tests = sizeof(staticRecordLengths) / sizeof(int);

    for (int i = 0; i < num_static_tests; i++) {
        int maxLen = staticRecordLengths[i];
        
        // Check if our max simulated data even fits
        if (maxLen < (MAX_TEST_NAME_LEN + 50)) { // 50 is buffer for IDs
             printf("| %-20s | %-20d | %-20s | %-20ld | %-20s |\n",
               "Static", maxLen, "Too Small", totalUsefulData, "N/A");
             continue;
        }

        long recordsPerPage_Static = (long)floor((double)PF_PAGE_SIZE / maxLen);
        if (recordsPerPage_Static == 0) recordsPerPage_Static = 1;
        
        long totalPagesNeeded_Static = (long)ceil((double)totalNumRecords / recordsPerPage_Static);
        long totalSpaceUsed_Static = totalPagesNeeded_Static * PF_PAGE_SIZE;
        double utilization_Static = (double)totalUsefulData / totalSpaceUsed_Static;

        printf("| %-20s | %-20d | %-20ld | %-20ld | %-20.2f |\n",
               "Static", maxLen, totalSpaceUsed_Static, totalUsefulData, utilization_Static * 100);
    }
    
    printf("\n");
    return 0;
}
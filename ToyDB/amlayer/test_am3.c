/*
 * test_am3.c: Test program for AM Layer Objective 3
 *
 * Compares three index-building strategies:
 * 1. Incremental Load: Building RM file and AM index simultaneously.
 * 2. Bulk Load (Unsorted): Scanning an existing RM file.
 * 3. Optimized Bulk Load (Sorted): Scanning, sorting, then loading.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "pf.h" // From amlayer directory
#include "am.h" // From amlayer directory
#include "rm.h" // From amlayer directory (copied)

/* --- File Definitions --- */
#define STUDENT_DB_FILE "student_slotted.db"
#define STUDENT_TXT_FILE "../../data/student.txt"
#define INDEX_FILE "student_index"

/* --- Index Definitions --- */
#define INDEX_ATTR_TYPE 'i'
#define INDEX_ATTR_LEN sizeof(int)
#define INDEX_FILE_NAME "student_index.0"

/* --- Other Constants --- */
#define MAX_RECORDS 20000 // Max records to load
#define MAX_LINE_LEN 256
#define MAX_TEST_NAME_LEN 100

/*
 * This struct is for the "Optimized Bulk Load" test.
 * We store all (key, RID) pairs to sort them before insertion.
 */
typedef struct {
    int key; // The student's roll number
    RID rid; // The (packed) Record ID
} KeyRidPair;

/*
 * qsort comparison function
 */
int compare_key_rid_pairs(const void *a, const void *b) {
    KeyRidPair *pairA = (KeyRidPair *)a;
    KeyRidPair *pairB = (KeyRidPair *)b;
    return (pairA->key - pairB->key);
}

/*
 * Helper: create_variable_record (Copied from test_rm.c)
 * Creates a simulated variable-length record.
 */
char* create_variable_record(char* line, int* recordLen) {
    char id1[50], id2[50];
    char* recordBuf;
    int nameLen;
    char* line_copy = strdup(line); // strtok modifies the string
    
    char* token = strtok(line_copy, ";");
    if (token) strncpy(id1, token, 50); else id1[0] = '\0';
    
    token = strtok(NULL, ";");
    if (token) strncpy(id2, token, 50); else id2[0] = '\0';
    
    nameLen = 10 + (rand() % (MAX_TEST_NAME_LEN - 10));
    
    *recordLen = strlen(id1) + 1 + strlen(id2) + 1 + nameLen + 1;
    recordBuf = (char*)malloc(*recordLen);
    if (!recordBuf) {
        free(line_copy);
        return NULL;
    }
    
    sprintf(recordBuf, "%s;%s;", id1, id2);
    for(int i = 0; i < nameLen; i++) {
        strcat(recordBuf, "A");
    }
    
    free(line_copy);
    return recordBuf;
}

/*
 * Helper: extract_key_from_record
 * Parses the record and returns the integer key (the 2nd ID).
 */
int extract_key_from_record(char *recordData) {
    char id1_str[50], id2_str[50];
    if (sscanf(recordData, "%49[^;];%49[^;];", id1_str, id2_str) == 2) {
        return atoi(id2_str); // Use the second ID as the integer key
    }
    return -1; // Error
}


int main() {
    RM_FileHandle rm_fh;
    RM_ScanHandle rm_scan;
    FILE *txt_file;
    char line[MAX_LINE_LEN];
    char record_buf[PF_PAGE_SIZE]; // Buffer for scanning
    int record_len;
    char *var_record;
    RID rid;
    int key;
    int am_fd;
    long record_count = 0;
    
    clock_t start, end;
    double cpu_time;
    long phys_ios;

    // A buffer to hold (key, RID) pairs for sorting
    KeyRidPair *key_rid_buffer = malloc(sizeof(KeyRidPair) * MAX_RECORDS);
    if (!key_rid_buffer) {
        printf("Failed to allocate memory for sort buffer.\n");
        return 1;
    }

    // Initialize Layers
    PF_Init(50);
    srand(0);

    printf("Starting Index Construction Tests...\n");
    printf("========================================\n\n");

    /*
     * First, ensure the main student RM file exists.
     * We create it once, and Tests 2 & 3 will use it.
     */
    printf("Pre-loading %s for Tests 2 and 3...\n", STUDENT_DB_FILE);
    RM_DestroyFile(STUDENT_DB_FILE); // Clean up from previous run
    RM_CreateFile(STUDENT_DB_FILE);
    if (RM_OpenFile(STUDENT_DB_FILE, PF_LRU, &rm_fh) != RME_OK) {
        printf("Error: Could not open %s\n", STUDENT_DB_FILE);
        return 1;
    }
    if ((txt_file = fopen(STUDENT_TXT_FILE, "r")) == NULL) {
        printf("Error: Could not open data file: %s\n", STUDENT_TXT_FILE);
        return 1;
    }
    record_count = 0;
    while (fgets(line, MAX_LINE_LEN, txt_file) && record_count < MAX_RECORDS) {
        line[strcspn(line, "\n")] = 0;
        var_record = create_variable_record(line, &record_len);
        if (RM_InsertRecord(&rm_fh, var_record, record_len, &rid) == RME_OK) {
            record_count++;
        }
        free(var_record);
    }
    RM_CloseFile(&rm_fh);
    fclose(txt_file);
    printf("...Done. Loaded %ld records into %s.\n\n", record_count, STUDENT_DB_FILE);


    /*
     * Test 1: Incremental Load
     * (Simulates populating DB and Index at the same time)
     */
    printf("--- Test 1: Incremental Load ---\n");
    RM_DestroyFile("temp_rm.db");
    AM_DestroyIndex(INDEX_FILE, 0);
    RM_CreateFile("temp_rm.db");
    AM_CreateIndex(INDEX_FILE, 0, INDEX_ATTR_TYPE, INDEX_ATTR_LEN);
    RM_OpenFile("temp_rm.db", PF_LRU, &rm_fh);
    am_fd = PF_OpenFile(INDEX_FILE_NAME, PF_LRU);
    
    if ((txt_file = fopen(STUDENT_TXT_FILE, "r")) == NULL) return 1;

    PF_ResetStats();
    start = clock();

    record_count = 0;
    while (fgets(line, MAX_LINE_LEN, txt_file) && record_count < MAX_RECORDS) {
        line[strcspn(line, "\n")] = 0;
        var_record = create_variable_record(line, &record_len);
        
        // 1. Insert into RM file
        if (RM_InsertRecord(&rm_fh, var_record, record_len, &rid) != RME_OK) {
            free(var_record);
            continue;
        }
        
        // 2. Extract key and insert into AM index
        key = extract_key_from_record(var_record);
        if (key != -1) {
            AM_InsertEntry(am_fd, INDEX_ATTR_TYPE, INDEX_ATTR_LEN, (char *)&key, rid);
            record_count++;
        }
        free(var_record);
    }

    end = clock();
    phys_ios = PF_GetPhysicalIOs();
    cpu_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    printf("Results for Test 1 (Incremental):\n");
    printf("  Time Taken: %f sec\n", cpu_time);
    printf("  Physical I/Os: %ld\n\n", phys_ios);
    
    fclose(txt_file);
    PF_CloseFile(am_fd);
    RM_CloseFile(&rm_fh);
    AM_DestroyIndex(INDEX_FILE, 0);
    RM_DestroyFile("temp_rm.db");


    /*
     * Test 2: Bulk Load (Unsorted)
     * (Scans the pre-existing RM file)
     */
    printf("--- Test 2: Bulk Load (Unsorted Scan) ---\n");
    AM_DestroyIndex(INDEX_FILE, 0);
    AM_CreateIndex(INDEX_FILE, 0, INDEX_ATTR_TYPE, INDEX_ATTR_LEN);
    am_fd = PF_OpenFile(INDEX_FILE_NAME, PF_LRU);
    RM_OpenFile(STUDENT_DB_FILE, PF_LRU, &rm_fh);
    RM_OpenScan(&rm_fh, &rm_scan);

    PF_ResetStats();
    start = clock();

    while (RM_GetNextRecord(&rm_scan, &rid, record_buf, PF_PAGE_SIZE, &record_len) == RME_OK) {
        key = extract_key_from_record(record_buf);
        printf("Inserting key: %d\n", key);
        if (key != -1) {
            AM_InsertEntry(am_fd, INDEX_ATTR_TYPE, INDEX_ATTR_LEN, (char *)&key, rid);
        }
        else{
            printf("Warning: Could not extract key from record.\n");
        }
    }

    end = clock();
    phys_ios = PF_GetPhysicalIOs();
    cpu_time = ((double)(end - start)) / CLOCKS_PER_SEC;

    printf("Results for Test 2 (Bulk - Unsorted):\n");
    printf("  Time Taken: %f sec\n", cpu_time);
    printf("  Physical I/Os: %ld\n\n", phys_ios);

    RM_CloseScan(&rm_scan);
    RM_CloseFile(&rm_fh);
    PF_CloseFile(am_fd);
    AM_DestroyIndex(INDEX_FILE, 0);


    /*
     * Test 3: Optimized Bulk Load (Sorted)
     * (Scans, sorts in memory, then inserts)
     */
    printf("--- Test 3: Optimized Bulk Load (Sorted) ---\n");
    
    // Step 3a: Scan RM file and fill the sort buffer
    printf("  Scanning and buffering records...\n");
    RM_OpenFile(STUDENT_DB_FILE, PF_LRU, &rm_fh);
    RM_OpenScan(&rm_fh, &rm_scan);
    record_count = 0;
    while (RM_GetNextRecord(&rm_scan, &rid, record_buf, PF_PAGE_SIZE, &record_len) == RME_OK) {
        key = extract_key_from_record(record_buf);
        if (key != -1 && record_count < MAX_RECORDS) {
            key_rid_buffer[record_count].key = key;
            key_rid_buffer[record_count].rid = rid;
            record_count++;
        }
    }
    RM_CloseScan(&rm_scan);
    RM_CloseFile(&rm_fh);

    // Step 3b: Sort the buffer
    printf("  Sorting %ld (key, RID) pairs in memory...\n", record_count);
    qsort(key_rid_buffer, record_count, sizeof(KeyRidPair), compare_key_rid_pairs);
    
    // Step 3c: Create index and insert from the sorted buffer
    AM_DestroyIndex(INDEX_FILE, 0);
    AM_CreateIndex(INDEX_FILE, 0, INDEX_ATTR_TYPE, INDEX_ATTR_LEN);
    am_fd = PF_OpenFile(INDEX_FILE_NAME, PF_LRU);

    printf("  Inserting sorted keys into index...\n");
    PF_ResetStats();
    start = clock();
    
    for (long i = 0; i < record_count; i++) {
        AM_InsertEntry(am_fd, INDEX_ATTR_TYPE, INDEX_ATTR_LEN,
                       (char *)&(key_rid_buffer[i].key),
                       key_rid_buffer[i].rid);
    }
    
    end = clock();
    phys_ios = PF_GetPhysicalIOs();
    cpu_time = ((double)(end - start)) / CLOCKS_PER_SEC;

    printf("Results for Test 3 (Optimized - Sorted):\n");
    printf("  Time Taken: %f sec\n", cpu_time);
    printf("  Physical I/Os: %ld\n\n", phys_ios);

    PF_CloseFile(am_fd);
    AM_DestroyIndex(INDEX_FILE, 0);
    free(key_rid_buffer);
    
    printf("========================================\n");
    printf("All tests complete.\n");

    return 0;
}
/* rm.h: Record Manager interface */

#ifndef RM_H
#define RM_H

#include "pf.h" // We are built on top of the PF layer

/*
 * RID: Record ID
 * A unique identifier for a record, consisting of a page number
 * and a slot number within that page.
 */
typedef struct {
    int pageNum;
    int slotNum;
} RID;

/*
 * RM_FileHandle: File Handle
 * This struct stores information about an open RM file.
 */
typedef struct {
    int pfFileDesc; // The file descriptor from the PF layer
} RM_FileHandle;

/*
 * RM_ScanHandle: Scan Handle
 * This struct stores the state of an ongoing scan.
 */
typedef struct {
    RM_FileHandle *fileHandle;
    int currentPage; // The page number of the page we are scanning
    int currentSlot; // The slot number of the next record to check
    char *pageData;    // Pinned page buffer from PF layer
} RM_ScanHandle;


/* Error codes */
#define RME_OK         0
#define RME_EOF       -1  /* End of file or scan */
#define RME_NOMEM     -2
#define RME_BUFTOOSMALL -3 /* Provided buffer is too small */
#define RME_INVALIDRID -4  /* Record (page/slot) does not exist */
#define RME_ERROR     -99 /* A generic error */


/* --- File Management --- */

/*
 * RM_CreateFile
 * Desc: Creates a new paged file for the record manager.
 * (Just a wrapper for PF_CreateFile)
 */
int RM_CreateFile(char *fileName);

/*
 * RM_DestroyFile
 * Desc: Destroys an existing RM file.
 * (Just a wrapper for PF_DestroyFile)
 */
int RM_DestroyFile(char *fileName);

/*
 * RM_OpenFile
 * Desc: Opens an RM file.
 * Params: (char*) fileName - name of file to open
 * (PF_Strategy) strategy - buffer replacement strategy
 * (RM_FileHandle*) fh - (out) file handle
 * Returns: RME_OK or a PF error code
 */
int RM_OpenFile(char *fileName, PF_Strategy strategy, RM_FileHandle *fh);

/*
 * RM_CloseFile
 * Desc: Closes an RM file.
 * Returns: RME_OK or a PF error code
 */
int RM_CloseFile(RM_FileHandle *fh);


/* --- Record Management --- */

/*
 * RM_InsertRecord
 * Desc: Inserts a new record into the file.
 * Params: (RM_FileHandle*) fh - file handle
 * (char*) data - pointer to the record data
 * (int) dataLength - length of the record data
 * (RID*) rid - (out) the RID of the new record
 * Returns: RME_OK or an error code
 */
int RM_InsertRecord(RM_FileHandle *fh, char *data, int dataLength, RID *rid);

/*
 * RM_DeleteRecord
 * Desc: Deletes a record from the file.
 * Params: (RM_FileHandle*) fh - file handle
 * (RID*) rid - the RID of the record to delete
 * Returns: RME_OK or an error code
 */
int RM_DeleteRecord(RM_FileHandle *fh, RID *rid);

/*
 * RM_GetRecord
 * Desc: Retrieves a single record from the file.
 * Params: (RM_FileHandle*) fh - file handle
 * (RID*) rid - the RID of the record to retrieve
 * (char*) dataBuf - (out) buffer to copy record data into
 * (int) bufSize - size of the dataBuf
 * (int*) dataLength - (out) actual length of the record
 * Returns: RME_OK, RME_BUFTOOSMALL, or an error code
 */
int RM_GetRecord(RM_FileHandle *fh, RID *rid, char *dataBuf, int bufSize, int *dataLength);


/* --- Scanning --- */

/*
 * RM_OpenScan
 * Desc: Initializes a new scan on the file.
 * Params: (RM_FileHandle*) fh - file handle
 * (RM_ScanHandle*) sh - (out) scan handle
 * Returns: RME_OK or an error code
 */
int RM_OpenScan(RM_FileHandle *fh, RM_ScanHandle *sh);

/*
 * RM_GetNextRecord
 * Desc: Retrieves the next record in the scan.
 * Params: (RM_ScanHandle*) sh - scan handle
 * (RID*) rid - (out) RID of the next record
 * (char*) dataBuf - (out) buffer for record data
 * (int) bufSize - size of dataBuf
 * (int*) dataLength - (out) actual length of record
 * Returns: RME_OK (success), RME_EOF (no more records), or an error
 */
int RM_GetNextRecord(RM_ScanHandle *sh, RID *rid, char *dataBuf, int bufSize, int *dataLength);

/*
 * RM_CloseScan
 * Desc: Finalizes a scan.
 * Returns: RME_OK or an error code
 */
int RM_CloseScan(RM_ScanHandle *sh);

#endif /* RM_H */
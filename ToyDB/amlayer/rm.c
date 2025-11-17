#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rm.h"
#include "pf.h"
#include "pftypes.h" 

/* * Internal Page Structure
 *
 * We define our own slotted page layout which lives inside
 * the PF_PAGE_SIZE buffer provided by the PF layer.
 */

/* SlotEntry: An entry in the slot directory (at the end of the page) */
typedef struct {
    int recordOffset; // Byte offset from the start of the page
    int recordLength; // Length of the record in bytes
} SlotEntry;

#define SLOT_EMPTY -1 // A recordLength of -1 indicates an empty/deleted slot

/* PageHeader: Metadata at the very beginning of the page */
typedef struct {
    int numSlots;       // Total number of slots in the directory
    int freeSpaceOffset; // Offset of the start of free space (end of data)
} PageHeader;


/* --- Helper Macros --- */
#define RM_PAGE_SIZE PF_PAGE_SIZE

// Get a pointer to the Page Header (at the very start of the page data)
#define GET_HEADER(pageData) ((PageHeader*) (pageData))

// Get a pointer to the Slot Directory entry for a given SlotID
// The slot directory grows *backwards* from the end of the page.
#define GET_SLOT(pageData, slotID) \
    ((SlotEntry*) ( (pageData) + RM_PAGE_SIZE - sizeof(SlotEntry) * (slotID + 1) ))

// Calculate the amount of *contiguous* free space in the middle
#define GET_CONTIGUOUS_FREE_SPACE(pageData) \
    ( (RM_PAGE_SIZE) - (GET_HEADER(pageData)->numSlots * sizeof(SlotEntry)) - (GET_HEADER(pageData)->freeSpaceOffset) )

/*
 * RM_InitPage
 * Desc: Initializes a new, empty page as a slotted page.
 */
static void RM_InitPage(char *pageData) {
    PageHeader *header = GET_HEADER(pageData);
    header->numSlots = 0;
    header->freeSpaceOffset = sizeof(PageHeader);
}

/* --- Helper Functions for Packed RID --- */

RID RM_PackRID(int pageNum, int slotNum) {
    // Upper 16 bits for pageNum, Lower 16 bits for slotNum
    return (pageNum << 16) | (slotNum & 0xFFFF);
}

void RM_UnpackRID(RID rid, int *pageNum, int *slotNum) {
    *pageNum = (rid >> 16) & 0xFFFF;
    *slotNum = rid & 0xFFFF;
}


/* --- File Management --- */

int RM_CreateFile(char *fileName) {
    return PF_CreateFile(fileName);
}

int RM_DestroyFile(char *fileName) {
    return PF_DestroyFile(fileName);
}

int RM_OpenFile(char *fileName, PF_Strategy strategy, RM_FileHandle *fh) {
    int pf_fd;
    if ((pf_fd = PF_OpenFile(fileName, strategy)) < 0) {
        PF_PrintError("RM_OpenFile: PF_OpenFile");
        return pf_fd; // Return the PF error code
    }
    fh->pfFileDesc = pf_fd;
    return RME_OK;
}

int RM_CloseFile(RM_FileHandle *fh) {
    return PF_CloseFile(fh->pfFileDesc);
}


/* --- Record Management --- */

int RM_InsertRecord(RM_FileHandle *fh, char *data, int dataLength, RID *rid) {
    int pageNum, pf_err;
    char *pageData;
    int spaceNeeded = dataLength; // Space for data
    
    int foundEmptySlot = FALSE;
    int targetSlotID = -1;

    // 1. Find a page with enough space
    pageNum = -1;
    while (PF_GetNextPage(fh->pfFileDesc, &pageNum, &pageData) == PFE_OK) {
        PageHeader *header = GET_HEADER(pageData);
        
        // Check for an empty slot
        foundEmptySlot = FALSE;
        for (int i = 0; i < header->numSlots; i++) {
            if (GET_SLOT(pageData, i)->recordLength == SLOT_EMPTY) {
                foundEmptySlot = TRUE;
                targetSlotID = i;
                break;
            }
        }

        int currentSpaceNeeded = spaceNeeded;
        if (!foundEmptySlot) {
            currentSpaceNeeded += sizeof(SlotEntry); // Need space for data AND a new slot
        }

        if (GET_CONTIGUOUS_FREE_SPACE(pageData) >= currentSpaceNeeded) {
            // Found a page with space!
            break;
        }
        
        // Unfix the page (not dirty) and continue to the next one
        if ((pf_err = PF_UnfixPage(fh->pfFileDesc, pageNum, FALSE)) != PFE_OK) {
            PF_PrintError("RM_InsertRecord: PF_UnfixPage");
            return pf_err;
        }
    }

    // 2. If no page found, allocate a new one
    if (PFerrno == PFE_EOF || pageNum == -1) {
        if ((pf_err = PF_AllocPage(fh->pfFileDesc, &pageNum, &pageData)) != PFE_OK) {
            PF_PrintError("RM_InsertRecord: PF_AllocPage");
            return pf_err;
        }
        // Initialize the new page
        RM_InitPage(pageData);
        targetSlotID = -1; // Will use slot 0
    } else if (PFerrno != PFE_OK) {
        // Some other error occurred
        PF_PrintError("RM_InsertRecord: PF_GetNextPage");
        return PFerrno;
    }

    // 3. We now have a page (pageData) and pageNum. Insert the record.
    PageHeader *header = GET_HEADER(pageData);
    
    // 3a. Finalize slot
    if (targetSlotID == -1) {
        targetSlotID = header->numSlots; // Use a new slot
    }

    // 4. Add the data to the page
    SlotEntry *slot = GET_SLOT(pageData, targetSlotID);
    int dataOffset = header->freeSpaceOffset;
    memcpy(pageData + dataOffset, data, dataLength);

    // 5. Update the slot and header
    slot->recordOffset = dataOffset;
    slot->recordLength = dataLength;
    
    header->freeSpaceOffset += dataLength;
    if (targetSlotID == header->numSlots) {
        header->numSlots++;
    }

    // 6. Set the output RID (PACKED)
    *rid = RM_PackRID(pageNum, targetSlotID);

    // 7. Unfix the page, marking it as dirty
    if ((pf_err = PF_UnfixPage(fh->pfFileDesc, pageNum, TRUE)) != PFE_OK) {
        PF_PrintError("RM_InsertRecord: PF_UnfixPage (dirty)");
        return pf_err;
    }

    return RME_OK;
}

int RM_DeleteRecord(RM_FileHandle *fh, RID rid) {
    char *pageData;
    int pf_err, pageNum, slotNum;

    // 1. Unpack the RID
    RM_UnpackRID(rid, &pageNum, &slotNum);

    // 2. Get the page
    if ((pf_err = PF_GetThisPage(fh->pfFileDesc, pageNum, &pageData)) != PFE_OK) {
        PF_PrintError("RM_DeleteRecord: PF_GetThisPage");
        return (pf_err == PFE_INVALIDPAGE) ? RME_INVALIDRID : pf_err;
    }

    PageHeader *header = GET_HEADER(pageData);

    // 3. Check if RID is valid
    if (slotNum >= header->numSlots) {
        PF_UnfixPage(fh->pfFileDesc, pageNum, FALSE); // Not dirty
        return RME_INVALIDRID;
    }
    
    SlotEntry *slot = GET_SLOT(pageData, slotNum);
    if (slot->recordLength == SLOT_EMPTY) {
        PF_UnfixPage(fh->pfFileDesc, pageNum, FALSE); // Not dirty
        return RME_INVALIDRID; // Already deleted
    }

    // 4. Get info about the record to be deleted
    int deletedOffset = slot->recordOffset;
    int deletedLength = slot->recordLength;

    // 5. Mark the slot as empty
    slot->recordLength = SLOT_EMPTY;

    // 6. **Compact the data**
    //    Move all data *after* the deleted record to the left
    char *holeStart = pageData + deletedOffset;
    char *holeEnd = holeStart + deletedLength;
    char *dataEnd = pageData + header->freeSpaceOffset;
    
    memmove(holeStart, holeEnd, dataEnd - holeEnd);

    // 7. Update all other slot offsets
    for (int i = 0; i < header->numSlots; i++) {
        SlotEntry *otherSlot = GET_SLOT(pageData, i);
        if (otherSlot->recordLength != SLOT_EMPTY && otherSlot->recordOffset > deletedOffset) {
            otherSlot->recordOffset -= deletedLength;
        }
    }

    // 8. Update the header
    header->freeSpaceOffset -= deletedLength;

    // (Optional) We could shrink header->numSlots if this was the last slot,
    // but we'll leave it for simplicity and slot reuse.

    // 9. Unfix the page, marking it as dirty
    if ((pf_err = PF_UnfixPage(fh->pfFileDesc, pageNum, TRUE)) != PFE_OK) {
        PF_PrintError("RM_DeleteRecord: PF_UnfixPage (dirty)");
        return pf_err;
    }

    return RME_OK;
}

int RM_GetRecord(RM_FileHandle *fh, RID rid, char *dataBuf, int bufSize, int *dataLength) {
    char *pageData;
    int pf_err, pageNum, slotNum;

    // 1. Unpack the RID
    RM_UnpackRID(rid, &pageNum, &slotNum);

    // 2. Get the page
    if ((pf_err = PF_GetThisPage(fh->pfFileDesc, pageNum, &pageData)) != PFE_OK) {
        PF_PrintError("RM_GetRecord: PF_GetThisPage");
        return (pf_err == PFE_INVALIDPAGE) ? RME_INVALIDRID : pf_err;
    }

    PageHeader *header = GET_HEADER(pageData);

    // 3. Check if RID is valid
    if (slotNum >= header->numSlots) {
        PF_UnfixPage(fh->pfFileDesc, pageNum, FALSE);
        return RME_INVALIDRID;
    }

    SlotEntry *slot = GET_SLOT(pageData, slotNum);
    if (slot->recordLength == SLOT_EMPTY) {
        PF_UnfixPage(fh->pfFileDesc, pageNum, FALSE);
        return RME_INVALIDRID; // Record was deleted
    }

    // 4. Check if buffer is large enough
    if (bufSize < slot->recordLength) {
        PF_UnfixPage(fh->pfFileDesc, pageNum, FALSE);
        return RME_BUFTOOSMALL;
    }

    // 5. Copy data to output buffer
    memcpy(dataBuf, pageData + slot->recordOffset, slot->recordLength);
    *dataLength = slot->recordLength;

    // 6. Unfix the page (not dirty)
    if ((pf_err = PF_UnfixPage(fh->pfFileDesc, pageNum, FALSE)) != PFE_OK) {
        PF_PrintError("RM_GetRecord: PF_UnfixPage");
        return pf_err;
    }

    return RME_OK;
}


/* --- Scanning --- */

int RM_OpenScan(RM_FileHandle *fh, RM_ScanHandle *sh) {
    sh->fileHandle = fh;
    sh->currentPage = -1; // Will start with PF_GetNextPage (or First)
    sh->currentSlot = -1; // Start before the first slot
    sh->pageData = NULL;  // No page is pinned yet
    return RME_OK;
}

int RM_GetNextRecord(RM_ScanHandle *sh, RID *rid, char *dataBuf, int bufSize, int *dataLength) {
    int pf_err;
    RM_FileHandle *fh = sh->fileHandle;

    while (TRUE) {
        // 1. Check if we need to get a new page
        if (sh->pageData == NULL) {
            int oldPage = sh->currentPage;

            if (sh->currentPage == -1) { // First call
                pf_err = PF_GetFirstPage(fh->pfFileDesc, &sh->currentPage, &sh->pageData);
            } else { // Subsequent calls
                pf_err = PF_GetNextPage(fh->pfFileDesc, &sh->currentPage, &sh->pageData);
                // Unfix the *previous* page (if it existed)
                if (oldPage != -1) {
                    PF_UnfixPage(fh->pfFileDesc, oldPage, FALSE);
                }
            }

            if (pf_err == PFE_EOF) return RME_EOF; // No more pages
            if (pf_err != PFE_OK) {
                PF_PrintError("RM_GetNextRecord: PF_GetFirst/NextPage");
                return pf_err;
            }
            
            sh->currentSlot = -1; // Reset slot scan for the new page
        }

        // 2. We have a page, scan its slots
        PageHeader *header = GET_HEADER(sh->pageData);
        sh->currentSlot++;

        if (sh->currentSlot < header->numSlots) {
            // 2a. Check this slot
            SlotEntry *slot = GET_SLOT(sh->pageData, sh->currentSlot);
            if (slot->recordLength != SLOT_EMPTY) {
                // Found a valid record!
                if (bufSize < slot->recordLength) {
                    return RME_BUFTOOSMALL;
                }
                // Copy data
                memcpy(dataBuf, sh->pageData + slot->recordOffset, slot->recordLength);
                *dataLength = slot->recordLength;
                
                // Set the output RID (PACKED)
                *rid = RM_PackRID(sh->currentPage, sh->currentSlot);
                
                return RME_OK; // Success!
            }
            // else, slot was empty, loop continues to next slot
        } else {
            // 2b. Reached end of slots for this page
            //    Flag to fetch a new page on the next loop iteration
            sh->pageData = NULL; 
        }
    }
}

int RM_CloseScan(RM_ScanHandle *sh) {
    // Unfix the last page we were holding (if any)
    if (sh->pageData != NULL) {
        if (PF_UnfixPage(sh->fileHandle->pfFileDesc, sh->currentPage, FALSE) != PFE_OK) {
            PF_PrintError("RM_CloseScan: PF_UnfixPage");
            return PFerrno;
        }
        sh->pageData = NULL;
    }
    return RME_OK; /* RKE_OK might be a typo, should be RME_OK */
}
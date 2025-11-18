/* pf.h: externs and error codes for Paged File Interface*/

#ifndef PF_H
#define PF_H

#ifndef TRUE
#define TRUE 1		
#endif
#ifndef FALSE
#define FALSE 0
#endif


#define PFE_OK		0	/* OK */
#define PFE_NOMEM	-1	/* no memory */
#define PFE_NOBUF	-2	/* no buffer space */
#define PFE_PAGEFIXED 	-3	/* page already fixed in buffer */
#define PFE_PAGENOTINBUF -4	/* page to be unfixed is not in the buffer */
#define PFE_UNIX	-5	/* unix error */
#define PFE_INCOMPLETEREAD -6	/* incomplete read of page from file */
#define PFE_INCOMPLETEWRITE -7	/* incomplete write of page to file */
#define PFE_HDRREAD	-8	/* incomplete read of header from file */
#define PFE_HDRWRITE	-9	/* incomplte write of header to file */
#define PFE_INVALIDPAGE -10	/* invalid page number */
#define PFE_FILEOPEN	-11	/* file already open */
#define	PFE_FTABFULL	-12	/* file table is full */
#define PFE_FD		-13	/* invalid file descriptor */
#define PFE_EOF		-14	/* end of file */
#define PFE_PAGEFREE	-15	/* page already free */
#define PFE_PAGEUNFIXED	-16	/* page already unfixed */

/* Internal error: please report to the TA */
#define PFE_PAGEINBUF	-17	/* new page to be allocated already in buffer */
#define PFE_HASHNOTFOUND -18	/* hash table entry not found */
#define PFE_HASHPAGEEXIST -19	/* page already exist in hash table */


/* page size */
#define PF_PAGE_SIZE	4096

// Replacement Strategy Enum 
typedef enum { PF_LRU = 0, PF_MRU = 1 } PF_Strategy;

/* externs from the PF layer */
extern int PFerrno;		/* error number of last error */


/*
 * PF_Init
 *
 * Desc: Initialize the PF layer.
 * This function must be called before any other PF functions.
 * It initializes the buffer manager with a specified number
 * of buffer pages.
 * Params: (int) bufsize - the number of buffer pages to allocate.
 */
extern void PF_Init(int bufsize);

/*
 * PF_CreateFile
 *
 * Desc: Create a new paged file with the given name.
 * Params: (char*) fname - name of the file to create.
 * Returns: PFE_OK if success, or a PF error code otherwise.
 */
extern int PF_CreateFile(char *fname);

/*
 * PF_DestroyFile
 *
 * Desc: Destroy the paged file with the given name.
 * The file must not be open.
 * Params: (char*) fname - name of the file to destroy.
 * Returns: PFE_OK if success, or a PF error code otherwise.
 */
extern int PF_DestroyFile(char *fname);

/*
 * PF_OpenFile
 *
 * Desc: Open the paged file with the given name.
 * The file must already exist.
 * The specified replacement strategy will be used for this file's
 * pages in the buffer pool.
 * Params: (char*) fname - name of the file to open.
 * (PF_Strategy) strategy - replacement strategy (PF_LRU or PF_MRU).
 * Returns: A file descriptor (int) >= 0 if success, or a PF error code otherwise.
 */
extern int PF_OpenFile(char *fname, PF_Strategy strategy);

/*
 * PF_CloseFile
 *
 * Desc: Close the file associated with the given file descriptor.
 * All pages for this file will be flushed from the buffer.
 * It is an error to close a file with pages still fixed.
 * Params: (int) fd - file descriptor.
 * Returns: PFE_OK if success, or a PF error code otherwise.
 */
extern int PF_CloseFile(int fd);

/*
 * PF_GetFirstPage
 *
 * Desc: Get the first valid (used) page in the file.
 * The page is fixed in the buffer pool.
 * Params: (int) fd - file descriptor.
 * (int*) pagenum - (out) placeholder for the page number.
 * (char**) pagebuf - (out) placeholder for the page buffer.
 * Returns: PFE_OK if success, PFE_EOF if no pages, or PF error code.
 */
extern int PF_GetFirstPage(int fd, int *pagenum, char **pagebuf);

/*
 * PF_GetNextPage
 *
 * Desc: Get the next valid (used) page in the file, following *pagenum.
 * The page is fixed in the buffer pool.
 * Params: (int) fd - file descriptor.
 * (int*) pagenum - (in/out) current page on input, next page on output.
 * (char**) pagebuf - (out) placeholder for the page buffer.
 * Returns: PFE_OK if success, PFE_EOF if no more pages, or PF error code.
 */
extern int PF_GetNextPage(int fd, int *pagenum, char **pagebuf);

/*
 * PF_GetThisPage
 *
 * Desc: Get the specific page with page number 'pagenum'.
 * The page is fixed in the buffer pool.
 * Params: (int) fd - file descriptor.
 * (int) pagenum - page number to retrieve.
 * (char**) pagebuf - (out) placeholder for the page buffer.
 * Returns: PFE_OK if success, or a PF error code otherwise.
 */
extern int PF_GetThisPage(int fd, int pagenum, char **pagebuf);

/*
 * PF_AllocPage
 *
 * Desc: Allocate a new page in the file.
 * The new page is fixed in the buffer pool.
 * Params: (int) fd - file descriptor.
 * (int*) pagenum - (out) placeholder for the new page number.
 * (char**) pagebuf - (out) placeholder for the page buffer.
 * Returns: PFE_OK if success, or a PF error code otherwise.
 */
extern int PF_AllocPage(int fd, int *pagenum, char **pagebuf);

/*
 * PF_DisposePage
 *
 * Desc: Dispose of a page (mark it as free).
 * The page must not be fixed in the buffer.
 * Params: (int) fd - file descriptor.
 * (int) pagenum - page number to dispose.
 * Returns: PFE_OK if success, or a PF error code otherwise.
 */
extern int PF_DisposePage(int fd, int pagenum);

/*
 * PF_UnfixPage
 *
 * Desc: Unfix a page from the buffer pool.
 * The 'dirty' flag indicates whether the page was modified.
 * Params: (int) fd - file descriptor.
 * (int) pagenum - page number to unfix.
 * (int) dirty - TRUE if the page was modified, FALSE otherwise.
 * Returns: PFE_OK if success, or a PF error code otherwise.
 */
extern int PF_UnfixPage(int fd, int pagenum, int dirty);

/*
 * PF_MarkDirty
 *
 * Desc: Explicitly mark a fixed page as dirty.
 * (Required by assignment objective 1)
 * Params: (int) fd - file descriptor.
 * (int) pagenum - page number to mark as dirty.
 * Returns: PFE_OK if success, or a PF error code otherwise.
 */
extern int PF_MarkDirty(int fd, int pagenum);


/************************************************************
 * Statistics Interface
 ************************************************************/

/*
 * PF_ResetStats
 *
 * Desc: Reset all I/O statistics counters to zero.
 */
extern void PF_ResetStats();

/*
 * PF_GetLogicalIOs
 *
 * Desc: Get the total number of logical I/O requests
 * (requests to PF_GetThisPage, PF_GetFirstPage, etc.)
 * Returns: (long) The count of logical I/Os.
 */
extern long PF_GetLogicalIOs();

/*
 * PF_GetPhysicalIOs
 *
 * Desc: Get the total number of physical I/Os (reads + writes).
 * Returns: (long) The count of physical I/Os.
 */
extern long PF_GetPhysicalIOs();

/*
 * PF_GetDiskReads
 *
 * Desc: Get the total number of pages read from disk.
 * Returns: (long) The count of disk reads.
 */
extern long PF_GetDiskReads();

/*
 * PF_GetDiskWrites
 *
 * Desc: Get the total number of pages written to disk.
 * Returns: (long) The count of disk writes.
 */
extern long PF_GetDiskWrites();


/************************************************************
 * Error Handling
 ************************************************************/

/*
 * PF_PrintError
 *
 * Desc: Print a PF error message to stderr.
 * Params: (char*) s - a prefix string to print.
 */
extern void PF_PrintError(char *s);

#endif /* PF_H */
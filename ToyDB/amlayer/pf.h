/* pf.h: externs and error codes for Paged File Interface*/

/* ADDED include guard */
#ifndef PF_H
#define PF_H

#ifndef TRUE
#define TRUE 1		
#endif
#ifndef FALSE
#define FALSE 0
#endif

/*
 * ADDED: PF_Strategy enum definition
 * This was missing and caused the first error.
 */
typedef enum {
    PF_LRU,
    PF_MRU
} PF_Strategy;


/************** Error Codes *********************************/
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
#define PF_PAGE_SIZE	1020

/* externs from the PF layer */
extern int PFerrno;		/* error number of last error */
/* --- MODIFIED --- Corrected prototype to match pf.c */
extern void PF_PrintError(char *);

/* ADDED: Missing function prototypes */
extern void PF_Init(int); /* Changed to void */
extern int PF_CreateFile(char *);
extern int PF_DestroyFile(char *);
extern int PF_OpenFile(char *, PF_Strategy);
extern int PF_CloseFile(int);
extern int PF_GetFirstPage(int, int *, char **);
extern int PF_GetNextPage(int, int *, char **);
extern int PF_GetThisPage(int, int, char **);
extern int PF_AllocPage(int, int *, char **);
extern int PF_DisposePage(int, int);
extern int PF_UnfixPage(int, int, int);
extern int PF_MarkDirty(int, int);

/* Statistics functions */
extern void PF_ResetStats();
extern long PF_GetLogicalIOs();
extern long PF_GetPhysicalIOs();
extern long PF_GetDiskReads();
extern long PF_GetDiskWrites();


#endif /* PF_H */
/* pftypes.h: declarations for Paged File interface */

#ifndef PFTYPES_H
#define PFTYPES_H

/* Include pf.h to get PF_Strategy and error codes */
#include "pf.h" 

/**************************** File Page Decls *********************/
/* Each file contains a header, which is a integer pointing
to the first free page, or -1 if no more free pages in the file.
Followed by this header are the file pages as declared in struct PFfpage */
typedef struct PFhdr_str {
	int	firstfree;	/* first free page in the linked list of
				free pages */
	int	numpages;	/* # of pages in the file */
} PFhdr_str;

#define PF_HDR_SIZE sizeof(PFhdr_str)	/* size of file header */

/* actual page struct to be written onto the file */
#define PF_PAGE_LIST_END	-1	/* end of list of free pages */
#define PF_PAGE_USED		-2	/* page is being used */
typedef struct PFfpage {
	int nextfree;	/* page number of next free page in the linked
			list of free pages, or PF_PAGE_LIST_END if
			end of list, or PF_PAGE_USED if this page is not free */
	char pagebuf[PF_PAGE_SIZE];	/* actual page data */
} PFfpage;

/*************************** Opened File Table **********************/
#define PF_FTAB_SIZE	20	/* size of open file table */

/* open file table entry */
typedef struct PFftab_ele {
	char *fname;	/* file name, or NULL if entry not used */
	int unixfd;	/* unix file descriptor*/
	PFhdr_str hdr;	/* file header */
	short hdrchanged; /* TRUE if file header has changed */
    PF_Strategy strategy; /* Replacement strategy for this file (PF_LRU or PF_MRU) */
} PFftab_ele;

/*
 * The PF File Table.
 * This is defined in pf.c and made external here so that
 * the buffer manager (buf.c) can access it to find the
 * replacement strategy for a given file descriptor.
 */
extern PFftab_ele PFftab[PF_FTAB_SIZE];


/************************** Buffer Page Decls *********************/
/*
 * NOTE: PF_MAX_BUFS is no longer a compile-time constant.
 * It is now a variable, set by PF_Init() and stored within buf.c
 */

/* buffer page decl */
typedef struct PFbpage {
	struct PFbpage *nextpage;	/* next in the linked list of
					buffer page */
	struct PFbpage *prevpage;	/* previous in the linked list
					of buffer pages */
	short	dirty:1,		/* TRUE if page is dirty */
		fixed:1;		/* TRUE if page is fixed in buffer*/
	int	page;			/* page number of this page */
	int	fd;			/* file desciptor of this page */
	PFfpage fpage; /* page data from the file */
} PFbpage;



/******************** Hash Table Decls ****************************/
#define PF_HASH_TBL_SIZE	20	/* size of PF hash table */

/* Hash table bucket entries*/
typedef struct PFhash_entry {
	struct PFhash_entry *nextentry; /* next hash table element, or NULL */
	struct PFhash_entry *preventry;/*previous hash table element,or NULL*/
	int fd;		/* file descriptor */
	int page;	/* page number */
	struct PFbpage *bpage; /* pointer to buffer holding this page */
} PFhash_entry;

/* Hash function for hash table */
#define PFhash(fd,page) (((fd)+(page)) % PF_HASH_TBL_SIZE)

/******************* Interface functions from Hash Table ****************/
extern void PFhashInit();
extern PFbpage *PFhashFind(int fd, int page);
extern int PFhashInsert(int fd, int page, PFbpage *bpage);
extern int PFhashDelete(int fd, int page);
extern void PFhashPrint();

/****************** Interface functions from Buffer Manager *************/
/*
 * These are the internal functions of the buffer manager,
 * called by the PF layer.
 */

/* Initialize the buffer manager with a given size */
extern void PFbufInit(int bufsize);

/* Get a page from the buffer */
extern int PFbufGet(int fd, int pagenum, PFfpage **fpage,
                     int (*readfcn)(int, int, PFfpage*),
                     int (*writefcn)(int, int, PFfpage*));

/* Unfix a page from the buffer */
extern int PFbufUnfix(int fd, int pagenum, int dirty);

/* Allocate a new page in the buffer */
extern int PFbufAlloc(int fd, int pagenum, PFfpage **fpage,
                       int (*writefcn)(int, int, PFfpage*));

/* Release all pages for a given file */
extern int PFbufReleaseFile(int fd, int (*writefcn)(int, int, PFfpage*));

/* Mark a page as used (and dirty) */
extern int PFbufUsed(int fd, int pagenum);

/* Print buffer contents (for debugging) */
extern void PFbufPrint();


// Explicitly mark a fixed page as dirty
extern int PFbufMarkDirty(int fd, int pagenum);

// Statistics functions 
extern void PFbufResetStats();
extern long PFbufGetLogicalIOs();
extern long PFbufGetPhysicalIOs();
extern long PFbufGetDiskReads();
extern long PFbufGetDiskWrites();

#endif /* PFTYPES_H */
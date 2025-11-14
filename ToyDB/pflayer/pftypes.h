/* pftypes.h: types for PF layer */
/* This file is included by pf.c and buf.c */

/* size of page */
#define PF_PAGE_SIZE 4096

/* page file header */
typedef struct PFhdr_str {
	int	firstfree;	/* first free page in the file */
	int	numpages;	/* # of pages in the file */
} PFhdr_str;

/* page data */
typedef struct PFfpage {
	char pagebuf[PF_PAGE_SIZE];
} PFfpage;

/* page buffer entry */
typedef struct PFbpage {
	PFfpage fpage;		/* page data */
	struct PFbpage *nextpage;	/* next in the buffer list */
	struct PFbpage *prevpage;	/* prev in the buffer list */
	int fd;			/* file descriptor */
	int page;		/* page number */
	unsigned int fixed:1;	/* TRUE if page is fixed */
	unsigned int dirty:1;	/* TRUE if page is dirty */
} PFbpage;

/* hash table entry */
typedef struct PFhash_entry {
	int fd;			/* file descriptor */
	int page;		/* page number */
	PFbpage *bpage;		/* ptr to buffer page */
	struct PFhash_entry *nextentry; /* next hash table entry */
} PFhash_entry;

/* file table entry */
typedef struct PFftab_ele {
	char *fname;		/* file name */
	int unixfd;		/* unix file descriptor */
	PFhdr_str hdr;		/* file header */
	unsigned int hdrchanged:1;	/* TRUE if header changed */
} PFftab_ele;


/* max # of files open at the same time */
#define PF_FTAB_SIZE 20

/* max # of pages in the buffer */
#define PF_MAX_BUFS 20

/* end of page list */
#define PF_PAGE_LIST_END -1

/* hash table size */
#define PF_HASH_TBL_SIZE 20
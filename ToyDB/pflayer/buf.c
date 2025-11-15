/* buf.c: buffer management routines. The interface routines are:
PFbufGet(), PFbufUnfix(), PFbufAlloc(), PFbufReleaseFile(), PFbufUsed() and
PFbufPrint() */

#include <stdio.h>
#include <stdlib.h> /* For malloc */
#include "pf.h"
#include "pftypes.h"

/* Global static variables for the buffer manager */
static int PFnumbpage = 0;	/* # of buffer pages in memory */
static int PF_MAX_BUFS;     /* Max # of buffers, set by PFbufInit */

static PFbpage *PFfirstbpage= NULL;	/* ptr to first buffer page (MRU), or NULL */
static PFbpage *PFlastbpage = NULL;	/* ptr to last buffer page (LRU), or NULL */
static PFbpage *PFfreebpage= NULL;	/* list of free buffer pages */

/* Statistics Counters */
static long PF_logical_ios = 0;
static long PF_physical_ios = 0;
static long PF_disk_reads = 0;
static long PF_disk_writes = 0;

/****************************************************************************
 * Internal Buffer List Management Routines
 ****************************************************************************/

static void PFbufInsertFree(PFbpage *bpage)
/****************************************************************************
SPECIFICATIONS:
	Insert the buffer page pointed by "bpage" into the free list.
*****************************************************************************/
{
	bpage->nextpage = PFfreebpage;
	PFfreebpage = bpage;
}


static void PFbufLinkHead(PFbpage *bpage)
/****************************************************************************
SPECIFICATIONS:
	Link the buffer page pointed by "bpage" as the head (MRU)
	of the used buffer list. No other field of bpage is modified.
*****************************************************************************/
{
	bpage->nextpage = PFfirstbpage;
	bpage->prevpage = NULL;
	if (PFfirstbpage != NULL)
		PFfirstbpage->prevpage = bpage;
	PFfirstbpage = bpage;
	if (PFlastbpage == NULL)
		PFlastbpage = bpage;
}

/*
 * *******************************************************************
 * NEW FUNCTION TO LINK A PAGE AT THE TAIL (LRU POSITION)
 * *******************************************************************
 */
static void PFbufLinkTail(PFbpage *bpage)
/****************************************************************************
SPECIFICATIONS:
	Link the buffer page pointed by "bpage" as the tail (LRU)
	of the used buffer list.
*****************************************************************************/
{
    bpage->nextpage = NULL;
    bpage->prevpage = PFlastbpage;
    if (PFlastbpage != NULL)
        PFlastbpage->nextpage = bpage;
    PFlastbpage = bpage;
    if (PFfirstbpage == NULL)
        PFfirstbpage = bpage;
}
	
void PFbufUnlink(PFbpage *bpage)
/****************************************************************************
SPECIFICATIONS:
	Unlink the page pointed by bpage from the buffer list. Assume
	that bpage is a valid pointer.  Set the "prevpage" and "nextpage"
	fields to NULL. The caller is responsible to either place
	the unlinked page into the free list, or insert it back
	into the used list.
*****************************************************************************/
{
	if (PFfirstbpage == bpage)
		PFfirstbpage = bpage->nextpage;
	
	if (PFlastbpage == bpage)
		PFlastbpage = bpage->prevpage;
	
	if (bpage->nextpage != NULL)
		bpage->nextpage->prevpage = bpage->prevpage;
	
	if (bpage->prevpage != NULL)
		bpage->prevpage->nextpage = bpage->nextpage;

	bpage->prevpage = bpage->nextpage = NULL;
}


static int PFbufInternalAlloc(PFbpage **bpage, int (*writefcn)(int, int, PFfpage*), int fd)
/****************************************************************************
SPECIFICATIONS:
	Allocate a buffer page and set *bpage to point to it. *bpage
	is set to NULL if one can not be allocated.
	writefcn() is used to write pages.
    fd is the file descriptor for the *new* page, used to determine
    the replacement strategy for eviction.

ALGORITHM:
    **VICTIM SELECTION (Objective 1):**
    - If strategy for `fd` is PF_LRU, finds victim from tail (PFlastbpage).
    - If strategy for `fd` is PF_MRU, finds victim from head (PFfirstbpage).
*****************************************************************************/
{
    PFbpage *tbpage;	/* temporary pointer to buffer page */
    int error;		/* error value returned*/
    PF_Strategy strategy; /* Replacement strategy */

	/* Set *bpage to the buffer page to be returned */
	if (PFfreebpage != NULL){
		/* Free list not empty, use the one from the free list. */
		*bpage = PFfreebpage;
		PFfreebpage = (*bpage)->nextpage;
	}
	else if (PFnumbpage < PF_MAX_BUFS){
		/* We have not reached max buffer limit, so
		malloc() a new one */
		if ((*bpage=(PFbpage *)malloc(sizeof(PFbpage)))==NULL){
			/* no mem */
			*bpage = NULL;
			PFerrno = PFE_NOMEM;
			return(PFerrno);
		}
		/* increment # of pages allocated */
		PFnumbpage++;
	}
	else {
		/* we have reached max buffer limit */
		/* choose a victim from the buffer based on strategy */

		*bpage = NULL;		/* set initial return value */
        
        /* Look up the strategy for the file requesting the page */
        strategy = PFftab[fd].strategy;

        if (strategy == PF_LRU)
        {
            /* LRU: Scan from the tail (Least Recently Used) backwards */
            for (tbpage = PFlastbpage; tbpage != NULL; tbpage = tbpage->prevpage){
                if (!tbpage->fixed)
                    /* found a page that can be swapped out */
                    break;
            }
        }
        else /* strategy == PF_MRU */
        {
            /* MRU: Scan from the head (Most Recently Used) forwards */
            for (tbpage = PFfirstbpage; tbpage != NULL; tbpage = tbpage->nextpage){
                if (!tbpage->fixed)
                    /* found a page that can be swapped out */
                    break;
            }
        }

		if (tbpage == NULL){
			/* couldn't find an un-fixed page */
			PFerrno = PFE_NOBUF;
			return(PFerrno);
		}

		/* write out the dirty page */
		if (tbpage->dirty) {
            if((error=(*writefcn)(tbpage->fd, tbpage->page, &tbpage->fpage)) != PFE_OK)
			    return(error);
            
            /* Increment Write Statistics */
            PF_disk_writes++;
            PF_physical_ios++;
        }
		tbpage->dirty = FALSE;

		/* unlink from hash table */
		if ((error=PFhashDelete(tbpage->fd,tbpage->page))!= PFE_OK)
			return(error);
		
		/* unlink from buffer list */
		PFbufUnlink(tbpage);

		*bpage = tbpage;
	}

    /*
     * *******************************************************************
     * MODIFICATION: Link new page at HEAD for LRU, TAIL for MRU
     * When a new page is brought in:
     * - For LRU, it's the "newest" (MRU), so link at head.
     * - For MRU, it's the "newest" (MRU), but we also want to protect
     * older pages. For a sequential scan, linking at the tail
     * is also fine, but linking at the head is the standard
     * "new page" behavior. We will link at the head here, and
     * make the main change in PFbufUnfix.
     * *******************************************************************
     */
	PFbufLinkHead(*bpage);
	return(PFE_OK);
}


/************************* Interface to the Outside World ****************/

void PFbufInit(int bufsize)
/****************************************************************************
SPECIFICATIONS:
	Initialize the buffer manager.
*****************************************************************************/
{
    PF_MAX_BUFS = bufsize;
    PFnumbpage = 0;
	PFfirstbpage= NULL;
	PFlastbpage = NULL;
	PFfreebpage= NULL;
    PFbufResetStats();
}


int PFbufGet(int fd, int pagenum, PFfpage **fpage,
             int (*readfcn)(int, int, PFfpage*),
             int (*writefcn)(int, int, PFfpage*))
/****************************************************************************
SPECIFICATIONS:
	Get a page whose number is "pagenum" from the file pointed
	by "fd". Set *fpage to point to the data for that page.
*****************************************************************************/
{
    PFbpage *bpage;	/* pointer to buffer */
    int error;

    /* A request for a page is one logical I/O */
    PF_logical_ios++;

	if ((bpage=PFhashFind(fd,pagenum)) == NULL){
		/* page not in buffer. */
		
		/* allocate an empty page */
		if ((error=PFbufInternalAlloc(&bpage, writefcn, fd))!= PFE_OK){
			*fpage = NULL;
			return(error);
		}
		
		/* read the page from disk */
		if ((error=(*readfcn)(fd, pagenum, &bpage->fpage))!= PFE_OK){
			PFbufUnlink(bpage);
			PFbufInsertFree(bpage);
			*fpage = NULL;
			return(error);
		}

        /* Increment Read Statistics (this was a page miss) */
        PF_disk_reads++;
        PF_physical_ios++;

		/* insert new page into hash table */
		if ((error=PFhashInsert(fd,pagenum,bpage))!=PFE_OK){
			PFbufUnlink(bpage);
			PFbufInsertFree(bpage);
			return(error);
		}

		/* set the fields for this page*/
		bpage->fd = fd;
		bpage->page = pagenum;
		bpage->dirty = FALSE;
	}
	else if (bpage->fixed){
		/* page already in memory, and is fixed */
		*fpage = &bpage->fpage;
		PFerrno = PFE_PAGEFIXED;
		return(PFerrno);
	}

    /*
     * *******************************************************************
     * MODIFICATION: On a buffer HIT, we must also re-link the page
     * according to the strategy.
     * *******************************************************************
     */
    PFbufUnlink(bpage);
    if (PFftab[fd].strategy == PF_LRU) {
        PFbufLinkHead(bpage);
    } else {
        PFbufLinkTail(bpage); /* For MRU, move to tail to protect it */
    }

	/* Fix the page in the buffer then return*/
	bpage->fixed = TRUE;
	*fpage = &bpage->fpage;
	return(PFE_OK);
}

int PFbufUnfix(int fd, int pagenum, int dirty)
/****************************************************************************
SPECIFICATIONS:
	Unfix the file page whose number is "pagenum" from the buffer.
*****************************************************************************/
{
    PFbpage *bpage;
    PF_Strategy strategy;

	if ((bpage= PFhashFind(fd,pagenum))==NULL){
		/* page not in buffer */
		PFerrno = PFE_PAGENOTINBUF;
		return(PFerrno);
	}

	if (!bpage->fixed){
		/* page already unfixed */
		PFerrno = PFE_PAGEUNFIXED;
		return(PFerrno);
	}

	if (dirty)
		/* mark this page dirty */
		bpage->dirty = TRUE;
	
	/* unfix the page */
	bpage->fixed = FALSE;
	
    /*
     * *******************************************************************
     * MODIFICATION: Relink the page based on the strategy
     * - For LRU, unfixing moves it to the HEAD (it's now MRU).
     * - For MRU, unfixing moves it to the TAIL (it's now LRU,
     * so it is protected from eviction).
     * *******************************************************************
     */
	
    /* Get the strategy for this file */
    strategy = PFftab[fd].strategy;
    
    /* unlink this page */
	PFbufUnlink(bpage);

    if (strategy == PF_LRU) {
	    /* insert it as head of linked list to make it most recently used*/
	    PFbufLinkHead(bpage);
    } else { /* strategy == PF_MRU */
        /* insert it as tail of linked list to make it least recently used*/
        PFbufLinkTail(bpage);
    }

	return(PFE_OK);
}

int PFbufAlloc(int fd, int pagenum, PFfpage **fpage, int (*writefcn)(int, int, PFfpage*))
/****************************************************************************
SPECIFICATIONS:
	Allocate a buffer and mark it belonging to page "pagenum"
	of file "fd".  Set *fpage to point to the buffer data.
*****************************************************************************/
{
    PFbpage *bpage;
    int error;

	*fpage = NULL;	/* initial value of fpage */

	if ((bpage=PFhashFind(fd,pagenum))!= NULL){
		/* page already in buffer*/
		PFerrno = PFE_PAGEINBUF;
		return(PFerrno);
	}

	if ((error=PFbufInternalAlloc(&bpage, writefcn, fd))!= PFE_OK)
		/* can't get any buffer */
		return(error);
	
	/* put ourselves into the hash table */
	if ((error=PFhashInsert(fd,pagenum,bpage))!= PFE_OK){
		/* can't insert into the hash table */
		PFbufUnlink(bpage);
		PFbufInsertFree(bpage);
		return(error);
	}

	/* init the fields of bpage and return */
	bpage->fd = fd;
	bpage->page = pagenum;
	bpage->fixed = TRUE;
	bpage->dirty = FALSE;

	*fpage = &bpage->fpage;
	return(PFE_OK);
}


int PFbufReleaseFile(int fd, int (*writefcn)(int, int, PFfpage*))
/****************************************************************************
SPECIFICATIONS:
	Release all pages of file "fd" from the buffer and
	put them into the free list 
*****************************************************************************/
{
    PFbpage *bpage;	/* ptr to buffer pages to search */
    PFbpage *temppage;
    int error;		/* error code */

	/* Do linear scan of the buffer to find pages belonging to the file */
	bpage = PFfirstbpage;
	while (bpage != NULL){
		if (bpage->fd == fd){
			/* The file descriptor matches*/
			if (bpage->fixed){
				PFerrno = PFE_PAGEFIXED;
				return(PFerrno);
			}

			/* write out dirty page */
			if (bpage->dirty) {
                if((error=(*writefcn)(fd,bpage->page, &bpage->fpage))!= PFE_OK)
				    return(error);

                /* Increment Write Statistics */
                PF_disk_writes++;
                PF_physical_ios++;
            }
			bpage->dirty = FALSE;

			/* get rid of it from the hash table */
			if ((error=PFhashDelete(fd,bpage->page))!= PFE_OK){
				/* internal error */
				printf("Internal error:PFbufReleaseFile()\n");
				exit(1);
			}

			/* put the page into free list */
			temppage = bpage;
			bpage = bpage->nextpage;
			PFbufUnlink(temppage);
			PFbufInsertFree(temppage);

		}
		else	bpage = bpage->nextpage;
	}
	return(PFE_OK);
}


int PFbufUsed(int fd, int pagenum)
/****************************************************************************
SPECIFICATIONS:
	Mark page numbered "pagenum" of file descriptor "fd" as used.
	The page must be fixed in the buffer.
*****************************************************************************/
{
    PFbpage *bpage;	/* pointer to the bpage we are looking for */

	/* Find page in the buffer */
	if ((bpage=PFhashFind(fd,pagenum))==NULL){
		/* page not in the buffer */
		PFerrno = PFE_PAGENOTINBUF;
		return(PFerrno);
	}

	if (!(bpage->fixed)){
		/* page not fixed */
		PFerrno = PFE_PAGEUNFIXED;
		return(PFerrno);
	}

	/* mark this page dirty */
	bpage->dirty = TRUE;

    /*
     * *******************************************************************
     * MODIFICATION: Relink the page based on the strategy
     * This function is called by PF_AllocPage, which is a "new" page.
     * *******************************************************************
     */
	PFbufUnlink(bpage);
    if (PFftab[fd].strategy == PF_LRU) {
	    PFbufLinkHead(bpage);
    } else {
        PFbufLinkTail(bpage);
    }

	return(PFE_OK);
}

int PFbufMarkDirty(int fd, int pagenum)
/****************************************************************************
SPECIFICATIONS:
	Explicitly mark a fixed page as dirty.
*****************************************************************************/
{
    PFbpage *bpage;

    /* Find page in the buffer */
	if ((bpage=PFhashFind(fd,pagenum))==NULL){
		/* page not in the buffer */
		PFerrno = PFE_PAGENOTINBUF;
		return(PFerrno);
	}

    /* Page must be fixed to be marked dirty */
	if (!(bpage->fixed)){
		/* page not fixed */
		PFerrno = PFE_PAGEUNFIXED;
		return(PFerrno);
	}

    /* Mark dirty */
    bpage->dirty = TRUE;
    return(PFE_OK);
}


void PFbufPrint()
/****************************************************************************
SPECIFICATIONS:
	Print the current page buffers.
*****************************************************************************/
{
    PFbpage *bpage;

	printf("buffer content:\n");
	if (PFfirstbpage == NULL)
		printf("empty\n");
	else {
		printf("fd\tpage\tfixed\tdirty\taddr\n");
		for(bpage = PFfirstbpage; bpage != NULL; bpage= bpage->nextpage)
			printf("%d\t%d\t%d\t%d\t%p\n",
				bpage->fd,bpage->page,(int)bpage->fixed,
				(int)bpage->dirty, (void*)&bpage->fpage);
	}
}


/****************************************************************************
 * Statistics Interface Functions
 ****************************************************************************/

void PFbufResetStats()
{
    PF_logical_ios = 0;
    PF_physical_ios = 0;
    PF_disk_reads = 0;
    PF_disk_writes = 0;
}

long PFbufGetLogicalIOs()
{
    return PF_logical_ios;
}

long PFbufGetPhysicalIOs()
{
    return PF_physical_ios;
}

long PFbufGetDiskReads()
{
    return PF_disk_reads;
}

long PFbufGetDiskWrites()
{
    return PF_disk_writes;
}
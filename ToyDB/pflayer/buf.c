#include <stdio.h>
#include <stdlib.h> /* For malloc */
#include "pf.h"
#include "pftypes.h"

/* Global static variables for the buffer manager */
static int PFnumbpage = 0;	/* # of buffer pages in memory */
static int PF_MAX_BUFS;     /* Max # of buffers, set by PFbufInit */

static PFbpage *PFfirstbpage= NULL;
static PFbpage *PFlastbpage = NULL;
static PFbpage *PFfreebpage= NULL;

/* Statistics Counters */
static long PF_logical_ios = 0;
static long PF_physical_ios = 0;
static long PF_disk_reads = 0;
static long PF_disk_writes = 0;

/****************************************************************************
 * Internal Buffer List Management Routines
 ****************************************************************************/

static void PFbufInsertFree(PFbpage *bpage)
{
	bpage->nextpage = PFfreebpage;
	PFfreebpage = bpage;
}


static void PFbufLinkHead(PFbpage *bpage)
{
	bpage->nextpage = PFfirstbpage;

	bpage->prevpage = NULL;

	if (PFfirstbpage != NULL)
		PFfirstbpage->prevpage = bpage;

	PFfirstbpage = bpage;

	if (PFlastbpage == NULL)
		PFlastbpage = bpage;
}

// New function to link a page at the tail (LRU position)
static void PFbufLinkTail(PFbpage *bpage) {

    bpage->nextpage = NULL;
    
	bpage->prevpage = PFlastbpage;
    
	if (PFlastbpage != NULL)
        PFlastbpage->nextpage = bpage;
    
	PFlastbpage = bpage;
    
	if (PFfirstbpage == NULL)
        PFfirstbpage = bpage;

}
	
void PFbufUnlink(PFbpage *bpage)
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
{
    PFbpage *tbpage;
    int error;	
    PF_Strategy strategy; 

	if (PFfreebpage != NULL){
		*bpage = PFfreebpage;
		PFfreebpage = (*bpage)->nextpage;
	}
	else if (PFnumbpage < PF_MAX_BUFS){
		if ((*bpage=(PFbpage *)malloc(sizeof(PFbpage)))==NULL){
			*bpage = NULL;
			PFerrno = PFE_NOMEM;
			return(PFerrno);
		}
		PFnumbpage++;
	}

	else {
		*bpage = NULL;		
        
        // Look up the strategy for the file requesting the page
        strategy = PFftab[fd].strategy;

        if (strategy == PF_LRU)
        {
            // LRU: Scan from the tail (Least Recently Used) backwards
            for (tbpage = PFlastbpage; tbpage != NULL; tbpage = tbpage->prevpage){
                if (!tbpage->fixed)
                    break;
            }
        }
        else // strategy == PF_MRU
        {
            // MRU: Scan from the head (Most Recently Used) forwards
            for (tbpage = PFfirstbpage; tbpage != NULL; tbpage = tbpage->nextpage){
                if (!tbpage->fixed)
                    break;
            }
        }

		if (tbpage == NULL){
			PFerrno = PFE_NOBUF;
			return(PFerrno);
		}

		if (tbpage->dirty) {
            if((error=(*writefcn)(tbpage->fd, tbpage->page, &tbpage->fpage)) != PFE_OK)
			    return(error);
            
            PF_disk_writes++;
            PF_physical_ios++;
        }
		tbpage->dirty = FALSE;

		if ((error=PFhashDelete(tbpage->fd,tbpage->page))!= PFE_OK)
			return(error);
		
		PFbufUnlink(tbpage);
		*bpage = tbpage;
	}

 
    // Page at head is most recently used
	PFbufLinkHead(*bpage);
	return(PFE_OK);
}



void PFbufInit(int bufsize)
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
{
    PFbpage *bpage;	/* pointer to buffer */
    int error;

    PF_logical_ios++;

	if ((bpage=PFhashFind(fd,pagenum)) == NULL){
		/* page not in buffer. */
		if ((error=PFbufInternalAlloc(&bpage, writefcn, fd))!= PFE_OK){
			*fpage = NULL;
			return(error);
		}
		
		if ((error=(*readfcn)(fd, pagenum, &bpage->fpage))!= PFE_OK){
			PFbufUnlink(bpage);
			PFbufInsertFree(bpage);
			*fpage = NULL;
			return(error);
		}

        PF_disk_reads++;
        PF_physical_ios++;

		if ((error=PFhashInsert(fd,pagenum,bpage))!=PFE_OK){
			PFbufUnlink(bpage);
			PFbufInsertFree(bpage);
			return(error);
		}

		bpage->fd = fd;
		bpage->page = pagenum;
		bpage->dirty = FALSE;
	}
	else if (bpage->fixed){
		*fpage = &bpage->fpage;
		PFerrno = PFE_PAGEFIXED;
		return(PFerrno);
	}

    /*
     * Page's position is NOT updated here.
     * It is only updated when PFbufUnfix is called.
     */

	/* Fix the page in the buffer then return*/
	bpage->fixed = TRUE;
	*fpage = &bpage->fpage;
	return(PFE_OK);
}

int PFbufUnfix(int fd, int pagenum, int dirty)
{
    PFbpage *bpage;
    PF_Strategy strategy;

	if ((bpage= PFhashFind(fd,pagenum))==NULL){
		PFerrno = PFE_PAGENOTINBUF;
		return(PFerrno);
	}

	if (!bpage->fixed){
		PFerrno = PFE_PAGEUNFIXED;
		return(PFerrno);
	}

	if (dirty)
		bpage->dirty = TRUE;
	
	bpage->fixed = FALSE;
	
    /*
     Relink the page based on the strategy
     - For LRU, unfixing moves it to the HEAD (it's now MRU).
     - For MRU, unfixing moves it to the TAIL (it's now LRU, so it is protected from eviction).
     */
	
    strategy = PFftab[fd].strategy;
	fprintf(stderr, "DEBUG: PF_OpenFile fd=%d strategy=%d (macro)\n", fd, PFftab[fd].strategy);

    
	PFbufUnlink(bpage);
	PFbufLinkHead(bpage); 

	return(PFE_OK);
}

int PFbufAlloc(int fd, int pagenum, PFfpage **fpage, int (*writefcn)(int, int, PFfpage*))
{
    PFbpage *bpage;
    int error;

	*fpage = NULL;	

	if ((bpage=PFhashFind(fd,pagenum))!= NULL){
		PFerrno = PFE_PAGEINBUF;
		return(PFerrno);
	}

	if ((error=PFbufInternalAlloc(&bpage, writefcn, fd))!= PFE_OK)
		return(error);
	
	if ((error=PFhashInsert(fd,pagenum,bpage))!= PFE_OK){
		PFbufUnlink(bpage);
		PFbufInsertFree(bpage);
		return(error);
	}

	bpage->fd = fd;
	bpage->page = pagenum;
	bpage->fixed = TRUE;
	bpage->dirty = FALSE;

	*fpage = &bpage->fpage;
	return(PFE_OK);
}


int PFbufReleaseFile(int fd, int (*writefcn)(int, int, PFfpage*))
{
    PFbpage *bpage;	
    PFbpage *temppage;
    int error;		

	bpage = PFfirstbpage;
	while (bpage != NULL){
		if (bpage->fd == fd){
			if (bpage->fixed){
				PFerrno = PFE_PAGEFIXED;
				return(PFerrno);
			}

			if (bpage->dirty) {
                if((error=(*writefcn)(fd,bpage->page, &bpage->fpage))!= PFE_OK)
				    return(error);

                PF_disk_writes++;
                PF_physical_ios++;
            }
			bpage->dirty = FALSE;

			if ((error=PFhashDelete(fd,bpage->page))!= PFE_OK){
				printf("Internal error:PFbufReleaseFile()\n");
				exit(1);
			}

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
{
    PFbpage *bpage;	

	if ((bpage=PFhashFind(fd,pagenum))==NULL){
		PFerrno = PFE_PAGENOTINBUF;
		return(PFerrno);
	}

	if (!(bpage->fixed)){
		PFerrno = PFE_PAGEUNFIXED;
		return(PFerrno);
	}

	bpage->dirty = TRUE;

	return(PFE_OK);
}

int PFbufMarkDirty(int fd, int pagenum)
{
    PFbpage *bpage;

	if ((bpage=PFhashFind(fd,pagenum))==NULL){
		PFerrno = PFE_PAGENOTINBUF;
		return(PFerrno);
	}

	if (!(bpage->fixed)){
		PFerrno = PFE_PAGEUNFIXED;
		return(PFerrno);
	}

    bpage->dirty = TRUE;
    return(PFE_OK);
}


void PFbufPrint()
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



// Statistics Interface Functions

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
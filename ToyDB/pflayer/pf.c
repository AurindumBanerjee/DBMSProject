/* pf.c: Paged File Interface Routines+ support routines */
#include <stdio.h>
#include <stdlib.h>     /* for malloc, exit */
#include <string.h>     /* for strlen, strcpy, memset */
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>     /* for lseek, write, read, close */
#include "pf.h"
#include "pftypes.h"

/* To keep system V and PC users happy */
#ifndef L_SET
#define L_SET 0
#endif

int PFerrno = PFE_OK;	/* last error message */

static PFftab_ele PFftab[PF_FTAB_SIZE]; /* table of opened files */

/* true if file descriptor fd is invaild */
#define PFinvalidFd(fd) ((fd) < 0 || (fd) >= PF_FTAB_SIZE \
				|| PFftab[fd].fname == NULL)

/* true if page number "pagenum" of file "fd" is invalid in the
sense that it's <0 or >= # of pages in the file */
#define PFinvalidPagenum(fd,pagenum) ((pagenum)<0 || (pagenum) >= \
				PFftab[fd].hdr.numpages)

/* Prototypes for buffer manager functions (from buf.c) */
extern void PFbufInit();
extern int PFbufGet(int fd, int pagenum, char **pagebuf);
extern int PFbufAlloc(int fd, int pagenum, char **pagebuf);
extern int PFbufUnfix(int fd, int pagenum, int dirty);
extern int PFbufReleaseFile(int fd);
extern int PFbufUsed(int fd, int pagenum);


/****************** Internal Support Functions *****************************/\
static char *savestr(str)
char *str;		/* string to be saved */
/****************************************************************************
SPECIFICATIONS:
	Allocate memory and save string pointed by str.
	Return a pointer to the saved string, or NULL if no memory.
*****************************************************************************/
{
char *s;

	if ((s=malloc(strlen(str)+1))!= NULL)
		strcpy(s,str);
	return(s);
}

/****************************************************************************
SPECIFICATIONS:
	Find a free file table entry.
	Return the index of the free entry, or PFE_FTABFULL if
	no more free entry.

AUTHOR: clc
*****************************************************************************/
static int PFfindFreeFtab()
{
int i;

	for (i=0; i < PF_FTAB_SIZE; i++)
		if (PFftab[i].fname == NULL)
			return(i);

	PFerrno = PFE_FTABFULL;
	return(PFerrno);
}

/************************** Interface Functions ****************************/
void PF_Init()
/****************************************************************************
SPECIFICATIONS:
	PF_Init: Initialize the paged file layer.
	It initializes the hash table, and the buffer pool.
	This function must be called before any other PF functions.

AUTHOR: clc
*****************************************************************************/
{
int i;

	/* initialize the file table */
	for (i=0; i < PF_FTAB_SIZE; i++)
		PFftab[i].fname = NULL;

	/* Initialize the buffer manager */
	PFbufInit();
}


int PF_CreateFile(fname)
char *fname;		/* file name */
/****************************************************************************
SPECIFICATIONS:
	Create a paged file named "fname".

AUTHOR: clc

RETURN VALUE:
	PFE_OK	if OK
	PF error code if error.
*****************************************************************************/
{
int fd;	/* unix file descriptor */
int numbytes;	/* # of bytes written */
PFhdr_str hdr;	/* file header */

	/* create the file */
	if ((fd=creat(fname,0666)) < 0){
		PFerrno = PFE_UNIX;
		return(PFerrno);
	}

	/* write the header */
	hdr.firstfree = PF_PAGE_LIST_END;
	hdr.numpages = 0;
	if ((numbytes=write(fd,(char *)&hdr,sizeof(hdr))) != sizeof(hdr)){
		/* error writing header */
		close(fd);
		unlink(fname);
		PFerrno = PFE_HDRWRITE;
		return(PFerrno);
	}

	/* close the file */
	if (close(fd) < 0){
		PFerrno = PFE_UNIX;
		return(PFerrno);
	}

	return(PFE_OK);
}


int PF_DestroyFile(fname)
char *fname;
/****************************************************************************
SPECIFICATIONS:
	Destroy the paged file "fname".

AUTHOR: clc

RETURN VALUE:
	PFE_OK	if OK,
	PF error code if error.
*****************************************************************************/
{
	if (unlink(fname) < 0){
		PFerrno = PFE_UNIX;
		return(PFerrno);
	}
	else
		return(PFE_OK);
}


int PF_OpenFile(fname)
char *fname;		/* file name */
/****************************************************************************
SPECIFICATIONS:
	Open the paged file "fname".
	The file header is read into the file table.
	A file descriptor is returned, which is an index into the
	file table.

AUTHOR: clc

RETURN VALUE:
	file descriptor(>=0) if OK
	PF error code if error.

*****************************************************************************/
{
int fd;	/* file table entry */
int unixfd;	/* unix file descriptor */
int numbytes;	/* # of bytes read */
PFhdr_str hdr;	/* file header */

	/* find a free file table entry */
	if ((fd=PFfindFreeFtab()) < 0){
		PFerrno = PFE_FTABFULL;
		return(PFerrno);
	}

	/* open the file */
	if ((unixfd = open(fname,O_RDWR)) < 0){
		PFerrno = PFE_UNIX;
		return(PFerrno);
	}

	/* * *** ADDED FIX ***
	 * Ensure fd is not 0, 1, or 2 (stdin, stdout, stderr)
	 */
	if (unixfd == 0 || unixfd == 1 || unixfd == 2) {
		int newfd = fcntl(unixfd, F_DUPFD, 3); /* Find first fd >= 3 */
		if (newfd < 0) {
			PFerrno = PFE_UNIX;
			close(unixfd);
			return(PFerrno);
		}
		close(unixfd);
		unixfd = newfd;
	}
	/* *** END OF FIX *** */


	/* read the header */
	if ((numbytes=read(unixfd,(char *)&hdr,sizeof(hdr))) != sizeof(hdr)){
		close(unixfd);
		PFerrno = PFE_HDRREAD;
		return(PFerrno);
	}

	/* save the file name */
	if ((PFftab[fd].fname = savestr(fname)) == NULL){
		close(unixfd);
		PFerrno = PFE_NOMEM;
		return(PFerrno);
	}

	/* save unix file descriptor and header */
	PFftab[fd].unixfd = unixfd;
	PFftab[fd].hdr = hdr;
	PFftab[fd].hdrchanged = FALSE;

	/* return the file descriptor */
	return(fd);
}


int PF_CloseFile(fd)
int fd;			/* file descriptor */
/****************************************************************************
SPECIFICATIONS:
	Close the paged file with file descriptor "fd".
	The file header is written to the file if changed.
	The file table entry is freed.

AUTHOR: clc

RETURN VALUE:
	PFE_OK	if OK
	PF error code if error.

*****************************************************************************/
{
int err;
int numbytes;

	if (PFinvalidFd(fd)){
		PFerrno = PFE_FD;
		return(PFerrno);
	}

	/* Flush all dirty pages for this file from the buffer */
	if ((err = PFbufReleaseFile(fd)) != PFE_OK)
		return(err);

	/* if header changed, write it back */
	if (PFftab[fd].hdrchanged){
		if (lseek(PFftab[fd].unixfd, (long)0, L_SET) < 0){
			PFerrno = PFE_UNIX;
			return(PFerrno);
		}
		if ((numbytes=write(PFftab[fd].unixfd,
		    (char *)&(PFftab[fd].hdr),
		    sizeof(PFftab[fd].hdr))) != sizeof(PFftab[fd].hdr)){
			PFerrno = PFE_HDRWRITE;
			return(PFerrno);
		}
	}

	/* close the file */
	if (close(PFftab[fd].unixfd) < 0){
		PFerrno = PFE_UNIX;
		return(PFerrno);
	}

	/* free file table entry */
	free(PFftab[fd].fname);
	PFftab[fd].fname = NULL;

	return(PFE_OK);
}


int PF_AllocPage(fd,pagenum,pagebuf)
int fd;			/* file descriptor */
int *pagenum;		/* page number of the allocated page */
char **pagebuf;		/* buffer address of the allocated page */
/****************************************************************************
SPECIFICATIONS:
	Allocate a new page for file "fd".
	The page number is returned in "pagenum", and the buffer
	address of the page is returned in "pagebuf".
	The page is fixed in the buffer.

AUTHOR: clc

RETURN VALUE:
	PFE_OK	if OK
	PF error code if error.
*****************************************************************************/
{
int err;
int page;

	if (PFinvalidFd(fd)){
		PFerrno = PFE_FD;
		return(PFerrno);
	}

	if (PFftab[fd].hdr.firstfree != PF_PAGE_LIST_END){
		/* has a free page. get it */
		page = *pagenum = PFftab[fd].hdr.firstfree;
		
		/* get the page */
		if ((err=PFbufGet(fd,page,pagebuf))!= PFE_OK)
			return(err);
		
		/* update the header */
		PFftab[fd].hdr.firstfree = *((int *)*pagebuf);
		PFftab[fd].hdrchanged = TRUE;
		return(PFE_OK);
	}

	/* no free page */
	*pagenum = PFftab[fd].hdr.numpages;
	
	/* add new page */
	PFftab[fd].hdr.numpages++;
	PFftab[fd].hdrchanged = TRUE;

	/* Let the buffer manager allocate the page */
	return(PFbufAlloc(fd,*pagenum,pagebuf));
}


int PF_DisposePage(fd,pagenum,pagebuf)
int fd;			/* file descriptor */
int pagenum;		/* page number to be disposed */
char *pagebuf;		/* buffer address of the page */
/****************************************************************************
SPECIFICATIONS:
	Dispose a page. The page becomes free.
	The page must be fixed in the buffer.
	The page is unfixed after the call.

AUTHOR: clc

RETURN VALUE:
	PFE_OK if OK
	PF error code if error
*****************************************************************************/
{
int err;

	if (PFinvalidFd(fd)){
		PFerrno = PFE_FD;
		return(PFerrno);
	}

	if (PFinvalidPagenum(fd,pagenum)){
		PFerrno = PFE_INVALIDPAGE;
		return(PFerrno);
	}

	/* put the page into the free list */
	*((int *)pagebuf) = PFftab[fd].hdr.firstfree;
	PFftab[fd].hdr.firstfree = pagenum;
	PFftab[fd].hdrchanged = TRUE;

	/*
	 * Mark the page dirty.
	 * This is necessary so that the page will be written
	 * to disk.
	 */
	if ((err=PF_MarkDirty(fd,pagenum))!= PFE_OK)
		return(err);

	/* unfix the page */
	if ((err=PF_UnfixPage(fd,pagenum,TRUE))!= PFE_OK)
		return(err);

	return(PFE_OK);
}


int PF_GetThisPage(fd,pagenum,pagebuf)
int fd;			/* file descriptor */
int pagenum;		/* page number */
char **pagebuf;		/* buffer address of the page */
/****************************************************************************
SPECIFICATIONS:
	Get a specific page of file "fd".
	The page is fixed in the buffer.

AUTHOR: clc

RETURN VALUE:
	PFE_OK if OK
	PF error code if error.
*****************************************************************************/
{
int err;

	if (PFinvalidFd(fd)){
		PFerrno = PFE_FD;
		return(PFerrno);
	}
	
	if (PFinvalidPagenum(fd,pagenum)){
		PFerrno = PFE_INVALIDPAGE;
		return(PFerrno);
	}

	/* Let the buffer manager get the page */
	if ((err=PFbufGet(fd,pagenum,pagebuf))!= PFE_OK)
		return(err);
	
	return(PFE_OK);
}

/*
 * PF_MarkDirty
 * Mark a page as dirty.
 */
int PF_MarkDirty(int fd, int pagenum)
{
    if (PFinvalidFd(fd)) {
        PFerrno = PFE_FD;
        return(PFerrno);
    }
    
    if (PFinvalidPagenum(fd, pagenum)) {
        PFerrno = PFE_INVALIDPAGE;
        return(PFerrno);
    }

    /* Use the existing PFbufUsed function, which marks */
    /* the page dirty AND moves it to the head of the list. */
    return PFbufUsed(fd, pagenum);
}


int PF_UnfixPage(fd,pagenum,dirty)
int fd;			/* file descriptor */
int pagenum;		/* page number */
int dirty;		/* TRUE if page has been modified */
/****************************************************************************
SPECIFICATIONS:
	Unfix a page.
	"dirty" is TRUE if the page has been modified.

AUTHOR: clc

RETURN VALUE:
	PFE_OK	if no error
	PF error code if error.

*****************************************************************************/
{

	if (PFinvalidFd(fd)){
		PFerrno = PFE_FD;
		return(PFerrno);
	}

	if (PFinvalidPagenum(fd,pagenum)){
		PFerrno = PFE_INVALIDPAGE;
		return(PFerrno);
	}

	/* Let the buffer manager unfix the page */
	return(PFbufUnfix(fd,pagenum,dirty));
}

/* error messages */
static char *PFerrormsg[]={
"No error",
"No memory",
"No buffer space",
"Page already fixed in buffer",
"page to be unfixed is not in the buffer",
"unix error",
"incomplete read of page from file",
"incomplete write of page to file",
"incomplete read of header from file",
"incomplete write of header to file",
"invalid page number",
"file already open",
"file table full",
"invalid file descriptor",
"end of file",
"page already free",
"page already unfixed",
"new page to be allocated already in buffer",
"hash table entry not found",
"page already in hash table"
};

void PF_PrintError(s)
char *s;	/* string to write */
/****************************************************************************
SPECIFICATIONS:
	Write the string "s" onto stderr, then write the last
	error message from PF onto stderr.

AUTHOR: clc

RETURN VALUE: none

*****************************************************************************/
{

	fprintf(stderr,"%s",s);
	fprintf(stderr,":%s",PFerrormsg[-PFerrno]);
	if (PFerrno == PFE_UNIX)
		perror(" ");
	else
		fprintf(stderr,"\n");
}
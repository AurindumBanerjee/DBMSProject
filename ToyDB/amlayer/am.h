/* am.h: MODIFIED FOR MODERN COMPILER */
#include <stdlib.h> /* For malloc/calloc */
#include <string.h> /* For bcopy/memcpy */

typedef struct am_leafheader
	{
		char pageType;
		int nextLeafPage;
		short recIdPtr;
		short keyPtr;
		short freeListPtr;
		short numinfreeList;
		short attrLength;
		short numKeys;
		short maxKeys;
	}  AM_LEAFHEADER; /* Header for a leaf page */

typedef struct am_intheader 
	{
		char pageType;
		short numKeys;
		short maxKeys;
		short attrLength;
	}	AM_INTHEADER ; /* Header for an internal node */

extern int AM_RootPageNum; /* The page number of the root */
extern int AM_LeftPageNum; /* The page Number of the leftmost leaf */
extern int AM_Errno; /* last error in AM layer */

/* * REMOVED conflicting extern char *calloc();
 * REMOVED conflicting extern char *malloc();
 * <stdlib.h> now provides these.
 */

# define AM_Check if (errVal != PFE_OK) {AM_Errno = AME_PF; return(AME_PF) ;}
# define AM_si sizeof(int)
# define AM_ss sizeof(short)
# define AM_sl sizeof(AM_LEAFHEADER)
# define AM_sint sizeof(AM_INTHEADER)
# define AM_sc sizeof(char)
# define AM_sf sizeof(float)
# define AM_NOT_FOUND 0 /* Key is not in tree */
# define AM_FOUND 1 /* Key is in tree */
# define AM_NULL 0 /* Null pointer for lists in a page */
# define AM_MAX_FNAME_LENGTH 80
# define AM_NULL_PAGE -1 
# define FREE 0 /* Free is chosen to be zero because C initialises all 
	   variablesto zero and we require that our scan table be initialised */
# define FIRST 1 
# define BUSY 2
# define LAST 3
# define OVER 4
# define ALL 0
# define EQUAL 1
# define LESS_THAN 2
# define GREATER_THAN 3
# define LESS_THAN_EQUAL 4
# define GREATER_THAN_EQUAL 5
# define NOT_EQUAL 6
# define MAXSCANS 20
# define AM_MAXATTRLENGTH 256


# define AME_OK 0
# define AME_INVALIDATTRLENGTH -1
# define AME_NOTFOUND -2
# define AME_PF -3
# define AME_INTERROR -4
# define AME_INVALID_SCANDESC -5
# define AME_INVALID_OP_TO_SCAN -6
# define AME_EOF -7
# define AME_SCAN_TAB_FULL -8
# define AME_INVALIDATTRTYPE -9
# define AME_FD -10
# define AME_INVALIDVALUE -11


/* --- ADDED FUNCTION PROTOTYPES --- */

/* From am.c */
int AM_SplitLeaf(int, char *, int *, int, int, char *, int, int, char *);
int AM_AddtoParent(int, int, char *, int);
void AM_AddtoIntPage(char *, char *, int, AM_INTHEADER *, int);
void AM_FillRootPage(char *, int, int, char *, short, short);
void AM_SplitIntNode(char *, char *, char *, AM_INTHEADER *, char *, int, int);

/* From amfns.c */
int AM_CreateIndex(char *, int, char, int);
int AM_DestroyIndex(char *, int);
int AM_DeleteEntry(int, char, int, char *, int);
int AM_InsertEntry(int, char, int, char *, int);
void AM_PrintError(char *);

/* From aminsert.c */
int AM_InsertintoLeaf(char *, int, char *, int, int, int);
void AM_InsertToLeafFound(char *, int, int, AM_LEAFHEADER *);
void AM_InsertToLeafNotFound(char *, char *, int, int, AM_LEAFHEADER *);
void AM_Compact(int, int, char *, char *, AM_LEAFHEADER *);

void AM_PrintIntNode(char *, char);
void AM_PrintLeafNode(char *, char);
/* --- MODIFIED --- Changed return type to int */
int AM_DumpLeafPages(int, int, char, int);
void AM_PrintLeafKeys(char *, char);
void AM_PrintAttr(char *, char, int);
void AM_PrintTree(int, int, char);

/* From amscan.c */
int AM_OpenIndexScan(int, char, int, int, char *);
int AM_FindNextEntry(int);
int AM_CloseIndexScan(int);
int GetLeftPageNum(int);

/* From amsearch.c */
int AM_Search(int, char, int, char *, int *, char **, int *);
int AM_BinSearch(char *, char, int, char *, int *, AM_INTHEADER *);
int AM_SearchLeaf(char *, char, int, char *, int *, AM_LEAFHEADER *);
int AM_Compare(char *, char, int, char *);

/* From amstack.c */
void AM_PushStack(int, int);
void AM_PopStack(void);
void AM_topofStack(int *, int *);
void AM_EmptyStack(void);
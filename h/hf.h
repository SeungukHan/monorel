#ifndef __HF_H__
#define __HF_H__

#include <minirel.h>

/****************************************************************************
 * hf.h: external interface definition for the HF layer 
 ****************************************************************************/

#define PAGE_FORMAT_SIZE 524
#define PAGE_FORMAT_OFFSET (PAGE_SIZE - PAGE_FORMAT_SIZE)
#define PAGE_FORMAT_BITMAP_OFFSET (PAGE_FORMAT_OFFSET + sizeof(int) * 3)

#define BITMAP_SIZE 4096	/* Granularity bit */


/* [Data Structure]
 * How header page format should be set?
 * Header page should be allocated for each file. (i.e. Files have each own header page)
 *
 * |-----------------------------------------|
 *    nr_pages  |  rec_size    ...
 *     (4KB)    |   (4KB)     ...
 *   [pf layer] | [hf layer]  ...
 * |-----------------------------------------|
 */

typedef struct _HFHeader {
	int rec_size;           /* Record size */
	int nr_rec;             /* Number of records per page */
	int nr_pages;           /* Number of data pages in file */
	/*int nr_free_pages;*/	/* Number of free pages in the file */ 
	int free_page_list;	/**/
	int full_page_list;	/**/
	
} HFHeader;

typedef struct _hf_table_entry {
	int valid;
	int scan_active;
	char fname[STR_SIZE];
	int pf_fd;
	struct _HFHeader header;
} hf_table_entry;

typedef struct _page_format {
	int next;		/* (4) Next page number in list */
	int prev;		/* (4) Previous page number in list */
	int nr_rec;		/* (4) Number of stored record */
	char *bitmap;		/* (512) */
} page_format;

typedef struct _hf_scan_entry {
	int valid;
	int hf_fd;
	char attrType;
	int attrLength;
	int attrOffset;
	int op;
	char *value;
	RECID rec_id;
} hf_scan_entry;

/*
 * prototypes for HF-layer functions
 */
void 		HF_Init(void);
int 		HF_CreateFile(char *fileName, int RecSize);
int 		HF_DestroyFile(char *fileName);
int 		HF_OpenFile(char *fileName);
int		HF_CloseFile(int fileDesc);
RECID		HF_InsertRec(int fileDesc, char *record);
int	 	HF_DeleteRec(int fileDesc, RECID recId);
RECID 		HF_GetFirstRec(int fileDesc, char *record);
RECID		HF_GetNextRec(int fileDesc, RECID recId, char *record);
int	 	HF_GetThisRec(int fileDesc, RECID recId, char *record);
int 		HF_OpenFileScan(int fileDesc, char attrType, int attrLength, 
				int attrOffset, int op, char *value);
RECID		HF_FindNextRec(int scanDesc, char *record);
int		HF_CloseFileScan(int scanDesc);
void		HF_PrintError(char *errString);
bool_t          HF_ValidRecId(int fileDesc, RECID recid);
/*int             HF_HeaderInfo(int fileDesc, HFHeader *FileInfo);*/

int		HF_HeaderRead(struct _HFHeader *header, int pf_fd);
int		HF_HeaderWrite(struct _HFHeader *header, int pf_fd);
int		HF_ReadPageFormat(struct _page_format *format, char *pagebuf);
int 		HF_WritePageFormat(struct _page_format *format, char *pagebuf);
int 		HF_InitList(char *pagebuf, int my_pagenum);

int 		_HF_DeletePageList(struct _hf_table_entry *table_entry, 
		      	 	  struct _page_format *format,
		      		  int pagenum, int is_free_page_list);

int 		_HF_InsertPageList(struct _hf_table_entry *table_entry, 
		      		  struct _page_format *format,
		      		  int pagenum, int is_free_page_list);


/******************************************************************************/
/*	Error codes definition			  			      */
/******************************************************************************/
#define	HF_NERRORS		19	/* number of error codes used */

#define HFE_OK                   0  /* HF routine successful */
#define HFE_PF                  -1  /* error in PF layer */
#define HFE_FTABFULL            -2  /* # files open exceeds MAXOPENFILES  */
#define HFE_STABFULL            -3  /* # scans open exceeds MAXSCANS  */
#define HFE_FD                  -4  /* invalid file descriptor  */
#define HFE_SD                  -5  /* invalid scan descriptor */
#define HFE_INVALIDRECORD       -6  /* invalid record id  */
#define HFE_EOF                 -7  /* end of file  */
#define HFE_RECSIZE             -8  /* record size invalid */
#define	HFE_FILEOPEN		-9  /* File is open in file table */
#define	HFE_SCANOPEN		-10 /* Scan Open for the given file */
#define HFE_FILENOTOPEN		-11 /* File not open in file table */
#define HFE_SCANNOTOPEN		-12 /* Scan not open in scan table */
#define HFE_ATTRTYPE		-13 /* Invalid attribute type in file scan*/
#define HFE_ATTRLENGTH		-14 /* Invalid attribute length */
#define HFE_ATTROFFSET   	-15 /* Invalid attribute offset */
#define HFE_OPERATOR		-16 /* Invalid Operator in file scan */
#define HFE_FILE		-17 /* File not a MINIREL file */
#define HFE_INTERNAL            -18 /* Error which should not have */
			            /* occured (possibly corrupt file) */
#define	HFE_PAGE		-19 /* Unable to allocate page for file b'coz */
				    /* page number  > MAXPGNUMBER */

#define HFE_INVALIDSTATS        -20 /* meaningful only when STATS_XXX macros
                                       are in use */
#define HFE_FILE_DESTROY	-21 /* File Destory Error */

/* Newly defined error code */
#define HFE_NULL_POINTER	-22 /* Buffer has null pointer */
#define HFE_GET_THIS_PAGE	-23 /* Failed to get page by pf */
#define HFE_DIRTY_PAGE		-24 /* Failed to make dirty page by pf */
#define HFE_PF_CREATE		-25 /* Failed to create new file by pf */
#define HFE_PF_OPEN		-26 /* Failed to open the file by pf */
#define HFE_REOPEN		-27 /* Try to re-open the opend file */
#define HFE_ALLOC_PAGE		-28 /* Failed to allocate the new page */
#define HFE_HEADER_WRITE	-29 /* Failed to write header info */
#define HFE_PF_CLOSE		-30 /* Failed to close file by pf */
#define HFE_PF_UNPIN		-31 
#define HFE_HEADER_READ		-32 /* Failed to read header info */
#define HFE_UNPIN		-33

/******************************************************************************/
/* The current HF layer error code or HFE_OK if function returned without one */
/******************************************************************************/
extern int	HFerrno;

#endif


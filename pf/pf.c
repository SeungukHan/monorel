#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include "minirel.h"
#include "pf.h"
#include "bf.h"

/* From user view, the page numbering will be start from 0 ... 100
 * But, from pf layer, the page number is treated with one increasing.
 * For example, when user request page 0 to pf layer,
 * pf layer will return the page 1 to user. Page 0 is always reserved for metadata space.
 */

#define INVALID_INDEX -1
#define META_PAGE_OFFSET 0
#define DATA_PAGE_OFFSET 1

struct PFftab_ele *pf_table; 
unsigned int nr_pf_elem;

bool_t PF_SearchTable(char *filename, int *ret_index)
{
	int i = 0;
	for (i = 0; i < PF_FTAB_SIZE; i++) {
		if (pf_table[i].unixfd != 0 && 
		    pf_table[i].valid == TRUE && 
		    !strncmp(filename, pf_table[i].fname, strlen(filename))) {
			*ret_index = i;
			return TRUE;
		}
	}

	return FALSE;
}


bool_t PF_SearchFile(char *filename)
{
	int fd;
	if ((fd = open(filename, O_RDONLY)) > 0) {
		close(fd);
		return TRUE;
	}
	return FALSE;
}

void PF_Init(void)
{
	int i = 0;
	
	pf_table = calloc(PF_FTAB_SIZE, sizeof(struct PFftab_ele));
	for (i = 0; i < PF_FTAB_SIZE; i++) {
		pf_table[i].fname = malloc(STR_SIZE * sizeof(char));
	}
	nr_pf_elem = 0;

	BF_Init();	

	return ;
}

/* Creating new file table elem, and allocate monorel fd then return. */
int PF_CreateFile(char *filename)
{
	int fd;
	int i = 0;
	struct PFftab_ele *elem;
	struct PFpage page;

	if (PF_SearchFile(filename)) {
		/* File already exist */
		printf("File Already exist\n");
		return PFE_FILEOPEN;
	}

	if ((fd = creat(filename, 0666)) < 3) {
		return PFE_FILEOPEN;
	}

	/* Find free elem in file table (Need be optimized) */
	for (i = 0; i < PF_FTAB_SIZE; i++) {
		if (pf_table[i].valid == FALSE) {
			elem = &pf_table[i];
			break;
		}
	}

	if (i == PF_FTAB_SIZE) {
		printf("PF_CreateFile: PF Table is full...\n");
		return PFE_FILEOPEN;
	}

	elem->valid = FALSE;
	elem->inode = i;
	strncpy(elem->fname, filename, STR_SIZE);
	elem->unixfd = fd;
	elem->hdr.numpages = 0;
	elem->hdr.offset = 0;

	if (pwrite(elem->unixfd, &(elem->hdr.numpages), 
				sizeof(int), 0) != sizeof(int)) {
		printf("PF_CreateFile: pwrite failed...\n");
		return PFE_FILEIO;
	}

	close(fd);

	return PFE_OK;
}

int PF_DestroyFile(char *filename)
{
	int ret;
	if ((ret = remove(filename)) < 0) 
		return PFE_FILENOTEXIST;
	
	return PFE_OK;
}

/* Should we allow multiple open? 
 * On this implementation, we allow multiple open (we don't have any multiple open check here.)
 * If the opened file is tried to be opened again, the table entry wiil be initialized.
 */
int  PF_OpenFile(char *filename)
{
	int index = INVALID_INDEX;
	int fd, i;
	struct PFftab_ele *elem;
	struct PFpage page;

	if (!PF_SearchFile(filename))
		return PFE_FILENOTEXIST;	

	if ((fd = open(filename, O_RDWR | O_CREAT)) < 0) {
		return PFE_FILEOPEN;
	}

	/* Allocate pf_table elem, The file had not been opened? */
	if (!PF_SearchTable(filename, &index)) {
		for (i = 0; i < PF_FTAB_SIZE; i++) {
			if (pf_table[i].valid == FALSE) {
				index = i;
				break;
			}
		}
		if (index == INVALID_INDEX) { 
			printf("[PF_OpenFile] PF Table is full\n");
			return PFE_FTABFULL;
		}

		elem = &(pf_table[index]);
		
		if ((pread(fd, &elem->hdr.numpages, 
			   sizeof(int), META_PAGE_OFFSET)) != sizeof(int)) 
			return PFE_FILEIO;
	} else {
		printf("[PF_OpenFile] Can not open the no-closed file\n");
		return PFE_FTABFULL;
	}

	elem->valid = TRUE;
	elem->inode = index;
	strncpy(elem->fname, filename, STR_SIZE);
	elem->unixfd = fd;
	elem->hdrchanged = FALSE;
	elem->hdr.offset = 0;

	return elem->inode;
}

/* If the pages in file are pinned, then return error */
int  PF_CloseFile(int fd)
{
	struct PFftab_ele *elem = &pf_table[fd];
	BFreq bq;
	int i = 0;
	int numpages;
	
	if (elem->valid == FALSE || elem->inode != fd) {
		printf("PF_CloseFile: elem state is wrong\n");
		return PFE_CLOSE;
	}

	bq.fd = elem->inode;
	bq.unixfd = elem->unixfd;
	numpages = elem->hdr.numpages;
	
	if (BF_FlushBuf(elem->inode) != BFE_OK) {
		printf("PF_CloseFile: BF_FlushBuf return error\n");
		return PFE_CLOSE;
	}
	
	if (elem->hdrchanged == TRUE) {
		if (pwrite(elem->unixfd, &(elem->hdr.numpages), 
					sizeof(int), 0) != sizeof(int)) {
			printf("PF_CloseFile: file io error\n");
			return PFE_FILEIO;
		}
	}
	
	elem->valid = FALSE;
	elem->hdrchanged = FALSE;
	close(bq.unixfd);

	return PFE_OK;
}


int  PF_AllocPage(int fd, int *pagenum, char **pagebuf)
{
	struct PFftab_ele *elem;
	BFreq bq;
	PFpage *fpage;

	elem = &pf_table[fd];
	bq.fd = elem->inode;
	bq.unixfd = elem->unixfd;
	bq.pagenum = elem->hdr.numpages + DATA_PAGE_OFFSET;

	if (BF_AllocBuf(bq, &fpage) != BFE_OK) {
		return PFE_ALLOC;
	}

	if (BF_TouchBuf(bq) != BFE_OK) {
		return PFE_ALLOC;
	}

	*pagenum = elem->hdr.numpages;
	*pagebuf = fpage->pagebuf;

	elem->hdr.numpages++;
	elem->hdrchanged = TRUE;

	return PFE_OK;
}

int PF_GetThisPage(int fd, int pagenum, char **pagebuf)
{
	struct PFftab_ele *elem;
	BFreq bq;
	PFpage *fpage;
	int ret = 0;

	elem = &pf_table[fd];
	
	if (elem->valid == FALSE || elem->inode != fd)
		return PFE_GETTHIS;

	if (pagenum >= elem->hdr.numpages) {
	/*	printf("PF_GetThisPage: numpages : %d\n", elem->hdr.numpages); */
	/*	printf("PF_GetThisPage: pagenum  : %d\n", pagenum);		*/
		
		return PFE_INVALIDPAGE;
	}

	bq.fd = elem->inode;
	bq.unixfd = elem->unixfd;
	bq.pagenum = pagenum + DATA_PAGE_OFFSET;

	if ((ret = BF_GetBuf(bq, &fpage)) != BFE_OK) {
		printf("GetBuf Error : %d\n", ret);
		return PFE_GETTHIS;
	}

	*pagebuf = fpage->pagebuf;
	return PFE_OK;
}

int PF_GetHeaderPage(int fd, char **pagebuf)
{
	struct PFftab_ele *elem;
	BFreq bq;
	PFpage *fpage;
	int ret = 0;

	elem = &pf_table[fd];
	
	if (elem->valid == FALSE || elem->inode != fd)
		return PFE_GETTHIS;

	bq.fd = elem->inode;
	bq.unixfd = elem->unixfd;
	bq.pagenum = META_PAGE_OFFSET;

	if ((ret = BF_GetBuf(bq, &fpage)) != BFE_OK) {
		printf("GetBuf Error : %d\n", ret);
		return PFE_GETTHIS;
	}

	*pagebuf = fpage->pagebuf;
	return PFE_OK;
}

int PF_GetFirstPage(int fd, int *pagenum, char **pagebuf)
{
	struct PFftab_ele *elem;
	BFreq bq;
	PFpage *fpage;

	elem = &pf_table[fd];

	if (elem->valid == FALSE || elem->inode != fd) 
		return PFE_GETNEXT;

	if (elem->hdr.numpages == 0) 
		return PFE_EOF;
	
	elem->hdr.offset = 0;

	if (PF_GetThisPage(fd, elem->hdr.offset, pagebuf) != PFE_OK) 
		return PFE_GETNEXT;

	*pagenum = 0;
	elem->hdr.offset++;
	return PFE_OK;
}

int PF_GetNextPage(int fd, int *pagenum, char **pagebuf)
{
	struct PFftab_ele *elem;
	BFreq bq;
	PFpage *fpage;

	elem = &pf_table[fd];
	
	if (elem->valid == FALSE || elem->inode != fd)
		return PFE_GETNEXT;

	if (*pagenum == -1) {
		return PF_GetFirstPage(fd, pagenum, pagebuf);
	}

	if (elem->hdr.offset >= elem->hdr.numpages) {
		return PFE_EOF;
	}

	if (PF_GetThisPage(fd, elem->hdr.offset, pagebuf) != PFE_OK) {
		return PFE_GETNEXT;
	}

	*pagenum = elem->hdr.offset;
	elem->hdr.offset++;
	return PFE_OK;
}

int PF_DirtyPage(int fd, int pagenum)
{
	struct PFftab_ele *elem;
	BFreq bq;
	PFpage *fpage;
	
	elem = &pf_table[fd];
	
	if (elem->valid == FALSE || elem->inode != fd) {
		return PFE_GETDIRTY;
	}

	if (elem->hdr.numpages <= pagenum) {
		return PFE_GETDIRTY;
	}

	bq.fd = fd;
	bq.unixfd = elem->unixfd;
	bq.pagenum = pagenum + DATA_PAGE_OFFSET;
	
	if (BF_TouchBuf(bq) != BFE_OK) {
		return PFE_GETDIRTY;
	}

	return PFE_OK;
}

int PF_UnpinPage(int fd, int pagenum, int dirty)
{
	struct PFftab_ele *elem;
	BFreq bq;
	PFpage *fpage;

	elem = &pf_table[fd];

	if (elem->valid == FALSE || elem->inode != fd) {
		return PFE_UNPINPAGE;
	}

	if (elem->hdr.numpages <= pagenum) {
		return PFE_UNPINPAGE;
	}

	if (dirty == TRUE) {
		if (PF_DirtyPage(fd, pagenum) != PFE_OK) {
			return PFE_UNPINPAGE;
		}
	}

	bq.fd = fd;
	bq.unixfd = elem->unixfd;
	bq.pagenum = pagenum + DATA_PAGE_OFFSET;
	BF_UnpinBuf(bq);

	return PFE_OK;
}

void PF_PrintError(const char *mesg)
{
	printf("%s\n", mesg);
	return ;
}

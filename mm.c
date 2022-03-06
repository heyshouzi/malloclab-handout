/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))


#define WSIZE   4 //word and header/footer size (bytes)
#define DSIZE   8 // double word size(bytes)
#define CHUNKSIZE   (1<<12) //Extend heap by this amount (bytes)
#define MINBLSIZE 16  //min block size, only include footer and header(bytes)  


#define MAX(x,y) ((x)>(y)?(x):(y))

#define PACK(size,alloc) ((size) | (alloc))

//read and write a word at address p
#define GET(p)  (*(unsigned int *)(p))
#define PUT(p,val)  (*(unsigned int *)(p)=(val))

//read the size and allocated fields from address p
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

//given block ptr bp compute address of its header and footer
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE((char *)((bp)-WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)((bp)-DSIZE)))



static void *extend_heap(size_t words);
static void *coalesce(char *bp);
static void *remove_freeblock(char *pr);
static char *find_fit(size_t asize);
static void *place(char *bp,size_t asize);


static char* heap_listp;

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    //初始化申请一个最小块（32 bytes）包括：1 * 对齐块 + 2 * 序言块 + 1 * 结尾块 
    if ((heap_listp = mem_sbrk(4 *WSIZE)) == (void *)-1 )
        return -1;
    //对齐块    
    PUT(heap_listp, 0);
    //序言块
    PUT(heap_listp + (WSIZE),PACK(DSIZE,1));
    PUT(heap_listp + (2*WSIZE),PACK(DSIZE,1));
    //结尾块
    PUT(heap_listp + (3*WSIZE),PACK(0,1));
    //将heap_listp 指向第二个序言块
    heap_listp += (2*WSIZE);

    //向内存系统申请额外的堆空间（1k words）
    if(extend_heap(CHUNKSIZE/WSIZE)==NULL)
        return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */

void *mm_malloc(size_t size)
{
    size_t asize;
    char *bp;
    size_t extendsize;
    if(size <= 0){
        return 0;
    }
    if(size <= DSIZE) {
        asize = 2*DSIZE;
    }
    else{
        asize = ALIGN(size) + DSIZE;
        }
    if((bp = find_fit(asize)) != NULL) {
        place(bp,asize);
        return bp;
        }
    extendsize = MAX(asize,CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL){
        return NULL;
        }
    place(bp, asize);
    return bp;
    
    // int newsize = ALIGN(size + SIZE_T_SIZE);
    // void *p = mem_sbrk(newsize);
    // if (p == (void *)-1)
	// return NULL;
    // else {
    //     *(size_t *)p = size;
    //     return (void *)((char *)p + SIZE_T_SIZE);
    // }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp),PACK(size,0));
    PUT(FTRP(bp),PACK(size,0));

    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}



static void *extend_heap(size_t words) {
    char *bp;
    size_t asize;
    asize = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if(asize < MINBLSIZE)
        asize = MINBLSIZE;
    if((long)(bp = mem_sbrk(asize)) == -1) {
        return NULL;
    }
    PUT(HDRP(bp),PACK(asize,0));
    PUT(FTRP(bp),PACK(asize,0));
    PUT(HDRP(NEXT_BLKP(bp)),PACK(0,1));
    return coalesce(bp);

}

static void *coalesce(char *bp)
{
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {
        return bp;
    }

    else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp),PACK(size,0));
        // remove_freeblock(FTRP(bp));
        // remove_freeblock(HDRP(NEXT_BLKP(bp)));
        PUT(FTRP(NEXT_BLKP(bp)),PACK(size,0));
    }
    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp),PACK(size,0));
        // remove_freeblock(HDRP(bp));
        // remove_freeblock(FTRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));
        bp = HDRP(PREV_BLKP(bp));
    }
    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)),PACK(size,0));
        // remove_freeblock(HDRP(bp));
        // remove_freeblock(FTRP(bp));
        // remove_freeblock(FTRP(PREV_BLKP(bp)));
        // remove_freeblock(HDRP(NEXT_BLKP(bp)));
    }

    return bp;

}
/* 
static void* remove_freeblock(char *pr)
{
    PUT((int *)pr,PACK(0,0));
    return (char *)pr;
} */

static char *find_fit(size_t asize)
{
    char *bp;
    for(bp = heap_listp;GET_SIZE(HDRP(bp)) > 0;bp = NEXT_BLKP(bp)) {
        if ((GET_SIZE(HDRP(bp)) >= asize) && !GET_ALLOC(HDRP(bp))) {
            return bp;
        }
    }
    return NULL;
}

static void *place(char *bp,size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    if (csize - asize <= 2*DSIZE) {
        PUT(HDRP(bp),PACK(csize,1));
        PUT(FTRP(bp),PACK(csize,1));
    }
    else {
        PUT(HDRP(bp),PACK(asize,1));
        PUT(FTRP(bp),PACK(asize,1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp),PACK(csize-asize,0));
        PUT(FTRP(bp),PACK(csize-asize,0));
    }
    return bp;
}




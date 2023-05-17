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
//add function
/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "team eight",
    /* First member's full name */
    "bigPerson",
    /* First member's email address */
    "daein7076@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};


// basic constants and macros
#define WSIZE               4  // 워드 사이즈 4
#define DSIZE               8  // 더블워드 사이즈 8, 더블 워드 정렬
#define CHUNKSIZE           (1<<13)  // bit 연산 4096 (bytes), extend 기본 크기
#define MAX(x, y)           ((x) > (y) ? (x) : (y))
// 사이즈와 비트 할당(allocate or free)를 통합하여 헤더와 풋터에 저장할 수 있는 값을 리턴
#define PACK(size, alloc)   ((size) | (alloc)) 
//  p가 현재 참조 하고 있는 값에 대해 읽기 및 쓰기
#define GET(p)              (*(unsigned int *)(p))
#define PUT(p, val)         (*(unsigned int *)(p) = (val))
// read: 주소 p의 할당된 헤더 혹은 푸터에서 해당 사이즈 또는 할당 여부를 리턴
#define GET_SIZE(p)         (GET(p) & ~0x7)
#define GET_ALLOC(p)        (GET(p) & 0x1)
// 블록 포인터 bp, bp에 대한 헤더와 풋터의 주소 계산
#define HEADER_P(bp)        ((char *)(bp) - WSIZE)
#define FOOTER_P(bp)        ((char *)(bp) + GET_SIZE(HEADER_P(bp)) - DSIZE)
// 블록 포인터 bp, bp에 대한 다음 및 이전 블록의 포인터 구하기
// block pointer + whole block size (with self header pointer)
#define NEXT_BLOCK_P(bp)    ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))  
// block pointer - prev whole block size (with prev footer pointer)
#define PREV_BLOCK_P(bp)    ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))  

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static char *heap_list_p;
static char *next_fit_p;  // next_fit 에서 가장 최근에 사용했던 bp 저장용 global variable

/* 
 * mm_init - initialize the malloc package.
    함수 초기화, 성공 시 0, 실패시 -1
    묵시적 가용리스트 사용
    최소 블럭 크기는 16byte
 */
int mm_init(void)
{
    // create empty heap list
    // heap_list_p에 16의 사이즈 할당 이하 (heap이라 명칭)
    // heap에 16만큼의 크기를 주고 시작 pointer를 받아옴
    if ((heap_list_p = mem_sbrk(4 * WSIZE)) == (void *)-1) {
        return -1;
    }
    // heap의 첫번째 word는 미사용 패딩 워드(epilogue block을 위한 것으로 이해)
    PUT(heap_list_p, 0);
    //  header 및 footer는 초기화 시 heap의 앞, 뒤가 아닌 앞쪽에 연속적으로 붙어있음 (prologue block)
    PUT(heap_list_p + (1 * WSIZE), PACK(DSIZE, 1));  // header (8bytes 할당)
    PUT(heap_list_p + (2 * WSIZE), PACK(DSIZE, 1));  // footer (8bytes 할당)
    PUT(heap_list_p + (3 * WSIZE), PACK(0, 1));  // epilogue block (할당된 자료의 끝을 알려줌)
    heap_list_p += (2 * WSIZE);  //static global variable, 항상 프롤로그 블록을 가리킴(header 바로 뒤)

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
        return -1;
    }
    next_fit_p = heap_list_p;
    return 0;
}
/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0) {
        return NULL;
    }
    // 최소 사이즈를 16바이트, 그 이상은 8의 배수로 맞춰줌
    if (size <= DSIZE) {
        asize = 2 * DSIZE;
    } else {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }
    // free 가 있는지 찾음
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) {
        return NULL;
    }
    place(bp, asize);
    return bp;

}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HEADER_P(ptr));
    PUT(HEADER_P(ptr), PACK(size, 0));
    PUT(FOOTER_P(ptr), PACK(size, 0));
    coalesce(ptr);
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
    copySize = GET_SIZE(HEADER_P(oldptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

// add function

static void *extend_heap(size_t words) {
    char *bp;
    size_t size;
    // 더블 워드의 배수로 반올림
    size = (words % 2) ? (words + 1) * WSIZE : (words) * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) {
        return NULL;
    }

    PUT(HEADER_P(bp), PACK(size, 0));
    PUT(FOOTER_P(bp), PACK(size, 0));
    PUT(HEADER_P(NEXT_BLOCK_P(bp)), PACK(0, 1));

    return coalesce(bp);

}


static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(HEADER_P(PREV_BLOCK_P(bp)));
    size_t next_alloc = GET_ALLOC(HEADER_P(NEXT_BLOCK_P(bp)));
    size_t size = GET_SIZE(HEADER_P(bp));

    if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HEADER_P(NEXT_BLOCK_P(bp)));
        PUT(HEADER_P(bp), PACK(size, 0));
        PUT(FOOTER_P(bp), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HEADER_P(PREV_BLOCK_P(bp)));
        PUT(FOOTER_P(bp), PACK(size, 0));
        PUT(HEADER_P(PREV_BLOCK_P(bp)), PACK(size, 0));
        bp = PREV_BLOCK_P(bp);
    }

    else if (!prev_alloc && !next_alloc) {
        size += GET_SIZE(HEADER_P(PREV_BLOCK_P(bp))) + GET_SIZE(FOOTER_P(NEXT_BLOCK_P(bp)));
        PUT(HEADER_P(PREV_BLOCK_P(bp)), PACK(size, 0));
        PUT(FOOTER_P(NEXT_BLOCK_P(bp)), PACK(size, 0));
        bp = PREV_BLOCK_P(bp);
    }
    next_fit_p = bp;
    return bp;
}

// 가용 리스트에서 검색
static void *find_fit(size_t asize) {
    void *bp;
    // for (bp = heap_list_p; GET_SIZE(HEADER_P(bp)) > 0; bp = NEXT_BLOCK_P(bp)) {
    //     if (!GET_ALLOC(HEADER_P(bp)) && (asize <= GET_SIZE(HEADER_P(bp)))) {
    //         return bp;
    //     }
    // }
    // return NULL;
    
    // next_fit_p 가 epilogue 만날 때 까지 검색
    for (bp = next_fit_p; GET_SIZE(HEADER_P(bp)) > 0; bp = NEXT_BLOCK_P(bp)) {
        if (!GET_ALLOC(HEADER_P(bp)) && (asize <= GET_SIZE(HEADER_P(bp)))) {
            // next_fit_p = bp;
            return bp;
        }
    }
    // 만약 epilogue까지 없으면 처음부터 next_fit_p까지 검색
    for (bp = heap_list_p; (char *)bp <= next_fit_p; bp = NEXT_BLOCK_P(bp)) {
        if (!GET_ALLOC(HEADER_P(bp)) && (asize <= GET_SIZE(HEADER_P(bp)))) {
            // next_fit_p = bp;
            return bp;
        }
    }
    return NULL;
}
// 맞는 블록을 찾으면 할당기는 요청한 블록을 배치하고 옵션으로 초과부분을 분할, 새롭게 할당한 블록 리턴
static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HEADER_P(bp));

    if ((csize - asize) >= (2 * DSIZE)) {
        PUT(HEADER_P(bp), PACK(asize, 1));
        PUT(FOOTER_P(bp), PACK(asize, 1));
        bp = NEXT_BLOCK_P(bp);
        PUT(HEADER_P(bp), PACK(csize - asize, 0));
        PUT(FOOTER_P(bp), PACK(csize - asize, 0));
    } else {
        PUT(HEADER_P(bp), PACK(csize, 1));
        PUT(FOOTER_P(bp), PACK(csize, 1));
    }
}
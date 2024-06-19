#include "cross-platform.h"
#include "lfq.h"
#ifdef DEBUG
#include <assert.h>
#endif
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

#define MAXFREE 150

static
int inHP(struct lfq_ctx *ctx, struct lfq_node * lfn) {
    for ( int i = 0 ; i < ctx->MAXHPSIZE ; i++ ) {
        if (ctx->HP[i] == lfn)
            return 1;
    }
    return 0;
}

static
void enpool(struct lfq_ctx *ctx, struct lfq_node * lfn) {
    lfn->free_next = NULL;
    volatile struct lfq_node *old_tail = atomic_exchange(&ctx->fpt, lfn);  // seq_cst
    old_tail->free_next = lfn;
}

static
void free_pool(struct lfq_ctx *ctx, bool freeall ) {
    if (!atomic_compare_exchange_strong(&ctx->is_freeing, &(int){0}, 1))
        return; // this pool free is not support multithreading.
    volatile struct lfq_node * p;

    for ( int i = 0 ; i < MAXFREE || freeall ; i++ ) {
        p = ctx->fph;
        if ( (!p->can_free) || (!p->free_next) || inHP(ctx, (struct lfq_node *)p) )
            goto exit;
        ctx->fph = p->free_next;
        free((void *)p);
    }
exit:
    atomic_store(&ctx->is_freeing, 0);
    atomic_thread_fence(memory_order_seq_cst);
}

static
void safe_free(struct lfq_ctx *ctx, struct lfq_node * lfn) {
    if (lfn->can_free && !inHP(ctx,lfn)) {
        if (atomic_compare_exchange_strong(&ctx->is_freeing, &(int){0}, 1)) {
            lfn->next = (void*)-1;    // poison the pointer to detect use-after-free
            free(lfn);    // we got the lock; actually free
            atomic_store(&ctx->is_freeing, 0);
            atomic_thread_fence(memory_order_seq_cst);
        } else               
            enpool(ctx, lfn);
    } else
        enpool(ctx, lfn);
    free_pool(ctx, false);
}

static
int alloc_tid(struct lfq_ctx *ctx) {
    for (int i = 0; i < ctx->MAXHPSIZE; i++) 
        if (ctx->tid_map[i] == 0) 
            if (atomic_compare_exchange_strong(&ctx->tid_map[i], &(int){0}, 1))
                return i;

    return -1;
}

static
void free_tid(struct lfq_ctx *ctx, int tid) {
    ctx->tid_map[tid]=0;
}

int lfq_init(struct lfq_ctx *ctx, int max_consume_thread) {
    struct lfq_node * tmpnode = calloc(1,sizeof(struct lfq_node));
    if (!tmpnode) 
        return -errno;
        
    struct lfq_node * free_pool_node = calloc(1,sizeof(struct lfq_node));
    if (!free_pool_node) 
        return -errno;
        
    tmpnode->can_free = free_pool_node->can_free = true;
    memset(ctx, 0, sizeof(struct lfq_ctx));
    ctx->MAXHPSIZE = max_consume_thread;
    ctx->HP = calloc(max_consume_thread,sizeof(struct lfq_node));
    ctx->tid_map = calloc(max_consume_thread,sizeof(struct lfq_node));
    ctx->head = ctx->tail=tmpnode;
    ctx->fph = ctx->fpt=free_pool_node;
    
    return 0;
}

long lfg_count_freelist(const struct lfq_ctx *ctx) {
    long count=0;
    struct lfq_node *p = (struct lfq_node *)ctx->fph; // non-volatile
    while(p) {
        count++;
        p = p->free_next;
    }
    
    return count;
}

int lfq_clean(struct lfq_ctx *ctx){
    if ( ctx->tail && ctx->head ) { // if have data in queue
        struct lfq_node *tmp;
        while ( (struct lfq_node *) ctx->head ) { // while still have node
            tmp = (struct lfq_node *) ctx->head->next;
            safe_free(ctx, (struct lfq_node *)ctx->head);
            ctx->head = tmp;
        }
        ctx->tail = 0;
    }
    if ( ctx->fph && ctx->fpt ) {
        free_pool(ctx, true);
        if ( ctx->fph != ctx->fpt )
            return -1;
        free((void *)ctx->fpt); // free the empty node
        ctx->fph=ctx->fpt=0;
    }
    if ( !ctx->fph && !ctx->fpt ) {
        free((void *)ctx->HP);
        free((void *)ctx->tid_map);
        memset(ctx,0,sizeof(struct lfq_ctx));
    } else
        return -1;
        
    return 0;
}

int lfq_enqueue(struct lfq_ctx *ctx, void * data) {
    struct lfq_node * insert_node = calloc(1,sizeof(struct lfq_node));
    if (!insert_node)
        return -errno;
    insert_node->data=data;

    volatile struct lfq_node *old_tail;
    old_tail = atomic_exchange(&ctx->tail, insert_node);

    old_tail->next = insert_node;
    return 0;
}

void * lfq_dequeue_tid(struct lfq_ctx *ctx, int tid ) {
    volatile struct lfq_node *old_head, *new_head;
    do {
    retry: 
        old_head = ctx->head;
        ctx->HP[tid] = old_head;  // seq-cst store.  (better: use xchg instead of mov + mfence on x86)
        atomic_thread_fence(memory_order_seq_cst);

        if (old_head != ctx->head)  // another thread freed it before seeing our HP[tid] store
            goto retry;
        new_head = old_head->next;   
        if (new_head==0 ){  
            ctx->HP[tid] = 0;
            return 0;  
        }
    } while( ! atomic_compare_exchange_strong(&ctx->head, &old_head, new_head) );

    ctx->HP[tid] = 0;
    void *ret = new_head->data;
    new_head->can_free = true;

    safe_free(ctx, (struct lfq_node *)old_head);
    return ret;
}

void * lfq_dequeue(struct lfq_ctx *ctx ) {
    int tid = alloc_tid(ctx);
    if (tid==-1)
        return (void *)-1; 

    void * ret = lfq_dequeue_tid(ctx, tid);
    free_tid(ctx, tid);
    return ret;
}

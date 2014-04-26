#include "csapp.h"
#include "cache.h"

static cache_block *new_cache(char *url, char *content, unsigned int block_size);
static void insert_cache(cache_list *cl, cache_block *cb);
static void replace_cache(cache_list *cl, cache_block *old_cb, cache_block *new_cb);
static cache_block *delete_cache(cache_block *cb);
static void update_cache(cache_list *cl, cache_block *cb);

void init_cache_list(cache_list *cl) {
	cl->total_size = 0;
	cl->head = new_cache(NULL, NULL, 0);
	cl->tail = new_cache(NULL, NULL, 0);
	cl->head->next = cl->tail;
	cl->tail->prev = cl->head;

	return;
}

void free_cache_list(cache_list *cl) {
	cache_block *cb;
	for(cb = cl->head->next; cb != cl->tail;) {
		cb = delete_cache(cb);
	}

	Free(cl->head);
	Free(cl->tail);
	return;
}

static cache_block *new_cache(char *url, char *content, 
		unsigned int block_size) {
	cache_block *cb;
	cb = (cache_block *)Malloc(sizeof(cache_block));

	cb->block_size = block_size;
	cb->id = url;
	cb->content = content;
	cb->prev = NULL;
	cb->next = NULL;

	return cb;
}

static void insert_cache(cache_list *cl, cache_block *cb) {
	cb->prev = cl->head;
    cb->next = cl->head->next;
    cl->head->next->prev = cb;
    cl->head->next = cb;
    cl->total_size += cb->block_size;
    return;
}

static cache_block *delete_cache(cache_block *cb) {
	cache_block *next_cb;
	cb->next->prev = cb->prev;
	cb->prev->next = cb->next;
	next_cb = cb->next;

    Free(cb);
    return next_cb;
}

static void update_cache(cache_list *cl, cache_block *cb) {
	cb->next->prev = cb->prev;
	cb->prev->next = cb->next;
	cb->prev = cl->head;
	cb->next = cl->head->next;
	cl->head->next = cb;
	cb->next->prev = cb;

	return;
}

static void replace_cache(cache_list *cl, cache_block *old_cb, cache_block *new_cb) {
	delete_cache(old_cb);
	insert_cache(cl, new_cb);

	return;
}

cache_block *find_cache(cache_list *cl, char *url) {
	cache_block *cb;
    
    for(cb = cl->head->next; cb != cl->tail; cb = cb->next) {
    	if(!strcmp(cb->id, url)) {
    		update_cache(cl, cb);
    		return cb; 
    	} 			
    }

    return NULL;
}

void modify_cache(cache_list *cl, char *url, char *content, 
		unsigned int block_size) {
	cache_block *new_cb;
	new_cb = new_cache(url, content, block_size);

    if(cl->total_size + block_size <= MAX_CACHE_SIZE) {
    	insert_cache(cl, new_cb);
    }
    else {
    	cache_block *itr_cb;
    	for(itr_cb = cl->tail->prev; itr_cb != cl->tail; itr_cb = itr_cb->prev) {
    		if(cl->total_size - itr_cb->block_size + block_size 
    				<= MAX_CACHE_SIZE) {
    			replace_cache(cl, itr_cb, new_cb);
    			break;
    		}
    	}
    }

    return;

}

void print_list(cache_list *cl) {
	cache_block *cb;
    for(cb = cl->head->next; cb != NULL; cb = cb->next) {
    	;
    }

    return;
}
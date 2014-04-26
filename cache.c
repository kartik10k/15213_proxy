#include "csapp.h"
#include "cache.h"

void init_cache_list(cache_list *cl) {
	cl->total_size = 0;
	cl->head = new_cache(NULL, NULL, 0);
	cl->tail = new_cache(NULL, NULL, 0);
	cl->head->next = cl->tail;
	cl->tail->prev = cl->head;

	return;
}

void free_cache_list(cache_list *cl) {

}

cache_block *new_cache(char *url, char *content, unsigned int block_size) {
	cache_block *cb;
	cb = (cache_block *)Malloc(sizeof(cache_block));

	cb->block_size = block_size;
	cb->id = url;
	cb->content = content;
	cb->prev = NULL;
	cb->next = NULL;

	return cb;
}

void insert_cache(cache_list *cl, cache_block *cb) {
	cb->prev = cl->head;
    cb->next = cl->head->next;
    cl->head->next->prev = cb;
    cl->head->next = cb;
    cl->total_size += cb->block_size;
    return;
}

void delete_cache(cache_list *cl, cache_block *cb) {
	if (cl->tail->prev == cl->head)
        return;

    cache_block *cb;
    cb = cl->tail->prev;
    cl->tail->prev = cb->prev;
    cl->tail->prev->next = cl->tail;
    cl->total_size -= cb->block_size;

    Free(cb);
    return;
}

void update_cache(cache_list *cl, cache_block *cb) {
	
}

void replace_cache(cache_list *cl, cache_block *old_cb, cache_block *new_cb) {
	delete_cache(cl);
	insert_cache(cl, cb);
}

cache_block *find_cache(cache_list *cl, char *url, char *content, unsigned int block_size) {
	cache_block *cb;
    
    for(cb = cl->head->next; cb != NULL; cb = cb->next) {
    	if(!strcmp(cb->id, url)) {
    		return cb; 
    	}
  			
    }



    if(cl->total_size + block_size <= MAX_CACHE_SIZE) {
    	cb = new_cache(url, content, block_size);
    	insert_cache(cl, cb);
    }
    else {
    	cb = new_cache(url, content, block_size);
    	replace_cache(cl, cb);
    }

    return cb;
}

void print_list(cache_list *cl) {
	cache_block *cb;
    for(cb = cl->head->next; cb != NULL; cb = cb->next) {

    }
}
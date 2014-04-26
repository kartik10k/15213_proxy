#ifndef CACHE_H
#define CACHE_H

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

typedef struct cacheblock{
	char *id;
    unsigned int block_size;
    char *content;
    struct cacheblock *next;
    struct cacheblock *prev;
}cache_block;

typedef struct {
	unsigned int total_size;
	cache_block *head;
	cache_block *tail;
}cache_list;

void init_cache_list(cache_list *cl);
cache_block* new_cache(char *url, char *content, unsigned int block_size);
void insert_cache(cache_list *cl, cache_block *cb);
void delete_cache(cache_list *cl);
void replace_cache(cache_list *cl, cache_block *cb);
cache_block *find_cache(cache_list *cl, char *url, char *content, unsigned int block_size);
void print_list(cache_list *cl);

#endif
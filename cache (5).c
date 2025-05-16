#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cache.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;


 int cache_create(int num_entries) {
    
    //dont create if prev cache hasnt been destroyed
    if (cache != NULL) {
        return -1;
    }
    // validate size
    if (num_entries < 2 || num_entries > 4096) {
        return -1;
    }
    // allocate mem
    cache = calloc(num_entries, sizeof *cache);
    if (cache == NULL) {
        return -1;
    }
    cache_size = num_entries;
    // set defaults
    for (int i = 0; i < cache_size; i++) {
        cache[i].valid       = false;
        cache[i].disk_num    = -1;
        cache[i].block_num   = -1;
        cache[i].access_time = 0;
    
    }
    num_queries = 0;
    num_hits    = 0;
    return 1;
}

//frees cache mem sets to null and size
int cache_destroy(void) {
    if (cache == NULL) {
        return -1;  // nothing to destroy
    }
    free(cache);
    cache = NULL;
    cache_size = 0;
    return 0;
}



int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
    //prevents erros
    if (buf == NULL || cache == NULL) {
        return -1;
    }
    // check block number first
    if (block_num < 0 || block_num >= JBOD_NUM_BLOCKS_PER_DISK) {
        return -1;
    }

    // then check disk number
    if (disk_num < 0 || disk_num >= JBOD_NUM_DISKS) {
        return -1;
    }
    //increment queries count
    num_queries++;
    for (int i = 0; i < cache_size; i++) {
        if (cache[i].valid && cache[i].block_num == block_num && cache[i].disk_num == disk_num) {
            memcpy(buf, cache[i].block, JBOD_BLOCK_SIZE);
            //increment hit count
            num_hits++;
            clock++;
            cache[i].access_time = clock;
            // Return 1 on a successful lookup.
            return 1;
        }
    }
    return -1;
}
static int find_lru_index(void) {
    int lru = 0;
    int mintime = cache[0].access_time;
    for (int i = 1; i < cache_size; i++) {
        if (cache[i].access_time < mintime) {
            mintime = cache[i].access_time;
            lru = i;
        }
    }
    return lru;
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
    // prevent erros
    if (buf == NULL || cache == NULL) {
        return -1;
    }
    // check block number first
    if (block_num < 0 || block_num >= JBOD_NUM_BLOCKS_PER_DISK) {
        return -1;
    }

    // then check disk number
    if (disk_num < 0 || disk_num >= JBOD_NUM_DISKS) {
        return -1;
    }
    // if an entry for this disk and block already exists  dont insert and insert if it doesnt
    for (int i = 0; i < cache_size; i++) {
        if (cache[i].valid && cache[i].block_num == block_num && cache[i].disk_num == disk_num) {
            return -1;
        }
    }
    
    //track time
    clock++;
    // find unused spot in cache
    for (int i = 0; i < cache_size; i++) {
        if (!cache[i].valid) {
            cache[i].valid = true;
            cache[i].block_num = block_num;
            cache[i].disk_num = disk_num;
            memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE);
            cache[i].access_time = clock;
            return 1;
        }
    }
    
    int lru = find_lru_index();
    cache[lru].block_num = block_num;
    cache[lru].disk_num = disk_num;
    memcpy(cache[lru].block, buf, JBOD_BLOCK_SIZE);
    cache[lru].access_time = clock;
    cache[lru].valid = true;
    return 1;
}


void cache_update(int disk_num, int block_num, const uint8_t *buf) {
    // prevent erros
    if (cache == NULL || buf == NULL) {
        return;
    }
    // check block number first
    if (block_num < 0 || block_num >= JBOD_NUM_BLOCKS_PER_DISK) {
        return;
    }

    // then check disk number
    if (disk_num < 0 || disk_num >= JBOD_NUM_DISKS) {
        return;
    }
    for (int i = 0; i < cache_size; i++) {
        if (cache[i].valid && cache[i].block_num == block_num && cache[i].disk_num == disk_num) {
            memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE);
            clock++;
            cache[i].access_time = clock;
            return;
        }
    }
}


//retrun true if cache is enabled/bigger then 1
bool cache_enabled(void) {
    return (cache != NULL && cache_size >= 2);
}



void cache_print_hit_rate(void) {
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}


#include "cache.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "net.h"
#include "mdadm.h"
#include "util.h"
#include "jbod.h"


int requested_cache_size = 0;
int mounted = 0;

//mounts linear device by using JBOD_MOUNT 
//returns -1 on failure and 1 on success
int mdadm_mount(void) {
    uint32_t mount = (0 << 24) | (JBOD_MOUNT) | (0 << 20);
    if (jbod_client_operation(mount, NULL) == 0) {
        mounted = 1;
        // create new cache if cache requested
        if (requested_cache_size > 0) {
            if (cache_create(requested_cache_size) != 0) {
                fprintf(stderr, "Failed to create cache.\n");
                return -1;
            }
        }
        return 1;
    }
    return -1;
}

//unmounts using JBOD_UNMOUNT 
//returns 1 on succes and -1 on failure
int mdadm_unmount(void) {
    // destroy cache on unount
    cache_destroy();
    uint32_t unmount = (0 << 20) | (0 << 24) | (JBOD_UNMOUNT);
    if (jbod_client_operation(unmount, NULL) == 0) {
        mounted = 0;
        return 1;
    }
    return -1;
}

int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
    // check mount status.
    if (mounted == 0) {
        return -1;
    }
    // check that len is less than 1024
    if (len > 1024) {
        return -1;
    }
    // check that addrress + lenght not too long
    if (len + addr > JBOD_NUM_DISKS * JBOD_BLOCK_SIZE * JBOD_NUM_BLOCKS_PER_DISK) {
        return -1;
    }
    //0 length addr and null pointer returns 0
    if (len != 0 && buf == NULL) {
        return -1;
    }
   
    if (len == 0 && buf == NULL) {
        return 0;
    }

    uint32_t current = addr;
    uint32_t bytesRead = 0;

    while (bytesRead < len) {
        //determine which disk and block the operation goes too
        int diskID = current / (JBOD_BLOCK_SIZE * JBOD_NUM_BLOCKS_PER_DISK);
        int blockID = (current % (JBOD_BLOCK_SIZE * JBOD_NUM_BLOCKS_PER_DISK)) / JBOD_BLOCK_SIZE;
        uint32_t offset = current % JBOD_BLOCK_SIZE;
        
        uint8_t block[JBOD_BLOCK_SIZE];

        // see in cache
        if (cache_enabled() && cache_lookup(diskID, blockID, block) == 1) {
            // if hits then u can skip jbod ops
        } else {
        // find the specified disk block to read current data.
            uint32_t seekdisk = (0 << 24) | (diskID << 20) | JBOD_SEEK_TO_DISK;
            if (jbod_client_operation(seekdisk, NULL) == -1) {
                return -1;
            }
            uint32_t seekblock = (blockID << 24) | (0 << 20) | JBOD_SEEK_TO_BLOCK;
            if (jbod_client_operation(seekblock, NULL) == -1) {
                return -1;
            }
            uint32_t readBlock = (0 << 24) | (0 << 20) | JBOD_READ_BLOCK;
            if (jbod_client_operation(readBlock, block) == -1) {
                return -1;
            }
            // insert into cache 
            if (cache_enabled()) {
                cache_insert(diskID, blockID, block);
            }
        }

        // // figure out number of bytes to copy
        uint32_t bytesToCopy = JBOD_BLOCK_SIZE - offset;
        if (bytesToCopy > (len - bytesRead)) {
            bytesToCopy = len - bytesRead;
        }
        memcpy(buf + bytesRead, block + offset, bytesToCopy);

        bytesRead += bytesToCopy;
        current += bytesToCopy;
    }
    return bytesRead;
}
int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {
    // check conditions like mdadm read
    if (mounted == 0) {
        return -1;
    }
    
    if (len > 1024) {
        return -1;
    }
 
    if (len + addr > JBOD_NUM_DISKS * JBOD_BLOCK_SIZE * JBOD_NUM_BLOCKS_PER_DISK) {
        return -1;
    }
    
    if (len != 0 && buf == NULL) {
        return -1;
    }
    
    if (len == 0 && buf == NULL) {
        return 0;
    }

    uint32_t current = addr;
    uint32_t bytesWrote = 0;

    while (bytesWrote < len) {
        // decide disk, block offset in block
        int diskID = current / (JBOD_BLOCK_SIZE * JBOD_NUM_BLOCKS_PER_DISK);
        int blockID = (current % (JBOD_BLOCK_SIZE * JBOD_NUM_BLOCKS_PER_DISK)) / JBOD_BLOCK_SIZE;
        uint32_t offset = current % JBOD_BLOCK_SIZE;
        
        // find # of bytes to write in the block
        uint32_t bytesneedcopy = JBOD_BLOCK_SIZE - offset;
        if (bytesneedcopy > (len - bytesWrote)) {
            bytesneedcopy = len - bytesWrote;
        }
        
        uint8_t block[JBOD_BLOCK_SIZE];
        // otherwise read and write as needed
        if (offset == 0 && bytesneedcopy == JBOD_BLOCK_SIZE) {
            memcpy(block, buf + bytesWrote, JBOD_BLOCK_SIZE);
        } else {
            if (cache_enabled() && cache_lookup(diskID, blockID, block) == 1) {
            } else {
                // miss jbod ops
                // find the specified disk block to read current data.
                uint32_t seekdisk = (0 << 24) | (diskID << 20) | JBOD_SEEK_TO_DISK;
                if (jbod_client_operation(seekdisk, NULL) == -1) {
                    return -1;
                }
                uint32_t seekblock = (blockID << 24) | (0 << 20) | JBOD_SEEK_TO_BLOCK;
                if (jbod_client_operation(seekblock, NULL) == -1) {
                    return -1;
                }
                uint32_t readBlock = (0 << 24) | (0 << 20) | JBOD_READ_BLOCK;
                if (jbod_client_operation(readBlock, block) == -1) {
                    return -1;
                }
            }
            //update block w data
            memcpy(block + offset, buf + bytesWrote, bytesneedcopy);
        }
        
        // find correct disk and block for writing.
        uint32_t seekdisk = (0 << 24) | (diskID << 20) | JBOD_SEEK_TO_DISK;
        if (jbod_client_operation(seekdisk, NULL) == -1) {
            return -1;
        }
        uint32_t seekblock = (blockID << 24) | (0 << 20) | JBOD_SEEK_TO_BLOCK;
        if (jbod_client_operation(seekblock, NULL) == -1) {
            return -1;
        }
        // write the updated block to disk using jbodwriteblock.
        uint32_t writeBlock = (0 << 24) | (0 << 20) | JBOD_WRITE_BLOCK;
        if (jbod_client_operation(writeBlock, block) == -1) {
            return -1;
        }
        
        // update cache write thru
        if (cache_enabled()) {
            cache_update(diskID, blockID, block);
        }
        
        bytesWrote += bytesneedcopy;
        current += bytesneedcopy;
    }
    
    return bytesWrote;
}



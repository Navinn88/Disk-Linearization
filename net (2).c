#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"

/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

/* attempts to read n (len) bytes from fd; returns true on success and false on failure. 
It may need to call the system call "read" multiple times to reach the given size len. 
*/
static bool nread(int fd, int len, uint8_t *buf) {
    size_t recieved = 0;
    while (len > recieved) {
        // read remaining bytes
        ssize_t chunk = read(fd, recieved + buf, len - recieved);
        if (chunk <= 0) {
            // less then 0 means error
            return false;
        }
        recieved += (size_t) chunk;
    }
    return true;
}

/* attempts to write n bytes to fd; returns true on success and false on failure 
It may need to call the system call "write" multiple times to reach the size len.
*/
static bool nwrite(int fd, int len, uint8_t *buf) {
    size_t recieved = 0;
    while (recieved < len) {
        //write remaining bytes
        ssize_t written = write(fd, recieved + buf, len - recieved);
        if (written <= 0) {
            // less then 0 meanserror
            return false;
        }
        recieved += (size_t)written;
    }
    return true;
}

/* Through this function call the client attempts to receive a packet from sd 
(i.e., receiving a response from the server.). It happens after the client previously 
forwarded a jbod operation call via a request message to the server.  
It returns true on success and false on failure. 
The values of the parameters (including op, ret, block) will be returned to the caller of this function: 

op - the address to store the jbod "opcode"  
ret - the address to store the return value of the server side calling the corresponding jbod_operation function.
block - holds the received block content if existing (e.g., when the op command is JBOD_READ_BLOCK)

In your implementation, you can read the packet header first (i.e., read HEADER_LEN bytes first), 
and then use the length field in the header to determine whether it is needed to read 
a block of data from the server. You may use the above nread function here.  
*/
// pull 16-bit value change pointer
static inline uint16_t extract_u16(const uint8_t **ptr) {
    uint16_t value;
    memcpy(&value, *ptr, sizeof(value));
    *ptr += sizeof(value);
    return value;
}

//pull a 32-bit value cahnge the pointer
static inline uint32_t extract_u32(const uint8_t **ptr) {
    uint32_t value;
    memcpy(&value, *ptr, sizeof(value));
    *ptr += sizeof(value);
    return value;
}

static bool recv_packet(int sd, uint32_t *op, uint16_t *ret, uint8_t *block) {
    uint8_t header[HEADER_LEN];
    // read header
    if (!nread(sd, HEADER_LEN, header)) {
        return false;
    }

    // parse using pointer arthrimetic
    const uint8_t *cursor = header;
    uint16_t netlen = extract_u16(&cursor);
    uint32_t netop  = extract_u32(&cursor);
    uint16_t netret = extract_u16(&cursor);

    // change from net byte order
    *op  = ntohl(netop);
    *ret = ntohs(netret);

    // calc payload and read
    uint16_t packetlen  = ntohs(netlen);
    if (packetlen <= HEADER_LEN) {
        return true;
    }
    uint16_t payloadlen = packetlen - HEADER_LEN;
    return nread(sd, payloadlen, block);
}

/* The client attempts to send a jbod request packet to sd (i.e., the server socket here); 
returns true on success and false on failure. 

op - the opcode. 
block- when the command is JBOD_WRITE_BLOCK, the block will contain data to write to the server jbod system;
otherwise it is NULL.

The above information (when applicable) has to be wrapped into a jbod request packet (format specified in readme).
You may call the above nwrite function to do the actual sending.  
*/
// copy n bytes and cahnge dest pointer
static inline uint8_t *copy_advance(uint8_t *dst, const void *src, size_t n) {
    memcpy(dst, src, n);
    return dst + n;
}

static bool send_packet(int sd, uint32_t op, uint8_t *block) {
    // calc total length header + optional block
    uint16_t payloadlen = (op == JBOD_WRITE_BLOCK && block) ? JBOD_BLOCK_SIZE : 0;
    uint16_t total   = HEADER_LEN + payloadlen;

    // prepare network fields
    uint16_t netlen = htons(total);
    uint32_t netop  = htonl(op);
    uint16_t netret = htons(0);

    // create packet in single buf
    uint8_t packet[JBOD_BLOCK_SIZE + HEADER_LEN];
    uint8_t *ptr = packet;
    ptr = copy_advance(ptr, &netlen, sizeof(netlen));
    ptr = copy_advance(ptr, &netop,  sizeof(netop));
    ptr = copy_advance(ptr, &netret, sizeof(netret));

    // add payload when writing a block
    if (payloadlen) {
        ptr = copy_advance(ptr, block, JBOD_BLOCK_SIZE);
    }

    // send all bytes at end
    return nwrite(sd, total, packet);
}
/* attempts to connect to server and set the global cli_sd variable to the
 * socket; returns true if successful and false if not. 
 * this function will be invoked by tester to connect to the server at given ip and port.
 * you will not call it in mdadm.c
*/
bool jbod_connect(const char *ip, uint16_t port) {
    struct sockaddr_in saddr;
    
    //step 1 config address 
    saddr.sin_family = AF_INET;
    saddr.sin_port   = htons(port);
    if (inet_pton(AF_INET, ip, &saddr.sin_addr) != 1) {
        cli_sd = -1;
        return false;
    }
    
    /// step 2 create socket 
    cli_sd = socket(AF_INET, SOCK_STREAM, 0);
    if (cli_sd < 0) {
        cli_sd = -1;
        return false;
    }

    // step 3 attempt to connect to server
    if (connect(cli_sd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        cli_sd = -1;
        return false;
    }

    return true;
}

/* disconnects from the server and resets cli_sd */
void jbod_disconnect(void) {
  if (cli_sd >= 0) {
        close(cli_sd);
        cli_sd = -1;
    }
  else
    cli_sd = -1;
}

/* sends the JBOD operation to the server (use the send_packet function) and receives 
(use the recv_packet function) and processes the response. 

The meaning of each parameter is the same as in the original jbod_operation function. 
return: 0 means success, -1 means failure.
*/
int jbod_client_operation(uint32_t op, uint8_t *block) {
    uint16_t status;
    uint32_t tag;

    //send request and await response
    if ( send_packet(cli_sd, op, block)
      && recv_packet(cli_sd, &tag, &status, block) )
    {
        return (int16_t)status;
    }

    //return -1 on failure
    return -1;
}

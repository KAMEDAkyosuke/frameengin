//
//  frameengine.h
//  FrameEngine
//
//  Created by organlounge on 2013/10/02.
//  Copyright (c) 2013年 organlounge. All rights reserved.
//

#ifndef FrameEngine_frameengine_h
#define FrameEngine_frameengine_h

#include <stdint.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>

extern int FE_result_ok;
extern int FE_result_timeout;
extern int FE_result_closed;

typedef struct FE_tcp_context{
    int socket;
    uint16_t  local_port;
    
    in_addr_t target_ipv4;
    uint16_t  target_port;
    
    // callbacks
    void(*on_connect_timeout)(struct FE_tcp_context *ctx);
    void(*on_connect)(struct FE_tcp_context *ctx, uint16_t local_port);
    void(*on_read_timeout)(struct FE_tcp_context *ctx);
    void(*on_read)(struct FE_tcp_context *ctx, int8_t *buf, ssize_t len);
    
    void *data;
} FE_tcp_context;

int FE_tcp_init_ipv4(FE_tcp_context *ctx);
int FE_tcp_connect_ipv4(FE_tcp_context *ctx, struct timeval *timeout);
int FE_tcp_write(FE_tcp_context *ctx, int8_t *buf, int bytes);
int FE_tcp_read(FE_tcp_context *ctx);
int FE_tcp_read_block(FE_tcp_context *ctx, struct timeval *timeout);
int FE_tcp_close(FE_tcp_context *ctx);

typedef struct FE_udp_context{
    int socket;
    uint16_t  local_port;
    
    in_addr_t target_ipv4;
    uint16_t  target_port;
    
    // callbacks
    void(*on_read_timeout)(struct FE_udp_context *ctx);
    void(*on_read)(struct FE_udp_context *ctx, int8_t *buf, ssize_t len,
                   uint16_t local_port, struct sockaddr_in *target_sin);
    
    void *data;
} FE_udp_context;

int FE_udp_init_ipv4(FE_udp_context *ctx);
int FE_udp_write_ipv4(FE_udp_context *ctx, int8_t *buf, int bytes);
int FE_udp_read_ipv4(FE_udp_context *ctx);
int FE_udp_read_block_ipv4(FE_udp_context *ctx, struct timeval *timeout);
int FE_udp_close(FE_udp_context *ctx);

#endif

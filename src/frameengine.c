//  frameengine.c
//  FrameEngine
//
//  Created by organlounge on 2013/10/02.
//  Copyright (c) 2013年 organlounge. All rights reserved.
//

#include "frameengine.h"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/fcntl.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>

int FE_result_ok      =  0;
int FE_result_timeout = -1;
int FE_result_closed  = -2;

static int tcp_read(FE_tcp_context *ctx);
static int tcp_recv_msg(FE_tcp_context *ctx);
static int udp_read_ipv4(FE_udp_context *ctx);
static int udp_recv_msg_ipv4(FE_udp_context *ctx);

int FE_tcp_init_ipv4(FE_tcp_context *ctx)
{
    ctx->target_ipv4 = 0;
    ctx->target_port = 0;
    
    ctx->on_connect_timeout = NULL;
    ctx->on_connect         = NULL;
    ctx->on_read_timeout    = NULL;
    ctx->on_read            = NULL;
    
    ctx->data = NULL;
    
    // setup socket
    ctx->socket = -1;
    
    ctx->socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(ctx->socket != -1);
    
    // sigpipe を無視する。
    int value = 1;
    if(setsockopt(ctx->socket, SOL_SOCKET, SO_NOSIGPIPE, &value, sizeof(value)) != 0){
        perror("setsockopt fail");
        assert(false);
    }
    
    // non-blocking
    int flags = fcntl(ctx->socket, F_GETFL, 0);
    if (flags < 0) {
        perror("fcntl(F_GETFL) failed");
        assert(false);
    }
    if (fcntl(ctx->socket, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("fcntl(F_SETFL) failed");
        assert(false);
    }
    return FE_result_ok;
}

int FE_tcp_connect_ipv4(FE_tcp_context *ctx, struct timeval *timeout)
{
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = ctx->target_ipv4;
    sin.sin_port        = htons(ctx->target_port);
    
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 1000;    // 1ms
reconnect:
    if((connect(ctx->socket, (struct sockaddr *) &sin, sizeof(sin)) < 0)){
        int err = errno;
        perror("connect fail");
        switch(err){
            case EISCONN:    /* connected */
            case ECONNREFUSED:
                puts("connected");
                struct sockaddr_in sin;
                socklen_t len = sizeof(sin);
                if(getsockname(ctx->socket, (struct sockaddr *) &sin, &len) != 0){
                    perror("getsockname failed");
                    assert(false);
                }
                if(ctx->on_connect != NULL){
                    ctx->on_connect(ctx, ntohs(sin.sin_port));
                }
                break;
            case EINPROGRESS:
            case EALREADY:
            {
                /* sleep */
                select(0, NULL, NULL, NULL, &tv);
                if(timercmp(&tv, timeout, >)){
                    if(ctx->on_connect_timeout != NULL){
                        ctx->on_connect_timeout(ctx);
                    }
                    return FE_result_timeout;
                }
                timeradd(&tv, &tv, &tv);    // 次は2倍待つ
                puts("re connect");
                goto reconnect;
                break;
            }
            default:
                printf("errno = %d\n", err);
                assert(false);
        }
    }
    return FE_result_ok;
}

int FE_tcp_write(FE_tcp_context *ctx, int8_t *buf, int bytes)
{
    ssize_t ret;
    int write_len = 0;
    int err;
    for(;;){
        if(bytes <= write_len){    // すべて書き込んだ場合
            break;
        }
        ret = write(ctx->socket, buf, bytes - write_len);
        if (ret == -1) {
            err = errno;
            perror("write fail");
            switch (err) {
                case EINTR:     // シグナルを検知した場合
                    continue;
                    break;
                case EAGAIN:    // ブロックが発生した場合
                    return write_len;    // 今まで書き込んだ byte 数を返す
                    break;
                case EPIPE:     // 読み取り側がクローズした場合
                    return FE_result_closed;
                    break;
                default:
                    printf("errno = %d\n", err);
                    assert(false);
            }
        }
        write_len += ret;
        buf += ret;
    }
    return write_len;
}

int FE_tcp_read(FE_tcp_context *ctx)
{
    return tcp_read(ctx);
}

int FE_tcp_read_block(FE_tcp_context *ctx, struct timeval *timeout)
{
    int r;
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(ctx->socket, &readfds);
    
    r = select(ctx->socket + 1, &readfds, NULL, NULL, timeout);
    if(r == 0){    // timeout
        if(ctx->on_read_timeout != NULL){
            ctx->on_read_timeout(ctx);
        }
        return FE_result_timeout;
    }
    else if(r == -1){
        int err = errno;
        printf("errno = %d\n", err);
        assert(false);
    }
    else{
        if(FD_ISSET(ctx->socket, &readfds)){
            return tcp_read(ctx);
        }
        else{
            assert(false);
        }
    }
    return FE_result_ok;
}

int FE_tcp_send_msg(FE_tcp_context *ctx, int8_t *buf, int bytes)
{
    ssize_t ret;
    int write_len = 0;
    int err;
    for(;;){
        if(bytes <= write_len){    // すべて書き込んだ場合
            break;
        }
        struct iovec iov;
        iov.iov_base = buf;
        iov.iov_len  = bytes - write_len;
        
        struct msghdr msg;
        msg.msg_name       = NULL;
        msg.msg_namelen    = 0;
        msg.msg_iov        = &iov;
        msg.msg_iovlen     = 1;
        msg.msg_control    = NULL;
        msg.msg_controllen = 0;
        msg.msg_flags      = 0;
        
        ret = sendmsg(ctx->socket, &msg, 0);
        if(ret == -1){
            err = errno;
            perror("sendmsg fail");
            switch (err) {
            case EINTR:     // シグナルを検知した場合
                continue;
                break;
            case EAGAIN:    // ブロックが発生した場合
                return write_len;    // 今まで書き込んだ byte 数を返す
                break;
            case EPIPE:     // 読み取り側がクローズした場合
                return FE_result_closed;
                break;
            default:
                printf("errno = %d\n", err);
                assert(false);
            }
        }
        write_len += ret;
        buf += ret;
    }
    return write_len;
}

int FE_tcp_recv_msg(FE_tcp_context *ctx)
{
    return tcp_recv_msg(ctx);
}

int FE_tcp_recv_msg_block(FE_tcp_context *ctx, struct timeval *timeout)
{
   int r;
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(ctx->socket, &readfds);
    
    r = select(ctx->socket + 1, &readfds, NULL, NULL, timeout);
    if(r == 0){    // timeout
        if(ctx->on_read_timeout != NULL){
            ctx->on_read_timeout(ctx);
        }
        return FE_result_timeout;
    }
    else if(r == -1){
        int err = errno;
        printf("errno = %d\n", err);
        assert(false);
    }
    else{
        if(FD_ISSET(ctx->socket, &readfds)){
            return tcp_recv_msg(ctx);
        }
        else{
            assert(false);
        }
    }
    return FE_result_ok;
}

int FE_tcp_close(FE_tcp_context *ctx)
{
    if(close(ctx->socket) != 0){
        perror("close fail");
    }
    return FE_result_ok;
}

int FE_udp_init_ipv4(FE_udp_context *ctx)
{
    ctx->target_ipv4 = 0;
    ctx->target_port = 0;

    ctx->on_read_timeout = NULL;
    ctx->on_read         = NULL;
    
    ctx->data = NULL;
    
    // setup socket
    ctx->socket = -1;
    
    ctx->socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    assert(ctx->socket != -1);
        
    // sigpipe を無視する。
    int value = 1;
    if(setsockopt(ctx->socket, SOL_SOCKET, SO_NOSIGPIPE, &value, sizeof(value)) != 0){
        perror("setsockopt fail");
        assert(false);
    }
    
    // non-blocking
    int flags = fcntl(ctx->socket, F_GETFL, 0);
    if (flags < 0) {
        perror("fcntl(F_GETFL) failed");
        assert(false);
    }
    if (fcntl(ctx->socket, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("fcntl(F_SETFL) failed");
        assert(false);
    }
    return FE_result_ok;
}

int FE_udp_write_ipv4(FE_udp_context *ctx, int8_t *buf, int bytes)
{
    struct sockaddr_in addr;
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = ctx->target_ipv4;
    addr.sin_port        = htons(ctx->target_port);
    
    ssize_t ret;
    int write_len = 0;
    int err;
    for(;;){
        if(bytes <= write_len){    // すべて書き込んだ場合
            break;
        }
        ret = sendto(ctx->socket, buf, bytes - write_len, 0, (struct sockaddr*)&addr, sizeof(addr));
        if (ret == -1) {
            err = errno;
            perror("sendto fail");
            switch (err) {
                case EINTR:     // シグナルを検知した場合
                    continue;
                    break;
                case EAGAIN:    // ブロックが発生した場合
                    return write_len;    // 今まで書き込んだ byte 数を返す
                    break;
                default:
                    printf("errno = %d\n", err);
                    assert(false);
            }
        }
        write_len += ret;
        buf += ret;
    }
    return bytes;
}

int FE_udp_read_ipv4(FE_udp_context *ctx)
{
    return udp_read_ipv4(ctx);
}

int FE_udp_read_block_ipv4(FE_udp_context *ctx, struct timeval *timeout)
{
    int r;
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(ctx->socket, &readfds);
    
    r = select(ctx->socket + 1, &readfds, NULL, NULL, timeout);
    if(r == 0){    // timeout
        if(ctx->on_read_timeout != NULL){
            ctx->on_read_timeout(ctx);
        }
        return FE_result_timeout;
    }
    else if(r == -1){
        int err = errno;
        printf("errno = %d\n", err);
        switch (err) {
            case EINTR:     // シグナルを検知した場合
                // 何もしない
                break;
            default:
                assert(false);
                break;
        }
    }
    else{
        if(FD_ISSET(ctx->socket, &readfds)){
            return udp_read_ipv4(ctx);
        }
        else{
            assert(false);
        }
    }
    return FE_result_ok;
}

int FE_udp_send_msg_ipv4(FE_udp_context *ctx, int8_t *buf, int bytes)
{
    struct sockaddr_in addr;
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = ctx->target_ipv4;
    addr.sin_port        = htons(ctx->target_port);

    ssize_t ret;
    int write_len = 0;
    int err;
    for(;;){
        if(bytes <= write_len){    // すべて書き込んだ場合
            break;
        }
        struct iovec iov;
        iov.iov_base = buf;
        iov.iov_len  = bytes - write_len;
        
        struct msghdr msg;
        msg.msg_name       = (struct sockaddr*)&addr;
        msg.msg_namelen    = sizeof(addr);
        msg.msg_iov        = &iov;
        msg.msg_iovlen     = 1;
        msg.msg_control    = NULL;
        msg.msg_controllen = 0;
        msg.msg_flags      = 0;
        
        ret = sendmsg(ctx->socket, &msg, 0);
        if(ret == -1){
            err = errno;
            perror("sendmsg fail");
            switch (err) {
            case EINTR:     // シグナルを検知した場合
                continue;
                break;
            case EAGAIN:    // ブロックが発生した場合
                return write_len;    // 今まで書き込んだ byte 数を返す
                break;
            case EPIPE:     // 読み取り側がクローズした場合
                return FE_result_closed;
                break;
            default:
                printf("errno = %d\n", err);
                assert(false);
            }
        }
        write_len += ret;
        buf += ret;
    }
    return write_len;
}

int FE_udp_recv_msg_ipv4(FE_udp_context *ctx)
{
    return udp_recv_msg_ipv4(ctx);
}

int FE_udp_recv_msg_block_ipv4(FE_udp_context *ctx, struct timeval *timeout)
{
   int r;
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(ctx->socket, &readfds);
    
    r = select(ctx->socket + 1, &readfds, NULL, NULL, timeout);
    if(r == 0){    // timeout
        if(ctx->on_read_timeout != NULL){
            ctx->on_read_timeout(ctx);
        }
        return FE_result_timeout;
    }
    else if(r == -1){
        int err = errno;
        printf("errno = %d\n", err);
        assert(false);
    }
    else{
        if(FD_ISSET(ctx->socket, &readfds)){
            return udp_recv_msg_ipv4(ctx);
        }
        else{
            assert(false);
        }
    }
    return FE_result_ok;
}

int FE_udp_close(FE_udp_context *ctx)
{
    if(close(ctx->socket) != 0){
        perror("close fail");
    }
    return FE_result_ok;
}

static int tcp_read(FE_tcp_context *ctx)
{
    ssize_t ret;
    static int8_t buf[4096];
    int err;
    for(;;){
        ret = read(ctx->socket, buf, sizeof(buf)/sizeof(buf[0]));
        if(ret == 0){    // ソケットがクローズされた場合
            return FE_result_closed;
        }
        else if(ret == -1) {
            err = errno;
            perror("read fail");
            switch (err) {
                case EINTR:     // シグナルを検知した場合
                    continue;
                    break;
                case EAGAIN:    // ブロックが発生した場合
                    return FE_result_ok;
                    break;
                case ECONNRESET:
                    return FE_result_closed;
                    break;
                default:
                    printf("errno = %d\n", err);
                    assert(false);
            }
        }
        if(ctx->on_read != NULL){
            ctx->on_read(ctx, buf, ret);
        }
    }
    return FE_result_ok;
}

static int tcp_recv_msg(FE_tcp_context *ctx)
{
    ssize_t ret;
    static int8_t buf[4096];
    static struct iovec iov;
    static struct msghdr msg;
    int err;
    for(;;){
        iov.iov_base = buf;
        iov.iov_len  = sizeof(buf)/sizeof(buf[0]);

        msg.msg_name       = NULL;
        msg.msg_namelen    = 0;
        msg.msg_iov        = &iov;
        msg.msg_iovlen     = 1;
        msg.msg_control    = NULL;
        msg.msg_controllen = 0;
        msg.msg_flags      = 0;
        
        ret = recvmsg(ctx->socket, &msg, 0);
        if(ret == 0){    // ソケットがクローズされた場合
            return FE_result_closed;
        }
        else if(ret == -1){
            err = errno;
            perror("recvmsg fail");
            switch (err) {
                case EINTR:     // シグナルを検知した場合
                    continue;
                    break;
                case EAGAIN:    // ブロックが発生した場合
                    return FE_result_ok;
                    break;
                case ECONNRESET:
                    return FE_result_closed;
                    break;
                default:
                    printf("errno = %d\n", err);
                    assert(false);
            }
        }

        // TODO:
        // check msg.msg_flags
        if(ctx->on_read != NULL){
            ctx->on_read(ctx, iov.iov_base, ret);
        }
    }
    return FE_result_ok;
}

static int udp_read_ipv4(FE_udp_context *ctx)
{
    ssize_t ret;
    static int8_t buf[4096];
    int err;
    struct sockaddr_in sin;
    socklen_t len = sizeof(sin);
    for(;;){
        ret = recvfrom(ctx->socket, buf, sizeof(buf)/sizeof(buf[0]), 0, (struct sockaddr*)&sin, &len);
        if(ret == -1) {
            err = errno;
            perror("read fail");
            switch (err) {
                case EINTR:     // シグナルを検知した場合
                    continue;
                    break;
                case EAGAIN:    // ブロックが発生した場合
                    return FE_result_ok;
                    break;
                default:
                    printf("errno = %d\n", err);
                    assert(false);
            }
        }
        
        struct sockaddr_in local;
        socklen_t len = sizeof(local);
        if(getsockname(ctx->socket, (struct sockaddr *) &local, &len) != 0){
            perror("getsockname failed");
            assert(false);
        }
        if(ctx->on_read != NULL){
            ctx->on_read(ctx, buf, ret, ntohs(local.sin_port), &sin);
        }
    }
    return FE_result_ok;
}

static int udp_recv_msg_ipv4(FE_udp_context *ctx)
{
    struct sockaddr_in addr;
    ssize_t ret;
    static int8_t buf[4096];
    static struct iovec iov;
    static struct msghdr msg;
    int err;
    for(;;){
        iov.iov_base = buf;
        iov.iov_len  = sizeof(buf)/sizeof(buf[0]);

        msg.msg_name       = (struct sockaddr*)&addr;
        msg.msg_namelen    = sizeof(addr);
        msg.msg_iov        = &iov;
        msg.msg_iovlen     = 1;
        msg.msg_control    = NULL;
        msg.msg_controllen = 0;
        msg.msg_flags      = 0;
        
        ret = recvmsg(ctx->socket, &msg, 0);
        if(ret == 0){    // ソケットがクローズされた場合
            return FE_result_closed;
        }
        else if(ret == -1){
            err = errno;
            perror("recvmsg fail");
            switch (err) {
                case EINTR:     // シグナルを検知した場合
                    continue;
                    break;
                case EAGAIN:    // ブロックが発生した場合
                    return FE_result_ok;
                    break;
                default:
                    printf("errno = %d\n", err);
                    assert(false);
            }
        }

        // TODO:
        // check msg.msg_flags
        struct sockaddr_in sin;
        socklen_t len = sizeof(sin);
        if(getsockname(ctx->socket, (struct sockaddr *) &sin, &len) != 0){
            perror("getsockname failed");
            assert(false);
        }
        if(ctx->on_read != NULL){
            ctx->on_read(ctx, iov.iov_base, ret, ntohs(sin.sin_port), msg.msg_name);
        }
    }
    return FE_result_ok;
}

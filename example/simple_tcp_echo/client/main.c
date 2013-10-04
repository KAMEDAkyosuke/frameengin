#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <arpa/inet.h>
#include <assert.h>

#include "../../../src/frameengine.h"


static void on_connect_timeout(struct FE_tcp_context *ctx);
static void on_connect(struct FE_tcp_context *ctx, uint16_t local_port);
static void on_read_timeout(struct FE_tcp_context *ctx);
static void on_read(struct FE_tcp_context *ctx, int8_t *buf, ssize_t len);

int main(int argc, char** argv)
{
    FE_tcp_context ctx;
    FE_tcp_init_ipv4(&ctx);
    
    ctx.on_connect_timeout = on_connect_timeout;
    ctx.on_connect         = on_connect;
    ctx.on_read_timeout    = on_read_timeout;
    ctx.on_read            = on_read;

    ctx.target_ipv4 = inet_addr("127.0.0.1");
    ctx.target_port = htons(12345);
    
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    if(FE_tcp_connect_ipv4(&ctx, &timeout) != FE_result_ok){
        assert(false);
    }
    
    char *buf = "hello world";
    int rest  = strlen(buf);
    while(0 < rest){
        int len = FE_tcp_write(&ctx, (int8_t*)buf, rest);
        if(len < 0){
            assert(false);
        }
        rest -= len;
        buf  += len;
    }

    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    
    for(;;){
        int r = FE_tcp_read_block(&ctx, &timeout);
        if(r == FE_result_ok || r == FE_result_again || r == FE_result_closed){
            break;
        }
        else{
            assert(false);
        }
    }
    FE_tcp_close(&ctx);

    return 0;
}

static void on_connect_timeout(struct FE_tcp_context *ctx)
{
    puts("on_connect_timeout");
}

static void on_connect(struct FE_tcp_context *ctx, uint16_t local_port)
{
    puts("on_connect");
}

static void on_read_timeout(struct FE_tcp_context *ctx)
{
    puts("on_read_timeout");
}

static void on_read(struct FE_tcp_context *ctx, int8_t *buf, ssize_t len)
{
    puts("on_read");
    char s[1024] = {0};
    memcpy(s, buf, sizeof(int8_t)*len);
    puts(s);
}

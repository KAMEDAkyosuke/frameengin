#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <arpa/inet.h>
#include <assert.h>

#include "../../../src/frameengine.h"


static void on_read_timeout(struct FE_udp_context *ctx);
static void on_read(struct FE_udp_context *ctx, int8_t *buf, ssize_t len,
                    uint16_t local_port, struct sockaddr_in *target_sin);

int main(int argc, char** argv)
{
    FE_udp_context ctx;
    FE_udp_init_ipv4(&ctx);
    
    ctx.on_read_timeout = on_read_timeout;
    ctx.on_read         = on_read;

    ctx.target_ipv4 = inet_addr("127.0.0.1");
    ctx.target_port = htons(12345);
    
    char *buf = "hello world";
    int rest  = strlen(buf);
    while(0 < rest){
        int len = FE_udp_send_msg_ipv4(&ctx, (int8_t*)buf, rest);
        if(len < 0){
            assert(false);
        }
        rest -= len;
        buf  += len;
    }

    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    
    for(;;){
        int r = FE_udp_recv_msg_block_ipv4(&ctx, &timeout);
        if(r == FE_result_ok || r == FE_result_closed){
            break;
        }
        else{
            assert(false);
        }
    }
    FE_udp_close(&ctx);

    return 0;
}

static void on_read_timeout(struct FE_udp_context *ctx)
{
    puts("on_read_timeout");
}

static void on_read(struct FE_udp_context *ctx, int8_t *buf, ssize_t len,
                    uint16_t local_port, struct sockaddr_in *target_sin)
{
    puts("on_read");
    char s[1024] = {0};
    memcpy(s, buf, sizeof(int8_t)*len);
    puts(s);
    printf("local port = %u\n",  ntohs(local_port));
    printf("target host = %s\n", inet_ntoa(target_sin->sin_addr));
    printf("target port = %u\n", ntohs(target_sin->sin_port));
}

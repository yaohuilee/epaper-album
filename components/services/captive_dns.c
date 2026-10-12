#include "provision_address.h"
#include "epaper_network.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include <string.h>

static int dns_socket=-1;
static volatile bool stop_dns;
static SemaphoreHandle_t stopped;
static void dns_task(void *arg)
{
    (void)arg;
    uint8_t packet[512];
    while(!stop_dns) {
        struct sockaddr_in peer;socklen_t peer_size=sizeof(peer);
        int n=recvfrom(dns_socket,packet,sizeof(packet)-16,0,(struct sockaddr*)&peer,&peer_size);
        if(n<12 || packet[4]!=0 || packet[5]!=1 || (packet[2]&0xf8)) continue;
        size_t end=12;
        while(end<(size_t)n && packet[end]) {
            unsigned len=packet[end++];
            if(len>63 || end+len>(size_t)n) {end=n;break;}
            end+=len;
        }
        if(end+5>(size_t)n) continue;
        end++;bool a=packet[end]==0 && packet[end+1]==1 && packet[end+2]==0 && packet[end+3]==1;
        end+=4;packet[2]=0x81;packet[3]=0x80;
        packet[6]=0;packet[7]=a?1:0;memset(packet+8,0,4);
        if(a) {
            const uint8_t answer[]={0xc0,0x0c,0,1,0,1,0,0,0,30,0,4,EPAPER_PROVISION_IP_OCTETS};
            memcpy(packet+end,answer,sizeof(answer));end+=sizeof(answer);
        }
        sendto(dns_socket,packet,end,0,(struct sockaddr*)&peer,peer_size);
    }
    xSemaphoreGive(stopped);vTaskDelete(NULL);
}
esp_err_t epaper_captive_dns_start(void)
{
    if(dns_socket>=0) return ESP_OK;
    stopped=xSemaphoreCreateBinary();if(!stopped) return ESP_ERR_NO_MEM;
    dns_socket=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    struct timeval timeout={.tv_sec=1};
    struct sockaddr_in addr={.sin_family=AF_INET,.sin_port=htons(53),.sin_addr.s_addr=inet_addr(EPAPER_PROVISION_IP)};
    if(dns_socket<0 || setsockopt(dns_socket,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout))<0 ||
       bind(dns_socket,(struct sockaddr*)&addr,sizeof(addr))<0) goto fail;
    stop_dns=false;
    if(xTaskCreate(dns_task,"captive_dns",3072,NULL,4,NULL)!=pdPASS) goto fail;
    return ESP_OK;
fail:
    if(dns_socket>=0) close(dns_socket);
    dns_socket=-1;
    vSemaphoreDelete(stopped);stopped=NULL;return ESP_FAIL;
}
void epaper_captive_dns_stop(void)
{
    if(dns_socket<0) return;
    stop_dns=true;xSemaphoreTake(stopped,portMAX_DELAY);
    close(dns_socket);dns_socket=-1;vSemaphoreDelete(stopped);stopped=NULL;
}

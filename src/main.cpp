#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Button2.h>
#include <Wire.h>
#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include <string>

#define LED_GPIO_PIN                     5
#define WIFI_CHANNEL_SWITCH_INTERVAL  (500)
#define WIFI_CHANNEL_MAX               (13)

typedef struct device{
    std::string mac;
    int rssi;
    int timestamp;
    bool isempty;
    device *next;
    device *prev;
} device;

class devicelist {

    private:
        device *head;

    public:
        devicelist() {
            head = NULL;
        }

        void insert(std::string mac, int rssi, int timestamp) {
            device *tmp = new device;
            tmp->mac = mac;
            tmp->rssi = rssi;
            tmp->timestamp = timestamp;
            tmp->isempty = false;

            if (head == NULL) {
                head = tmp;
                head->prev = NULL;
            } else {
                device *pos;
                pos = head;


                while(pos->next != NULL) {
                    if (pos->mac.compare(mac) == 0) {
                        if (pos == head) {
                            head = pos->next;
                            break;
                        } else {
                            pos->prev->next = pos->next;
                            pos = pos->prev;
                        }
                    }
                    pos = pos->next;
                }
                pos = head;
                while(pos->next != NULL && rssi < pos->rssi)  {
                    pos = pos->next;
                }
                if (pos == head) {
                    tmp->next = pos;
                    tmp->prev = NULL;
                    pos->prev = tmp;
                    head = tmp;
                } else if (pos->next != NULL) {
                    tmp->next = pos->next;
                    tmp->prev = pos;
                    pos->next = tmp;
                } else {
                    tmp->next = NULL;
                    tmp->prev = pos;
                    pos->next = tmp;
                }
                
            }
        }
        device* get() {
            return head;
        }

        device* get(int n) {
            device *tmp;
            tmp = head;
            for(int i = 0; i < n; i++) {
                if (tmp->next != NULL) {
                    tmp = tmp->next;
                } else {
                    device *empty = new device;
                    empty->isempty = true;
                    return empty;
                }
            }
            return tmp;
        }

        int size() {
            int i = 0;
            device *tmp;
            tmp = head;
            while (tmp->next != NULL) {
                tmp = tmp->next;
                i++;
            }
            return i;
        }
};

devicelist devices;

uint8_t level = 0, channel = 1;
TFT_eSPI tft = TFT_eSPI(135, 240);
static wifi_country_t wifi_country = {.cc="CN", .schan = 1, .nchan = 13}; //Most recent esp32 library struct

typedef struct {
    unsigned frame_ctrl:16;
    unsigned duration_id:16;
    uint8_t addr1[6];               /* receiver address */
    uint8_t addr2[6];               /* sender address */
    uint8_t addr3[6];               /* filtering address */
    unsigned sequence_ctrl:16;
    uint8_t addr4[6];               /* optional */
} wifi_ieee80211_mac_hdr_t;

typedef struct {
    wifi_ieee80211_mac_hdr_t hdr;
    uint8_t payload[0];             /* network data ended with 4 bytes csum (CRC32) */
} wifi_ieee80211_packet_t;

static esp_err_t event_handler(void *ctx, system_event_t *event);
static void wifi_sniffer_init(void);
static void wifi_sniffer_set_channel(uint8_t channel);
static const char *wifi_sniffer_packet_type2str(wifi_promiscuous_pkt_type_t type);
static void wifi_sniffer_packet_handler(void *buff, wifi_promiscuous_pkt_type_t type);

esp_err_t event_handler(void *ctx, system_event_t *event) {
    return ESP_OK;
}

void wifi_sniffer_init(void) {
    nvs_flash_init();
    tcpip_adapter_init();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_country(&wifi_country) );             /* set country for channel range [1, 13] */
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_NULL) );               //Weder AP noch Station
    ESP_ERROR_CHECK( esp_wifi_start() );
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_packet_handler);
}

void wifi_sniffer_set_channel(uint8_t channel) {
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
}

const char * wifi_sniffer_packet_type2str(wifi_promiscuous_pkt_type_t type) {
    switch(type) {
        case WIFI_PKT_MGMT: return "MGMT";
        case WIFI_PKT_DATA: return "DATA";
    default:  
      case WIFI_PKT_MISC: return "MISC";
    }
}

void wifi_sniffer_packet_handler(void* buff, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT)
        return;

    const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buff;
    const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)ppkt->payload;
    const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;
    
    char buffer[18];
    sprintf(buffer,   "%02x:%02x:%02x:%02x:%02x:%02x",
                            hdr->addr2[0],hdr->addr2[1],hdr->addr2[2],
                            hdr->addr2[3],hdr->addr2[4],hdr->addr2[5]
                        );
    devices.insert(buffer, ppkt->rx_ctrl.rssi, millis());
}

// the setup function runs once when you press reset or power the board
void setup() {
    Serial.begin(115200);
    delay(10);
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    
    wifi_sniffer_init();
}

// the loop function runs over and over again forever
void loop() {
    //wifi_sniffer_set_channel(channel);
    //channel = (channel % WIFI_CHANNEL_MAX) + 1;

    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0,0);
    tft.printf("devices: %d\n", devices.size());
    /*
    for (int i = 0; i < devices.size(); i++) {
        tft.printf("%s rssi: %d\n", devices.get(i)->mac.c_str(), devices.get(i)->rssi);
    }*/
    device *tmp = devices.get();
    while (tmp->next != NULL) {
        tft.printf("%s rssi: %d\n", tmp->mac.c_str(), tmp->rssi);
        tmp = tmp->next;
    }
    delay(1000);
}


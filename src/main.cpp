#include <TFT_eSPI.h>
#include <Button2.h>
#include <Wire.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "EEPROM.h"

#include <string>
#include <list>

#define WIFI_CHANNEL_SWITCH_INTERVAL        100
#define WIFI_CHANNEL_MAX                    13
#define BUTTON_UP                           35
#define BUTTON_DOWN                         0
#define TFT_REFRESH                         100
#define SCROLLOFF                           16

#define TFT_WIDTH                           240
#define TFT_HEIGHT                          135

typedef struct device{
    std::string mac;
    int rssi;
    int timestamp;
    device *next;
    device *prev;
} device;

class devicelist {

    private:
        device *head, *tail;

    public:
        devicelist() {
            head = new device;
            tail = new device;
            head->next = tail;
            tail->prev = head;
        }

        void insert(std::string mac, int rssi, int timestamp) {
            device *pos = head->next;
            while (pos != tail) {
                if (pos->mac.compare(mac) == 0) {
                    pos->rssi = rssi;
                    pos->timestamp = timestamp;
                    return;
                }
                pos = pos->next;
            }
            device *tmp = new device;
            tmp->mac = mac;
            tmp->rssi = rssi;
            tmp->timestamp = timestamp;

            tmp->prev = tail->prev;
            tail->prev->next = tmp;
            tmp->next = tail;
            tail->prev = tmp;
        }

        device* get() {
            return head->next;
        }

        int size() {
            int i = 0;
            device *tmp;
            tmp = head->next;
            while (tmp != tail) {     //zähle bis tail erreicht wurde
                tmp = tmp->next;
                i++;
            }
            return i;
        }

        device* get(std::string mac) {
            device *tmp;
            tmp = head->next;
            while (tmp != tail) {
                if (tmp->mac.compare(mac) == 0) {
                    return tmp;
                }
                tmp = tmp->next;
            }
            return NULL;
        }
};

enum Modi {NORMAL, WATCHLIST};
Modi mode = NORMAL;

devicelist devices;
std::list<std::string> watchlist;

TFT_eSPI tft = TFT_eSPI(TFT_HEIGHT, TFT_WIDTH);
Button2 button_up = Button2(BUTTON_UP);
Button2 button_down = Button2(BUTTON_DOWN);

int selectedline = 0;
int scroll;
uint8_t level = 0, channel = 1;
static wifi_country_t wifi_country = {.cc="CN", .schan = 1, .nchan = 13}; //Most recent esp32 library struct

bool existsinwatchlist(std::string mac) {
    std::list<std::string>::iterator it;
    it = std::find(watchlist.begin(), watchlist.end(), mac);
    return (it != watchlist.end());
}

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

esp_err_t event_handler(void *ctx, system_event_t *event) {
    return ESP_OK;
}


const char * packettype2str(wifi_promiscuous_pkt_type_t type) {
    switch(type) {
        case WIFI_PKT_MGMT: return "MGMT";
        case WIFI_PKT_DATA: return "DATA";
    default:  
      case WIFI_PKT_MISC: return "MISC";
    }
}

static void packet_handler(void* buff, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT)
        return;

    const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buff;
    const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)ppkt->payload;
    const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;
    
    char buffer[18];
    sprintf(buffer, "%02x:%02x:%02x:%02x:%02x:%02x",
            hdr->addr2[0],hdr->addr2[1],hdr->addr2[2],
            hdr->addr2[3],hdr->addr2[4],hdr->addr2[5]);
    devices.insert(buffer, ppkt->rx_ctrl.rssi, millis());
}

void render(void * pvParameter) {
    int i = 0;                          //counts up after each line
    while (true) {
        tft.fillRect(0, 0, WIFI_CHANNEL_MAX * 2, 6, TFT_BLACK);     //overwrite area for new bar
        tft.fillRect(0, 0, channel * 2, 6, TFT_WHITE);              //loading bar driven by wifi channel switch
        tft.setCursor(30, 0);
        tft.printf("devices %d line %3d - %-3d", devices.size(), scroll + 1, selectedline + 1);

        switch (mode) {
            case NORMAL:  tft.println("   NORMAL"); break;
            case WATCHLIST: tft.println("WATCHLIST"); break;
        }
        device *tmp;
        tmp = devices.get();
        
        for (int j = 0; j < scroll; j++) {
            tmp = tmp->next;
        }

        while (tmp->next->next != NULL) {
            if (mode == NORMAL) {
                if (i == selectedline - scroll) {       //highlight selected line
                    tft.textbgcolor = TFT_WHITE;
                    tft.textcolor = TFT_BLACK;
                }
                tft.printf("%s rssi: %d\n", tmp->mac.c_str(), tmp->rssi);
                i++;
            } else if (mode == WATCHLIST) {
                if (existsinwatchlist(tmp->mac)) {
                    if (i == selectedline - scroll) {       //highlight selected line
                        tft.textbgcolor = TFT_WHITE;
                        tft.textcolor = TFT_BLACK;
                    }
                    tft.printf("%s rssi: %d\n", tmp->mac.c_str(), tmp->rssi);
                    i++;
                }
            }
            tft.textbgcolor = TFT_BLACK;
            tft.textcolor = TFT_WHITE;
            tmp = tmp->next;
        }
        tft.fillRect(0, tft.getCursorY(), TFT_WIDTH, TFT_HEIGHT - tft.getCursorY(), TFT_BLACK);
        i = 0;
        vTaskDelay(TFT_REFRESH / portTICK_RATE_MS);
    }
}


void channel_switcher(void * pvParameter) {
    while (true) {
        esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
        channel = (channel % WIFI_CHANNEL_MAX) + 1;

        vTaskDelay(WIFI_CHANNEL_SWITCH_INTERVAL / portTICK_RATE_MS);
    }
}

void buttonhandler(Button2& btn) {
    int button;
    int listsize;
    switch (mode) {
        case NORMAL: listsize = devices.size(); break;
        case WATCHLIST: listsize = watchlist.size(); break;
        default: return;
    }
    switch (btn.getAttachPin()) {
        case BUTTON_UP: button = 0; break;
        case BUTTON_DOWN: button = 1; break;
        default: return;
    }
    switch (btn.getClickType()) {
        case SINGLE_CLICK: 
            if (button) {   //DOWN
                if (selectedline < listsize - 1) {
                    selectedline++;
                    if (scroll + SCROLLOFF - 1 < selectedline) {
                        scroll++;
                    }
                } else {
                    selectedline = 0;
                    scroll = 0;
                }
            } else {        //UP
                if (selectedline != 0) {
                    selectedline--;
                    if (selectedline == scroll - 1) {
                        scroll--;
                    }
                } else {
                    selectedline = listsize - 1;
                    if (selectedline >= SCROLLOFF) {
                        scroll = listsize - SCROLLOFF;
                    }
                }
            }
            break;
        case LONG_CLICK:
            if (button) {   //DOWN
                scroll = 0;
                selectedline = 0;
                if (mode == NORMAL) { 
                    mode = WATCHLIST;
                } else {
                    mode = NORMAL;
                }
            } else {        //UP
                device *tmp;
                tmp = devices.get();
                for (int i = 0; i < selectedline; i++) {
                    tmp = tmp->next;
                }
                if (!existsinwatchlist(tmp->mac)) {
                    watchlist.push_back(tmp->mac);
                } else {
                    watchlist.remove(tmp->mac);
                }
            }
            break;
        default: return;
    }
}

void setup() {
    Serial.begin(115200);
    
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
    esp_wifi_set_promiscuous_rx_cb(&packet_handler);
    
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextWrap(false, false);

    button_up.setClickHandler(buttonhandler);
    button_up.setLongClickHandler(buttonhandler);
    button_down.setClickHandler(buttonhandler);
    button_down.setLongClickHandler(buttonhandler);
    button_up.setDoubleClickTime(100);
    button_down.setDoubleClickTime(100);
    button_up.setLongClickTime(500);
    button_down.setLongClickTime(500);
    
    
    xTaskCreate(&channel_switcher, "wifi channel switcher", 1024, NULL, 5, NULL);   //
    xTaskCreate(&render, "render", 2048, NULL, 5, NULL);
}

void loop() {
    button_up.loop();
    button_down.loop();
}
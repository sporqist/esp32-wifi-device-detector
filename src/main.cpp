#include <TFT_eSPI.h>
#include <Button2.h>
#include <Wire.h>
#include <FreeRTOS.h>
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#include <string>
#include <list>

#include "devicelist.h"
#include "ieee802_11_frames.h"

#define WIFI_CHANNEL_SWITCH_INTERVAL        200
#define TFT_REFRESH                         200

#define WIFI_CHANNEL_MAX                    13
#define BUTTON_UP                           35
#define BUTTON_DOWN                         0

#define LINE_HEIGHT                         8

#define TFT_WIDTH                           240
#define TFT_HEIGHT                          135

static const int normalmode_lines = 15;
static const int watchlistmode_lines = 15;

enum Modi {NORMAL, WATCHLIST};
Modi mode;

devicelist devices;
std::list<std::string> watchlist;

TFT_eSPI tft = TFT_eSPI(TFT_HEIGHT, TFT_WIDTH);
Button2 button_up = Button2(BUTTON_UP);
Button2 button_down = Button2(BUTTON_DOWN);

float pps;
int pps_buffer[10];
int devices_online;
uint8_t channel;
int selectedline;
int scroll; 

static wifi_country_t wifi_country = {.cc="CN", .schan = 1, .nchan = 13}; //Most recent esp32 library struct


esp_err_t event_handler(void *ctx, system_event_t *event) {
    return ESP_OK;
}

bool existsinwatchlist(std::string mac) {
    std::list<std::string>::iterator it;
    it = std::find(watchlist.begin(), watchlist.end(), mac);
    return (it != watchlist.end());
}

bool hl;
bool texthl() { return hl; }
void texthl(bool highlight) {
    if (highlight) {
        tft.textbgcolor = TFT_WHITE;
        tft.textcolor = TFT_BLACK;
        hl = true;
    } else {
        tft.textbgcolor = TFT_BLACK;
        tft.textcolor = TFT_WHITE;
        hl = false;
    }
}

void pps_counter(void * pvParameter) {
    TickType_t prevWakeTime;
    const TickType_t frequency = 1000;          //every second
    prevWakeTime = xTaskGetTickCount(); 
        
    float tmp = 0;
    while (true) {
        for (int i = 0; i < 9; i++) {
            tmp += pps_buffer[i];
            pps_buffer[i] = pps_buffer[i + 1];
        }
        tmp = tmp + pps_buffer[9];
        pps_buffer[9] = 0;
        pps = tmp / 10;
        tmp = 0;

        vTaskDelayUntil(&prevWakeTime, frequency);
    }
}

void online_counter(void * pvParameter) {
    TickType_t prevWakeTime;
    const TickType_t frequency = 1000;         //every second
    prevWakeTime = xTaskGetTickCount();

    int online_tmp = 0;
    while (true) {
        device* tmp;
        tmp = devices.get();
        while (!devices.isTail(tmp)) {
            if ((xTaskGetTickCount() - tmp->timestamp) / 1000 < 60) {
                online_tmp++;
            }
            tmp = tmp->next;
        }
        devices_online = online_tmp;
        online_tmp = 0;

        vTaskDelayUntil(&prevWakeTime, frequency);
    }    
}

void channel_switcher(void * pvParameter) {
    TickType_t prevWakeTime;
    const TickType_t frequency = WIFI_CHANNEL_SWITCH_INTERVAL;
    prevWakeTime = xTaskGetTickCount();
    
    while (true) {
        esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
        channel = (channel % WIFI_CHANNEL_MAX) + 1;

        vTaskDelayUntil(&prevWakeTime, frequency);
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
    devices.insert(buffer, ppkt->rx_ctrl.rssi, xTaskGetTickCount(), channel);
    pps_buffer[9]++;
}

void render(void * pvParameter) {
    TickType_t prevWakeTime;
    const TickType_t frequency = TFT_REFRESH;
    prevWakeTime = xTaskGetTickCount();
    
    int i = 0;                          //counts up after each line
    int scrolloff;
    device * tmp;
    device * selected;
    while (true) {
        tft.fillRect(0, 0, WIFI_CHANNEL_MAX * 2, 7, TFT_BLACK);     //overwrite area for new channel indicator
        tft.fillRect((channel - 1) * 2, 0, 2, 7, TFT_WHITE);        //channel indicator
        tft.setCursor(30, 0);
        tft.printf("devices %d line %3d - %-3d", devices.size(), scroll + 1, selectedline + 1);     //headline

        tmp = devices.get();

        for (int j = 0; j < scroll && !devices.isTail(tmp); j++) {      //skip devices until scroll point is reached
            tmp = tmp->next;
        }
        
        switch (mode) {
            case NORMAL:
                tft.println("  NORMAL");
                scrolloff = normalmode_lines;

                while(!devices.isTail(tmp) && i < scrolloff) {
                    if (i == selectedline - scroll) {           //highlight selected line
                        selected = tmp;
                        texthl(true);
                    }
                    if (existsinwatchlist(tmp->mac)) {
                        tft.print("|| ");
                    } else {
                        tft.print("   ");
                    }
                    if ((xTaskGetTickCount() - tmp->timestamp) / 1000 < 60) {
                        tft.printf("%s rssi:    %-4d", tmp->mac.c_str(), tmp->rssi);
                    } else {
                        tft.printf("%s offline: %d min", tmp->mac.c_str(), (int) tmp->timestamp / 1000 / 60 + 1);
                    }
                    //overwrite rest of the line so no remains of old frames are displayed
                    if (texthl()) {
                        tft.fillRect(tft.getCursorX(), tft.getCursorY(), TFT_WIDTH - tft.getCursorX(), LINE_HEIGHT, TFT_WHITE);
                    } else {
                        tft.fillRect(tft.getCursorX(), tft.getCursorY(), TFT_WIDTH - tft.getCursorX(), LINE_HEIGHT, TFT_BLACK);
                    }
                    tft.println();
                    texthl(false);
                    tmp = tmp->next;
                    i++;
                }
                break;
            case WATCHLIST:
                tft.println("WATCHLIST"); 
                scrolloff = watchlistmode_lines;

                while(!devices.isTail(tmp) && i < scrolloff) {
                    if (existsinwatchlist(tmp->mac)) {
                        if (i == selectedline - scroll) {       //highlight selected line
                            selected = tmp;
                            texthl(true);
                        }
                        if ((xTaskGetTickCount() - tmp->timestamp) / 1000 < 60) {
                            tft.printf("%s rssi:    %-3d", tmp->mac.c_str(), tmp->rssi);
                        } else {
                            tft.printf("%s offline: %d min", tmp->mac.c_str(), (int) tmp->timestamp / 1000 / 60 + 1);
                        }
                        //overwrite rest of the line so no remains of old frames are displayed
                        if (texthl()) {
                            tft.fillRect(tft.getCursorX(), tft.getCursorY(), TFT_WIDTH - tft.getCursorX(), LINE_HEIGHT, TFT_WHITE);
                        } else {
                            tft.fillRect(tft.getCursorX(), tft.getCursorY(), TFT_WIDTH - tft.getCursorX(), LINE_HEIGHT, TFT_BLACK);
                        }
                        tft.println();
                        texthl(false);
                        i++;
                    }                    
                    tmp = tmp->next;
                }
                while (i < watchlistmode_lines) {
                    tft.fillRect(0, tft.getCursorY(), TFT_WIDTH, LINE_HEIGHT, TFT_BLACK);
                    tft.println();
                    i++;
                }/*
                tft.printf("channel: %-2d", selected->channel);
                tft.fillRect(tft.getCursorX(), tft.getCursorY(), TFT_WIDTH - tft.getCursorX(), LINE_HEIGHT, TFT_BLACK);
                tft.println();
                i++;*/                

                break;
        }
        while (i < 15) {
            tft.fillRect(0, tft.getCursorY(), TFT_WIDTH, LINE_HEIGHT, TFT_BLACK);
            tft.println();
            i++;
        }
        tft.printf("online: %3d pkt/s: %5.1f", devices_online, pps);
        i = 0;

        vTaskDelayUntil(&prevWakeTime, frequency);
    }
}

void buttonhandler(Button2& btn) {
    int button;
    int listsize;
    int scrolloff;
    switch (mode) {
        case NORMAL: 
            listsize = devices.size(); 
            scrolloff = normalmode_lines;
            break;
        case WATCHLIST: 
            listsize = watchlist.size(); 
            scrolloff = watchlistmode_lines;
            break;
    }
    switch (btn.getAttachPin()) {
        case BUTTON_UP: button = 0; break;
        case BUTTON_DOWN: button = 1; break;
    }
    switch (btn.getClickType()) {
        case SINGLE_CLICK: 
            if (button) {   //DOWN
                if (selectedline < listsize - 1) {
                    selectedline++;
                    if (scroll + scrolloff - 1 < selectedline) {
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
                    if (selectedline >= scrolloff) {
                        scroll = listsize - scrolloff;
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
                if (mode == NORMAL) {
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
                } else if (mode == WATCHLIST) {
                }
                
            }
            break;
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

    mode = NORMAL;
    pps = 0;
    for (int i = 0; i < 10; i++) {
        pps_buffer[i] = 0;
    }
    devices_online = 0;
    channel = 1;
    selectedline = 0;
    scroll = 0;
    hl = false;

    xTaskCreate(&pps_counter, "pps_counter", 768, NULL, 6, NULL);
    xTaskCreate(&online_counter, "online_counter", 768, NULL, 6, NULL);
    xTaskCreate(&channel_switcher, "channel switcher", 1024, NULL, 5, NULL);
    xTaskCreate(&render, "render", 2048, NULL, 9, NULL);
}

void loop() {
    button_up.loop();
    button_down.loop();
    vTaskDelay(10 / portTICK_RATE_MS);
}
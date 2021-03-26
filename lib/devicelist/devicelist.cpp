#include <string>
#include "devicelist.h"

devicelist::devicelist() {
    head = new device;
    tail = new device;
    head->next = tail;
    tail->prev = head;
}

void devicelist::insert(std::string mac, int rssi, uint32_t timestamp, uint8_t channel) {
    device *pos = head->next;
    while (pos != tail) {
        if (pos->mac.compare(mac) == 0) {
            pos->rssi = rssi;
            pos->timestamp = timestamp;
            pos->channel = channel;
            return;
        }
        pos = pos->next;
    }
    device *tmp = new device;
    tmp->mac = mac;
    tmp->rssi = rssi;
    tmp->timestamp = timestamp;
    tmp->channel = channel;

    tmp->prev = tail->prev;
    tail->prev->next = tmp;
    tmp->next = tail;
    tail->prev = tmp;
}

int devicelist::size() {
    int i = 0;
    device *tmp;
    tmp = head->next;
    while (tmp != tail) {     //zähle bis tail erreicht wurde
        tmp = tmp->next;
        i++;
    }
    return i;
}

device* devicelist::get() {
    return head->next;
}

device* devicelist::get(std::string mac) {
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

bool devicelist::isTail(device * tmp) {
    return tmp == tail;
}
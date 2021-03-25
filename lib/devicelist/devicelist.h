#ifndef devicelist_h
#define devicelist_h


typedef struct device {
    std::string mac;
    int rssi;
    uint32_t timestamp;
    uint8_t channel;
    device *next;
    device *prev;
} device;

class devicelist {
    private:
        device *head, *tail;

    public:
        devicelist();
        
        void insert(std::string mac, int rssi, uint32_t timestamp, uint8_t channel);

        device* get();

        int size();

        device* get(std::string mac);
};

#endif
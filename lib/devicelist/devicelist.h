#ifndef devicelist_h
#define devicelist_h


typedef struct device {
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
        devicelist();
        
        void insert(std::string mac, int rssi, int timestamp);

        device* get();

        int size();

        device* get(std::string mac);
};

#endif
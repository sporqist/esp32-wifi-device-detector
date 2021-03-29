#ifndef ieee802_11_frames.h
#define ieee802_11_frames.h

typedef struct {
    wifi_ieee80211_mac_hdr_t hdr;
    uint8_t payload[0];             /* network data ended with 4 bytes csum (CRC32) */
} wifi_ieee80211_packet_t;

typedef struct {
    unsigned frame_ctrl:16;
    unsigned duration_id:16;
    uint8_t addr1[6];               /* receiver address */
    uint8_t addr2[6];               /* sender address */
    uint8_t addr3[6];               /* filtering address */
    unsigned sequence_ctrl:16;
    uint8_t addr4[6];               /* optional */
} wifi_ieee80211_mac_hdr_t;

#endif
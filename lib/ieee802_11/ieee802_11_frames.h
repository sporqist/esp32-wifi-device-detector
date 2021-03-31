#ifndef ieee802_11_frames_h
#define ieee802_11_frames_h

#define WIFI_MGMT_FIRST_DYNAMIC                     112

#define WIFI_MGMT_SUBTYPE_ASSOCIATION_REQUEST       0x00
#define WIFI_MGMT_SUBTYPE_ASSOCIATION_RESPONSE      0x01
#define WIFI_MGMT_SUBTYPE_REASSOCIATION_REQUEST     0x02
#define WIFI_MGMT_SUBTYPE_REASSOCIATION_RESPONSE    0x03
#define WIFI_MGMT_SUBTYPE_PROBE_REQUEST             0x04
#define WIFI_MGMT_SUBTYPE_PROBE_RESPONSE            0x05
#define WIFI_MGMT_SUBTYPE_BEACON                    0x08
#define WIFI_MGMT_SUBTYPE_ATIM                      0x09
#define WIFI_MGMT_SUBTYPE_DISASSOCIATION            0x0a
#define WIFI_MGMT_SUBTYPE_AUTHENTICATION            0x0b
#define WIFI_MGMT_SUBTYPE_DEAUTHENTICATION          0x0c


typedef struct {
    uint8_t element_id;
    uint8_t length;
    uint8_t payload[0];
} wifi_ieee80211_mgnt_information_element_t;

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

typedef struct {
    unsigned auth_algorythm:16;
    unsigned auth_transaction_sequence_number:16;
    unsigned beacon_interval:16;
    unsigned capability:16;
    uint8_t current_ap_addr[6];
    unsigned listen_interval:16;
    unsigned association_id:16;
    unsigned long long timestamp:64;
    unsigned reason_code:16;
    unsigned status_code:16;
    uint8_t payload[0];
} wifi_ieee80211_mgmt_packet_t;

#endif
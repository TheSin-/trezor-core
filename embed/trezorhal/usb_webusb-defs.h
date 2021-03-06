/*
 * Copyright (c) Jan Pochyla, SatoshiLabs
 *
 * Licensed under TREZOR License
 * see LICENSE file for details
 */

#define USB_WEBUSB_VENDOR_CODE          0x01  // arbitrary
#define USB_WEBUSB_LANDING_PAGE         0x01  // arbitrary

#define USE_WINUSB 1

#if USE_WINUSB
#define USB_WINUSB_VENDOR_CODE          '!'  // arbitrary, but must be equivalent to the last character in extra string
#define USB_WINUSB_EXTRA_STRING         'M', 0x00, 'S', 0x00, 'F', 0x00, 'T', 0x00, USB_WINUSB_VENDOR_CODE , 0x00, 0x00, 0x00  // MSFT100!
#define USB_WINUSB_EXTRA_STRING_INDEX   0xEE
#endif

typedef struct __attribute__((packed)) {
    usb_interface_descriptor_t iface;
    usb_endpoint_descriptor_t ep_in;
    usb_endpoint_descriptor_t ep_out;
} usb_webusb_descriptor_block_t;

/* usb_webusb_info_t contains all information for setting up a WebUSB interface.  All
 * passed pointers need to live at least until the interface is disabled
 * (usb_stop is called). */
typedef struct {
    uint8_t *rx_buffer;         // With length of max_packet_len bytes
    uint8_t iface_num;          // Address of this WebUSB interface
    uint8_t ep_in;              // Address of IN endpoint (with the highest bit set)
    uint8_t ep_out;             // Address of OUT endpoint
    uint8_t subclass;           // usb_iface_subclass_t
    uint8_t protocol;           // usb_iface_protocol_t
    uint8_t polling_interval;   // In units of 1ms
    uint8_t max_packet_len;     // Length of the biggest report and of rx_buffer
} usb_webusb_info_t;

/* usb_webusb_state_t encapsulates all state used by enabled WebUSB interface.  It
 * needs to be completely initialized in usb_webusb_add and reset in
 * usb_webusb_class_init.  See usb_webusb_info_t for details of the configuration
 * fields. */
typedef struct {
    const usb_webusb_descriptor_block_t *desc_block;
    uint8_t *rx_buffer;
    uint8_t ep_in;
    uint8_t ep_out;
    uint8_t max_packet_len;

    uint8_t alt_setting;   // For SET_INTERFACE/GET_INTERFACE setup reqs
    uint8_t last_read_len; // Length of data read into rx_buffer
    uint8_t ep_in_is_idle; // Set to 1 after IN endpoint gets idle
} usb_webusb_state_t;

secbool __wur usb_webusb_add(const usb_webusb_info_t *webusb_info);
secbool __wur usb_webusb_can_read(uint8_t iface_num);
secbool __wur usb_webusb_can_write(uint8_t iface_num);
int __wur usb_webusb_read(uint8_t iface_num, uint8_t *buf, uint32_t len);
int __wur usb_webusb_write(uint8_t iface_num, const uint8_t *buf, uint32_t len);

int __wur usb_webusb_read_select(uint32_t timeout);
int __wur usb_webusb_read_blocking(uint8_t iface_num, uint8_t *buf, uint32_t len, int timeout);
int __wur usb_webusb_write_blocking(uint8_t iface_num, const uint8_t *buf, uint32_t len, int timeout);

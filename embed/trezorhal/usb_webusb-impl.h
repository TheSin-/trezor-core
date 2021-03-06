/*
 * Copyright (c) Jan Pochyla, SatoshiLabs
 *
 * Licensed under TREZOR License
 * see LICENSE file for details
 */

#define USB_CLASS_WEBUSB                0xFF

#define USB_WEBUSB_REQ_SET_PROTOCOL     0x0B
#define USB_WEBUSB_REQ_GET_PROTOCOL     0x03
#define USB_WEBUSB_REQ_SET_IDLE         0x0A
#define USB_WEBUSB_REQ_GET_IDLE         0x02

#define USB_WEBUSB_REQ_GET_URL          0x02
#define USB_WEBUSB_DESCRIPTOR_TYPE_URL  0x03
#define USB_WEBUSB_URL_SCHEME_HTTP      0
#define USB_WEBUSB_URL_SCHEME_HTTPS     1

/* usb_webusb_add adds and configures new USB WebUSB interface according to
 * configuration options passed in `info`. */
secbool usb_webusb_add(const usb_webusb_info_t *info) {

    usb_iface_t *iface = usb_get_iface(info->iface_num);

    if (iface == NULL) {
        return secfalse; // Invalid interface number
    }
    if (iface->type != USB_IFACE_TYPE_DISABLED) {
        return secfalse; // Interface is already enabled
    }

    usb_webusb_descriptor_block_t *d = usb_desc_alloc_iface(sizeof(usb_webusb_descriptor_block_t));

    if (d == NULL) {
        return secfalse; // Not enough space in the configuration descriptor
    }

    if ((info->ep_in & USB_EP_DIR_MASK) != USB_EP_DIR_IN) {
        return secfalse; // IN EP is invalid
    }
    if ((info->ep_out & USB_EP_DIR_MASK) != USB_EP_DIR_OUT) {
        return secfalse; // OUT EP is invalid
    }
    if (info->rx_buffer == NULL) {
        return secfalse;
    }

    // Interface descriptor
    d->iface.bLength            = sizeof(usb_interface_descriptor_t);
    d->iface.bDescriptorType    = USB_DESC_TYPE_INTERFACE;
    d->iface.bInterfaceNumber   = info->iface_num;
    d->iface.bAlternateSetting  = 0;
    d->iface.bNumEndpoints      = 2;
    d->iface.bInterfaceClass    = USB_CLASS_WEBUSB;
    d->iface.bInterfaceSubClass = info->subclass;
    d->iface.bInterfaceProtocol = info->protocol;
    d->iface.iInterface         = USBD_IDX_INTERFACE_STR;

    // IN endpoint (sending)
    d->ep_in.bLength          = sizeof(usb_endpoint_descriptor_t);
    d->ep_in.bDescriptorType  = USB_DESC_TYPE_ENDPOINT;
    d->ep_in.bEndpointAddress = info->ep_in;
    d->ep_in.bmAttributes     = USBD_EP_TYPE_INTR;
    d->ep_in.wMaxPacketSize   = info->max_packet_len;
    d->ep_in.bInterval        = info->polling_interval;

    // OUT endpoint (receiving)
    d->ep_out.bLength          = sizeof(usb_endpoint_descriptor_t);
    d->ep_out.bDescriptorType  = USB_DESC_TYPE_ENDPOINT;
    d->ep_out.bEndpointAddress = info->ep_out;
    d->ep_out.bmAttributes     = USBD_EP_TYPE_INTR;
    d->ep_out.wMaxPacketSize   = info->max_packet_len;
    d->ep_out.bInterval        = info->polling_interval;

    // Config descriptor
    usb_desc_add_iface(sizeof(usb_webusb_descriptor_block_t));

    // Interface state
    iface->type = USB_IFACE_TYPE_WEBUSB;
    iface->webusb.desc_block      = d;
    iface->webusb.rx_buffer       = info->rx_buffer;
    iface->webusb.ep_in           = info->ep_in;
    iface->webusb.ep_out          = info->ep_out;
    iface->webusb.max_packet_len  = info->max_packet_len;
    iface->webusb.alt_setting     = 0;
    iface->webusb.last_read_len   = 0;
    iface->webusb.ep_in_is_idle   = 1;

    return sectrue;
}

secbool usb_webusb_can_read(uint8_t iface_num) {
    usb_iface_t *iface = usb_get_iface(iface_num);
    if (iface == NULL) {
        return secfalse; // Invalid interface number
    }
    if (iface->type != USB_IFACE_TYPE_WEBUSB) {
        return secfalse; // Invalid interface type
    }
    if (iface->webusb.last_read_len == 0) {
        return secfalse; // Nothing in the receiving buffer
    }
    if (usb_dev_handle.dev_state != USBD_STATE_CONFIGURED) {
        return secfalse; // Device is not configured
    }
    return sectrue;
}

secbool usb_webusb_can_write(uint8_t iface_num) {
    usb_iface_t *iface = usb_get_iface(iface_num);
    if (iface == NULL) {
        return secfalse; // Invalid interface number
    }
    if (iface->type != USB_IFACE_TYPE_WEBUSB) {
        return secfalse; // Invalid interface type
    }
    if (iface->webusb.ep_in_is_idle == 0) {
        return secfalse; // Last transmission is not over yet
    }
    if (usb_dev_handle.dev_state != USBD_STATE_CONFIGURED) {
        return secfalse; // Device is not configured
    }
    return sectrue;
}

int usb_webusb_read(uint8_t iface_num, uint8_t *buf, uint32_t len) {
    usb_iface_t *iface = usb_get_iface(iface_num);
    if (iface == NULL) {
        return -1; // Invalid interface number
    }
    if (iface->type != USB_IFACE_TYPE_WEBUSB) {
        return -2; // Invalid interface type
    }
    usb_webusb_state_t *state = &iface->webusb;

    // Copy maximum possible amount of data and truncate the buffer length
    if (len < state->last_read_len) {
        return 0; // Not enough data in the read buffer
    }
    len = state->last_read_len;
    state->last_read_len = 0;
    memcpy(buf, state->rx_buffer, len);

    // Clear NAK to indicate we are ready to read more data
    usb_ep_clear_nak(&usb_dev_handle, state->ep_out);

    return len;
}

int usb_webusb_write(uint8_t iface_num, const uint8_t *buf, uint32_t len) {
    usb_iface_t *iface = usb_get_iface(iface_num);
    if (iface == NULL) {
        return -1; // Invalid interface number
    }
    if (iface->type != USB_IFACE_TYPE_WEBUSB) {
        return -2; // Invalid interface type
    }
    usb_webusb_state_t *state = &iface->webusb;

    state->ep_in_is_idle = 0;
    USBD_LL_Transmit(&usb_dev_handle, state->ep_in, UNCONST(buf), (uint16_t)len);

    return len;
}

int usb_webusb_read_select(uint32_t timeout) {
    const uint32_t start = HAL_GetTick();
    for (;;) {
        for (int i = 0; i < USBD_MAX_NUM_INTERFACES; i++) {
            if (sectrue == usb_webusb_can_read(i)) {
                return i;
            }
        }
        if (HAL_GetTick() - start >= timeout) {
            break;
        }
        __WFI(); // Enter sleep mode, waiting for interrupt
    }
    return -1; // Timeout
}

int usb_webusb_read_blocking(uint8_t iface_num, uint8_t *buf, uint32_t len, int timeout) {
    const uint32_t start = HAL_GetTick();
    while (sectrue != usb_webusb_can_read(iface_num)) {
        if (timeout >= 0 && HAL_GetTick() - start >= timeout) {
            return 0; // Timeout
        }
        __WFI(); // Enter sleep mode, waiting for interrupt
    }
    return usb_webusb_read(iface_num, buf, len);
}

int usb_webusb_write_blocking(uint8_t iface_num, const uint8_t *buf, uint32_t len, int timeout) {
    const uint32_t start = HAL_GetTick();
    while (sectrue != usb_webusb_can_write(iface_num)) {
        if (timeout >= 0 && HAL_GetTick() - start >= timeout) {
            return 0; // Timeout
        }
        __WFI(); // Enter sleep mode, waiting for interrupt
    }
    return usb_webusb_write(iface_num, buf, len);
}

static void usb_webusb_class_init(USBD_HandleTypeDef *dev, usb_webusb_state_t *state, uint8_t cfg_idx) {
    // Open endpoints
    USBD_LL_OpenEP(dev, state->ep_in, USBD_EP_TYPE_INTR, state->max_packet_len);
    USBD_LL_OpenEP(dev, state->ep_out, USBD_EP_TYPE_INTR, state->max_packet_len);

    // Reset the state
    state->alt_setting = 0;
    state->last_read_len = 0;
    state->ep_in_is_idle = 1;

    // Prepare the OUT EP to receive next packet
    USBD_LL_PrepareReceive(dev, state->ep_out, state->rx_buffer, state->max_packet_len);
}

static void usb_webusb_class_deinit(USBD_HandleTypeDef *dev, usb_webusb_state_t *state, uint8_t cfg_idx) {
    // Flush endpoints
    USBD_LL_FlushEP(dev, state->ep_in);
    USBD_LL_FlushEP(dev, state->ep_out);
    // Close endpoints
    USBD_LL_CloseEP(dev, state->ep_in);
    USBD_LL_CloseEP(dev, state->ep_out);
}

static int usb_webusb_class_setup(USBD_HandleTypeDef *dev, usb_webusb_state_t *state, USBD_SetupReqTypedef *req) {

#if USE_WINUSB
    static uint8_t winusb_wcid[] = {
        // header
        0x28, 0x00, 0x00, 0x00, // dwLength
        0x00, 0x01,             // bcdVersion
        0x04, 0x00,             // wIndex
        0x01,                   // bNumSections
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // reserved
        // functions
        0x00,                   // bInterfaceNumber - will get overriden below
        0x01,                   // reserved
        'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00,       // compatibleId
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // subCompatibleId
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,             // reserved
    };
    static const uint8_t winusb_guid[] = {
        // header
        0x92, 0x00, 0x00, 0x00, // dwLength
        0x00, 0x01,             // bcdVersion
        0x05, 0x00,             // wIndex
        0x01, 0x00,             // wNumFeatures
        // features
        0x88, 0x00, 0x00, 0x00, // dwLength
        0x07, 0x00, 0x00, 0x00, // dwPropertyDataType
        0x2A, 0x00,             // wNameLength
        'D', 0x00, 'e', 0x00, 'v', 0x00, 'i', 0x00, 'c', 0x00, 'e', 0x00, 'I', 0x00, 'n', 0x00, 't', 0x00, 'e', 0x00, 'r', 0x00, 'f', 0x00, 'a', 0x00, 'c', 0x00, 'e', 0x00, 'G', 0x00, 'U', 0x00, 'I', 0x00, 'D', 0x00, 's', 0x00, 0x00, 0x00, // .name
        0x50, 0x00, 0x00, 0x00, // dwPropertyDataLength
        '{', 0x00, 'c', 0x00, '6', 0x00, 'c', 0x00, '3', 0x00, '7', 0x00, '4', 0x00, 'a', 0x00, '6', 0x00, '-', 0x00, '2', 0x00, '2', 0x00, '8', 0x00, '5', 0x00, '-', 0x00, '4', 0x00, 'c', 0x00, 'b', 0x00, '8', 0x00, '-', 0x00, 'a', 0x00, 'b', 0x00, '4', 0x00, '3', 0x00, '-', 0x00, '1', 0x00, '7', 0x00, '6', 0x00, '4', 0x00, '7', 0x00, 'c', 0x00, 'e', 0x00, 'a', 0x00, '5', 0x00, '0', 0x00, '3', 0x00, 'd', 0x00, '}', 0x00, 0x00, 0x00, 0x00, 0x00,  // propertyData
    };
#endif

    switch (req->bmRequest & USB_REQ_TYPE_MASK) {

#if USE_WINUSB
        case USB_REQ_TYPE_VENDOR:           // Vendor request
            switch (req->bRequest) {
                case USB_WINUSB_VENDOR_CODE:
                    switch (req->bmRequest & USB_REQ_RECIPIENT_MASK) {
                        case USB_REQ_RECIPIENT_DEVICE:
                            winusb_wcid[16] = state->desc_block->iface.bInterfaceNumber;
                            USBD_CtlSendData(dev, UNCONST(winusb_wcid), sizeof(winusb_wcid));
                            break;
                        case USB_REQ_RECIPIENT_INTERFACE:
                            USBD_CtlSendData(dev, UNCONST(winusb_guid), sizeof(winusb_guid));
                            break;
                    }
                    break;
                default:
                    USBD_CtlError(dev, req);
                    return USBD_FAIL;
            }
            break;
#endif

        case USB_REQ_TYPE_STANDARD:         // Interface & Endpoint request

            switch (req->bRequest) {
                case USB_REQ_SET_INTERFACE:
                    state->alt_setting = req->wValue;
                    break;
                case USB_REQ_GET_INTERFACE:
                    USBD_CtlSendData(dev, &state->alt_setting, sizeof(state->alt_setting));
                    break;
            }
            break;
    }

    return USBD_OK;
}

static void usb_webusb_class_data_in(USBD_HandleTypeDef *dev, usb_webusb_state_t *state, uint8_t ep_num) {
    if ((ep_num | USB_EP_DIR_IN) == state->ep_in) {
        state->ep_in_is_idle = 1;
    }
}

static void usb_webusb_class_data_out(USBD_HandleTypeDef *dev, usb_webusb_state_t *state, uint8_t ep_num) {
    if (ep_num == state->ep_out) {
        state->last_read_len = USBD_LL_GetRxDataSize(dev, ep_num);

        // Prepare the OUT EP to receive next packet
        // User should provide state->rx_buffer that is big enough for state->max_packet_len bytes
        USBD_LL_PrepareReceive(dev, ep_num, state->rx_buffer, state->max_packet_len);

        if (state->last_read_len > 0) {
            // Block the OUT EP until we process received data
            usb_ep_set_nak(dev, ep_num);
        }
    }
}

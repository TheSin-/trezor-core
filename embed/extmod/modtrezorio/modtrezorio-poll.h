/*
 * Copyright (c) Pavol Rusnak, SatoshiLabs
 *
 * Licensed under TREZOR License
 * see LICENSE file for details
 */

#include <string.h>
#include <unistd.h>

#include "usb.h"

#define TOUCH_IFACE (255)
#define POLL_READ  (0x0000)
#define POLL_WRITE (0x0100)

#define CHECK_PARAM_RANGE(value, minimum, maximum) \
    if (value < minimum || value > maximum) { \
        mp_raise_ValueError(#value " is out of range"); \
    }

/// def poll(ifaces: Iterable[int], list_ref: List, timeout_us: int) -> bool:
///     '''
///     Wait until one of `ifaces` is ready to read or write (using masks
//      `io.POLL_READ` and `io.POLL_WRITE`) and assign the result into
///     `list_ref`:
///
///     `list_ref[0]` - the interface number, including the mask
///     `list_ref[1]` - for touch event, tuple of (event_type, x_position, y_position)
///                   - for USB read event, received bytes
///
///     If timeout occurs, False is returned, True otherwise.
///     '''
STATIC mp_obj_t mod_trezorio_poll(mp_obj_t ifaces, mp_obj_t list_ref, mp_obj_t timeout_us) {
    mp_obj_list_t *ret = MP_OBJ_TO_PTR(list_ref);
    if (!MP_OBJ_IS_TYPE(list_ref, &mp_type_list) || ret->len < 2) {
        mp_raise_TypeError("invalid list_ref");
    }

    const mp_uint_t timeout = mp_obj_get_int(timeout_us);
    const mp_uint_t deadline = mp_hal_ticks_us() + timeout;
    mp_obj_iter_buf_t iterbuf;

    for (;;) {
        mp_obj_t iter = mp_getiter(ifaces, &iterbuf);
        mp_obj_t item;
        while ((item = mp_iternext(iter)) != MP_OBJ_STOP_ITERATION) {
            const mp_uint_t i = mp_obj_int_get_truncated(item);
            const mp_uint_t iface = i & 0x00FF;
            const mp_uint_t mode = i & 0xFF00;

            if (iface == TOUCH_IFACE) {
                const uint32_t evt = touch_read();
                if (evt) {
                    mp_obj_tuple_t *tuple = MP_OBJ_TO_PTR(mp_obj_new_tuple(3, NULL));
                    tuple->items[0] = MP_OBJ_NEW_SMALL_INT((evt >> 24) & 0xFFU); // event type
                    tuple->items[1] = MP_OBJ_NEW_SMALL_INT((evt >> 12) & 0xFFFU); // x position
                    tuple->items[2] = MP_OBJ_NEW_SMALL_INT(evt & 0xFFFU); // y position
                    ret->items[0] = MP_OBJ_NEW_SMALL_INT(i);
                    ret->items[1] = MP_OBJ_FROM_PTR(tuple);
                    return mp_const_true;
                }
            } else
            if (mode == POLL_READ) {
                if (sectrue == usb_hid_can_read(iface)) {
                    uint8_t buf[64];
                    int len = usb_hid_read(iface, buf, sizeof(buf));
                    if (len > 0) {
                        ret->items[0] = MP_OBJ_NEW_SMALL_INT(i);
                        ret->items[1] = mp_obj_new_bytes(buf, len);
                        return mp_const_true;
                    }
                } else if (sectrue == usb_webusb_can_read(iface)) {
                    uint8_t buf[64];
                    int len = usb_webusb_read(iface, buf, sizeof(buf));
                    if (len > 0) {
                        ret->items[0] = MP_OBJ_NEW_SMALL_INT(i);
                        ret->items[1] = mp_obj_new_bytes(buf, len);
                        return mp_const_true;
                    }
                }
            } else
            if (mode == POLL_WRITE) {
                if (sectrue == usb_hid_can_write(iface)) {
                    ret->items[0] = MP_OBJ_NEW_SMALL_INT(i);
                    ret->items[1] = mp_const_none;
                    return mp_const_true;
                } else if (sectrue == usb_webusb_can_write(iface)) {
                    ret->items[0] = MP_OBJ_NEW_SMALL_INT(i);
                    ret->items[1] = mp_const_none;
                    return mp_const_true;
                }
            }
        }

        if (mp_hal_ticks_us() >= deadline) {
            break;
        } else {
            MICROPY_EVENT_POLL_HOOK
        }
    }

    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(mod_trezorio_poll_obj, mod_trezorio_poll);

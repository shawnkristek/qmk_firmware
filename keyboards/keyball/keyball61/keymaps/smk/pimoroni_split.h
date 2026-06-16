// Custom split transaction for sending the left-half Pimoroni trackball's
// scroll/click data across the TRRS cable to the master half.
//
// In normal use the USB cable is on the RIGHT half, so the right is master and
// the LEFT (Pimoroni) half is the slave. The keyball core only syncs its own
// PMW3360 motion over the split, so the Pimoroni data computed on the slave
// would otherwise never reach the host. This RPC closes that gap: the master
// pulls the slave's accumulated scroll + click each housekeeping cycle.
#pragma once

#include <stdint.h>

// NOTE: The transaction id PIMORONI_GET_SCROLL is declared via
// `#define SPLIT_TRANSACTION_IDS_USER PIMORONI_GET_SCROLL` in config.h. QMK
// expands that into its own transaction enum (transaction_id_define.h), so it
// must NOT be defined again here or it collides.

// Payload returned by the slave to the master.
typedef struct __attribute__((packed)) {
    int16_t h;      // horizontal scroll delta (already divided/tuned)
    int16_t v;      // vertical scroll delta (already divided/tuned)
    uint8_t click;  // nonzero while the ball is pressed
} pimoroni_scroll_t;

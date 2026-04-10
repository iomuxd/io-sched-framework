#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "daemon/device_registry.h"

/* Maximum capacity: dev_id is uint8_t, so 256 slots at most. */
#define DEV_REG_MAX_CAP 256

/* Allocate entries and active arrays for up to cap devices. */
int dev_reg_init(dev_registry_t *reg, size_t cap) {
    if (reg == NULL || cap == 0 || cap > DEV_REG_MAX_CAP)
        return -1;

    reg->entries = calloc(cap, sizeof(dev_entry_t));
    if (reg->entries == NULL)
        return -1;

    reg->active = calloc(cap, sizeof(uint8_t));
    if (reg->active == NULL) {
        free(reg->entries);
        return -1;
    }

    reg->cap = cap;
    return 0;
}

/* Free all registry resources and reset state. */
void dev_reg_destroy(dev_registry_t *reg) {
    if (reg == NULL)
        return;
    free(reg->entries);
    free(reg->active);
    reg->entries = NULL;
    reg->active  = NULL;
    reg->cap     = 0;
}

/* Register a new device with an initial duration estimate.
 * Rejects out-of-range or duplicate dev_id. */
int dev_reg_add(dev_registry_t *reg, uint8_t dev_id,
                const char *name, uint32_t init_us) {
    if (reg == NULL || name == NULL)
        return -1;
    if (dev_id >= reg->cap)
        return -1;
    if (reg->active[dev_id])
        return -1;

    dev_entry_t *e = &reg->entries[dev_id];
    e->dev_id     = dev_id;
    e->est_us     = init_us;
    e->init_us    = init_us;
    e->sample_cnt = 0;

    /* Ensure null-termination even when name exceeds buffer */
    strncpy(e->name, name, DEV_NAME_MAX - 1);
    e->name[DEV_NAME_MAX - 1] = '\0';

    reg->active[dev_id] = 1;
    return 0;
}

/* Look up a device entry by ID. Returns NULL if unregistered. */
dev_entry_t *dev_reg_lookup(const dev_registry_t *reg, uint8_t dev_id) {
    if (reg == NULL)
        return NULL;
    if (dev_id >= reg->cap)
        return NULL;
    if (!reg->active[dev_id])
        return NULL;

    return &reg->entries[dev_id];
}

/* Update the EMA-smoothed duration estimate after a completed I/O.
 * Uses alpha = 1/8: new = (actual + 7 * old) / 8.
 * Widened to uint64_t to prevent overflow in the intermediate sum. */
void dev_reg_update(dev_registry_t *reg, uint8_t dev_id, uint32_t actual_us) {
    dev_entry_t *entry = dev_reg_lookup(reg, dev_id);
    if (entry == NULL)
        return;

    entry->est_us = ((uint64_t)actual_us + 7ULL * entry->est_us) >> 3;
    if (entry->sample_cnt < UINT32_MAX)
        entry->sample_cnt++;
}

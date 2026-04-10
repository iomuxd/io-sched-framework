#ifndef DEVICE_REGISTRY_H
#define DEVICE_REGISTRY_H

#include <stddef.h>
#include <stdint.h>

#define DEV_NAME_MAX 16

/** Per-device metadata with EMA-smoothed duration estimate */
typedef struct {
    uint8_t  dev_id;
    char     name[DEV_NAME_MAX];
    uint32_t est_us;       /* EMA-smoothed expected duration (μs) */
    uint32_t init_us;      /* initial estimate supplied at registration */
    uint32_t sample_cnt;   /* number of EMA updates applied */
} dev_entry_t;

/** Device registry — fixed-capacity array indexed by dev_id */
typedef struct {
    dev_entry_t *entries;  /* heap-allocated array of cap elements */
    uint8_t     *active;   /* bitmap: 1 = slot registered, 0 = empty */
    size_t       cap;
} dev_registry_t;

/* Lifecycle */
int  dev_reg_init(dev_registry_t *reg, size_t cap);
void dev_reg_destroy(dev_registry_t *reg);

/* Registration */
int  dev_reg_add(dev_registry_t *reg, uint8_t dev_id,
                 const char *name, uint32_t init_us);

/* Lookup — returns NULL if dev_id is unregistered */
dev_entry_t *dev_reg_lookup(const dev_registry_t *reg, uint8_t dev_id);

/* Update duration estimate with a new observation (EMA) */
void dev_reg_update(dev_registry_t *reg, uint8_t dev_id, uint32_t actual_us);

#endif /* DEVICE_REGISTRY_H */

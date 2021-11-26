
#ifndef __NONIN_H__
#define __NONIN_H__

/* Oximetry Service - Measurement Characteristic */
#define NON3230_OXIMETRY_MEASUREMENT_CHAR_HND 24
#define NON3230_OXIMETRY_MEASUREMENT_CCCD_HND 25

/* Oximetry Service - Control Point Characteristic */
#define NON3230_OXIMETRY_CTRLPNT_CHAR_HND 27
#define NON3230_OXIMETRY_CTRLPNT_CCCD_HND 28

#define DEBOUNCE_SECS 5

typedef struct nonin_context_ {
    btsm_device_t *device;
    btsm_process_data_fn process_fn;
} nonin_context_t;

nonin_context_t nctx_g;

struct nonin3230_oximetry_data_ {
    char length;
    char status;
    char battery_voltage;
    short pulse_amplitude_index;
    short counter;
    char spo2;
    short pulse_rate;
} __attribute__((packed));

typedef struct nonin3230_oximetry_data_ nonin3230_oximetry_data_t;

#endif /* __NONIN_H__ */

#ifndef DIRECTOR_H
#define DIRECTOR_H

#include "qpc.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Public AO pointer (same pattern as your other AOs)
extern QActive *const AO_DIRECTOR;

// Hypothetical event published by your CAN AO.
// Adjust units to match your CAN messages once defined.
typedef struct
{
    QEvt super;
    uint16_t rpm;           // engine RPM
    int16_t temp_c_x10;     // temperature in 0.1C
    uint16_t press_kpa_x10; // coolant pressure in 0.1 kPa (example)
} EngineDataEvt;

// Constructor
void Director_ctor(void);

#ifdef __cplusplus
}
#endif

#endif // DIRECTOR_H
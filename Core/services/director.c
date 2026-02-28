#include "director.h"
#include "bsp.h"                   // for Q_ASSERT, etc.
#include "main.h"                  // CubeMX handles live in main.c (or corresponding headers)
#include "private_signal_ranges.h" // for PRIVATE_SIGNAL_DIRECTOR_START (you’ll add)
#include "stm32g4xx_hal.h"

#include <string.h>

Q_DEFINE_THIS_MODULE("Director");

// ------------------------- Configuration knobs -------------------------
// DAC is 12-bit for most gauge applications; update if you use different.
#define DAC12_MAX_CODE (4095U)

// DAC3 offset (sets the non-inverting reference of the two inverting opamps).
// TODO: choose the right default code for your analog front end.
#define DIRECTOR_DAC3_OFFSET_CODE (2048U) // midscale placeholder

// These are placeholder “engineering->needle” mappings.
// You’ll tune these once you know gauge transfer functions & scaling.
#define PRESS_MIN_KPA_X10 (0U)
#define PRESS_MAX_KPA_X10 (5000U) // 500.0 kPa example

#define TEMP_MIN_C_X10 (-200) // -20.0C example
#define TEMP_MAX_C_X10 (1200) // 120.0C example

// For RPM->PFM: pick a mapping. Example: 0..6000 RPM maps to 0..200 Hz.
// TODO: update to match your gauge movement’s expected frequency range.
#define RPM_MAX        (6000U)
#define RPM_PFM_MAX_HZ (200U)

// ----------------------------------------------------------------------

/**************************************************************************************************\
* Private type definitions
\**************************************************************************************************/
enum DIRECTOR_Signals
{
    // Private signals (timeouts, internal sequencing) start here if needed later
    DIRECTOR_DUMMY_SIG = PRIVATE_SIGNAL_DIRECTOR_START,
};

typedef struct
{
    QActive super; // inherit QActive
    // Add QTimeEvt(s) later if you want periodic smoothing/filters.
} Director;

/**************************************************************************************************\
* Private memory declarations
\**************************************************************************************************/
static Director director_inst;
QActive *const AO_DIRECTOR = &director_inst.super;

/**************************************************************************************************\
* Private prototypes
\**************************************************************************************************/
static QState initial(Director *const me, void const *const par);
static QState idle(Director *const me, QEvt const *const e);
static QState active(Director *const me, QEvt const *const e);

// Hardware helpers
static uint16_t clamp_u16(uint32_t x, uint32_t lo, uint32_t hi);
static uint16_t map_u16_to_dac12(uint32_t x, uint32_t x_min, uint32_t x_max);
static uint16_t map_s16_to_dac12(int32_t x, int32_t x_min, int32_t x_max);

static uint32_t tim8_get_clk_hz(void);
static void tim8_set_pwm_freq_hz(uint32_t freq_hz);
static uint32_t rpm_to_hz(uint16_t rpm);

/**************************************************************************************************\
* CubeMX-generated handles (defined in main.c)
\**************************************************************************************************/
extern DAC_HandleTypeDef hdac1;
extern DAC_HandleTypeDef hdac3;
extern OPAMP_HandleTypeDef hopamp1;
extern TIM_HandleTypeDef htim8;

/**************************************************************************************************\
* Public functions
\**************************************************************************************************/
void Director_ctor(void)
{
    Director *const me = &director_inst;
    QActive_ctor(&me->super, Q_STATE_CAST(&initial));
}

/**************************************************************************************************\
* HSM
\**************************************************************************************************/
static QState initial(Director *const me, void const *const par)
{
    Q_UNUSED_PAR(par);

    // Subscribe to the engine-data signal published by CAN AO.
    QActive_subscribe((QActive *) me, PUBSUB_ENGINE_DATA_SIG);

    return Q_TRAN(&idle);
}

static QState idle(Director *const me, QEvt const *const e)
{
    QState status;

    switch (e->sig)
    {
        case Q_ENTRY_SIG: {
            // Initialize needles to “zero”
            BSP_Gauge_SetPressure_V(0);
            BSP_Gauge_SetTemperature_V(0);
            BSP_Gauge_SetOpAmpRef_V(1.78);

            // Start at 0 Hz
            BSP_RpmGauge_SetPFM_Hz(0U);

            status = Q_HANDLED();
            break;
        }

        // Once CAN is online / you get first valid data, you might transition to active.
        // For now we can just go active immediately:
        case Q_INIT_SIG: {
            status = Q_HANDLED();
            break;
        }

        default: {
            status = Q_SUPER(&QHsm_top);
            break;
        }
    }

    return status;
}

static QState active(Director *const me, QEvt const *const e)
{
    QState status;

    switch (e->sig)
    {
        case Q_ENTRY_SIG: {
            status = Q_HANDLED();
            break;
        }
        case PUBSUB_ENGINE_DATA_SIG: { // TODO

            status = Q_HANDLED();
            break;
        }

        default: {
            status = Q_SUPER(&QHsm_top);
            break;
        }
    }

    return status;
}

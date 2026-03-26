#include "director.h"
#include "bsp.h"
#include "private_signal_ranges.h"

#include <stddef.h>

Q_DEFINE_THIS_MODULE("Director");

typedef struct
{
    int16_t input_x10;
    float output_volts;
} GaugeMapPoint_T;

// Placeholder tables for gauge calibration.
// Inputs are engineering units in x10; outputs are DAC volts after analog front-end tuning.
static const GaugeMapPoint_T s_temp_gauge_map[] = {
    {-200, 0.20f},
    {0, 0.40f},
    {200, 0.75f},
    {400, 1.15f},
    {600, 1.55f},
    {800, 1.95f},
    {1000, 2.30f},
    {1200, 2.65f},
};

static const GaugeMapPoint_T s_pressure_gauge_map[] = {
    {0, 0.20f},
    {100, 0.45f},
    {250, 0.85f},
    {400, 1.30f},
    {550, 1.75f},
    {700, 2.10f},
    {850, 2.40f},
    {1000, 2.70f},
};

/**************************************************************************************************\
* Private type definitions
\**************************************************************************************************/
enum DIRECTOR_Signals
{
    DIRECTOR_DUMMY_SIG = PRIVATE_SIGNAL_DIRECTOR_START,
    POLL_TIMEOUT_SIG,
};

typedef struct
{
    QActive super;
    QTimeEvt timeEvt;
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
static QState top(Director *const me, QEvt const *const e);
static float lookup_gauge_voltage(
    const GaugeMapPoint_T *map, size_t map_len, int16_t input_x10);
static float map_temperature_to_voltage(int16_t temp_x10);
static float map_pressure_to_voltage(int16_t pressure_x10);

/**************************************************************************************************\
* Public functions
\**************************************************************************************************/
void Director_ctor(void)
{
    Director *const me = &director_inst;
    QActive_ctor(&me->super, Q_STATE_CAST(&initial));

    QTimeEvt_ctorX(&me->timeEvt, &me->super, POLL_TIMEOUT_SIG, 0U);
}

/**************************************************************************************************\
* HSM
\**************************************************************************************************/
static QState initial(Director *const me, void const *const par)
{
    Q_UNUSED_PAR(par);

    QActive_subscribe((QActive *) me, PUBSUB_MOTOR_DATA_SIG);

    QTimeEvt_armX(&me->timeEvt, BSP_TICKS_PER_SEC / 10U, BSP_TICKS_PER_SEC / 10U);

    return Q_TRAN(&top);
}

static QState top(Director *const me, QEvt const *const e)
{
    QState status;

    switch (e->sig)
    {
        case Q_ENTRY_SIG: {
            BSP_Gauge_SetPressure_V(map_pressure_to_voltage(0));
            BSP_Gauge_SetTemperature_V(map_temperature_to_voltage(0));
            BSP_Gauge_SetOpAmpRef_V(1.78f);

            BSP_RpmGauge_SetPFM_RPM(0U);

            BSP_Set_Backlight(false);

            QEvt *evt = Q_NEW(QEvt, PUBSUB_BOX_TO_BOX_STARTUP_SIG);
            QACTIVE_PUBLISH(evt, &me->super);

            status = Q_HANDLED();
            break;
        }

        case PUBSUB_MOTOR_DATA_SIG: {
            const MotorDataEvent_T *evt = Q_EVT_CAST(MotorDataEvent_T);

            BSP_Gauge_SetPressure_V(map_pressure_to_voltage(evt->pressure));
            BSP_Gauge_SetTemperature_V(map_temperature_to_voltage(evt->temperature));
            BSP_RpmGauge_SetPFM_RPM(evt->tachometer);

            status = Q_HANDLED();
            break;
        }

        case POLL_TIMEOUT_SIG: {
            BSP_Set_Backlight(BSP_Get_Backlight());

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

static float lookup_gauge_voltage(
    const GaugeMapPoint_T *map, size_t map_len, int16_t input_x10)
{
    Q_ASSERT(map != NULL);
    Q_ASSERT(map_len > 0U);

    if (input_x10 <= map[0].input_x10)
    {
        return map[0].output_volts;
    }

    for (size_t i = 1U; i < map_len; ++i)
    {
        if (input_x10 <= map[i].input_x10)
        {
            const int16_t x0 = map[i - 1U].input_x10;
            const int16_t x1 = map[i].input_x10;
            const float y0   = map[i - 1U].output_volts;
            const float y1   = map[i].output_volts;
            const float span = (float) (x1 - x0);

            if (span <= 0.0f)
            {
                return y1;
            }

            const float position = ((float) input_x10 - (float) x0) / span;
            return y0 + (position * (y1 - y0));
        }
    }

    return map[map_len - 1U].output_volts;
}

static float map_temperature_to_voltage(int16_t temp_x10)
{
    return lookup_gauge_voltage(s_temp_gauge_map, Q_DIM(s_temp_gauge_map), temp_x10);
}

static float map_pressure_to_voltage(int16_t pressure_x10)
{
    return lookup_gauge_voltage(s_pressure_gauge_map, Q_DIM(s_pressure_gauge_map), pressure_x10);
}

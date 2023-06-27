#ifndef __SETTINGS_H__
#define __SETTINGS_H__
#include "esphome.h"

namespace settings
{
    enum actionEnum
    {
        HEAT = 1,
        COOL = 0
    };

    enum modeEnum
    {
        HEAT_ONLY = 1,
        COLD_ONLY = 0,
        BOTH = 2
    };
    enum hpErrorEnum
    {
        NONE = 0x0,
        PD = 0x04
    };
    typedef struct
    {
        // ============================
        uint8_t targetTemp;
        uint8_t defrostAutoEnableTime;
        uint8_t defrostEnableTemp;
        uint8_t defrostDisableTemp;
        uint8_t defrostMaxDuration;
        uint8_t restartOffsetTemp;
        uint8_t compressorStopMarginTemp;
        uint8_t thermalProtection;
        uint8_t maximumTemp;
        uint8_t stopWhenReachedDelay;
        // =============MODE==========
        bool specialCtrlMode;
        bool on;
        actionEnum action;
        bool autoRestart;
        modeEnum opMode;
    } ctrlSettings;

    typedef struct
    {
        // ============================
        uint8_t targetTemp;
        uint8_t defrostAutoEnableTime;
        uint8_t defrostEnableTemp;
        uint8_t defrostDisableTemp;
        uint8_t defrostMaxDuration;
        uint8_t restartOffsetTemp;
        uint8_t compressorStopMarginTemp;
        uint8_t thermalProtection;
        uint8_t maximumTemp;
        uint8_t stopWhenReachedDelay;
        // =============MODE==========
        bool specialCtrlMode;
        bool on;
        actionEnum action;
        bool autoRestart;
        modeEnum opMode;

        // =============DATA==========

        uint8_t waterTempIn;
        uint8_t waterTempOut;
        uint8_t coilTemp;
        uint8_t airOutletTemp;
        uint8_t outdoorAirTemp;
        hpErrorEnum errorCode;
        uint8_t timeSinceFan;
        uint8_t timeSincePump;
    } hpInfo;
}
#endif // __SETTINGS_H__
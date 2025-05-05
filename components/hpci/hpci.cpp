#include "hpci.h"

namespace esphome
{
    namespace hpci
    {
        uint8_t lastDataType;
        volatile bool data_to_send = false;

        void HeatPumpController::setup()
        {
            lastDataType = 0;
            this->high_freq_.start();

            settings::ctrlSettings defaultSettings = {
                29,                  // uint8_t targetTemp;
                40,                  // uint8_t defrostAutoEnableTime;
                7,                   // uint8_t defrostEnableTemp;
                13,                  // uint8_t defrostDisableTemp;
                8,                   // uint8_t defrostMaxDuration;
                2,                   // uint8_t restartOffsetTemp;
                0,                   // uint8_t compressorStopMarginTemp;
                118,                 // uint8_t thermalProtection;
                40,                  // uint8_t maximumTemp;
                15,                  // uint8_t stopWhenReachedDelay;
                true,                // bool specialCtrlMode;
                false,               // bool on;
                settings::HEAT,      // actionEnum action;
                true,                // bool autoRestart;
                settings::HEAT_ONLY, // modeEnum opMode;
            };
            this->hpSettings = defaultSettings;
            swi::swi_setup();
            ESP_LOGI("HPCI", "Successful setup!");
        }

        void HeatPumpController::loop()
        {
            swi::swi_loop();

            if (swi::frame_available)
            {
                swi::frame_available = false;
                if (this->frameIsValid(swi::read_frame, swi::frameCnt))
                {
                    ESP_LOGI("HPCI", "Frame Data (%s):", lastDataType == 0xD2 ? "Control" : "Status");
                    this->decode(swi::read_frame);
                    if (lastDataType == 0xD2)
                    {
                        ESP_LOGD("HPCI", "PAC %s, target: %d", (this->hpData.on ? "ON" : "OFF"), this->hpData.targetTemp);
                        ESP_LOGD("HPCI", "Defrost Auto Enable Time %d, Defrost Enable Temp: %d", this->hpData.defrostAutoEnableTime, this->hpData.defrostEnableTemp);
                        ESP_LOGD("HPCI", "Defrost Disable Temp %d, Defrost Max Duration: %d", this->hpData.defrostDisableTemp, this->hpData.defrostMaxDuration);
                    }
                    else if (lastDataType == 0xDD)
                    {
                        ESP_LOGD("HPCI", "Water temp IN %d, Water temp OUT: %d", this->hpData.waterTempIn, this->hpData.waterTempOut);
                        ESP_LOGD("HPCI", "Air Outlet Temp: %d, Outdoor Air Temp: %d", this->hpData.airOutletTemp, this->hpData.outdoorAirTemp);
                        ESP_LOGD("HPCI", "Error Code %d, Time Since Fan: %d", this->hpData.errorCode, this->hpData.timeSinceFan);
                        ESP_LOGD("HPCI", "Time Since Pump %d", this->hpData.timeSincePump);
                    }
                }
                else
                {
                    ESP_LOGW("HPCI", "Invalid or corrupt frame");
                }
            }
            if (this->data_to_send)
            {
                if (this->sendControl(this->hpSettings))
                {
                    this->data_to_send = false;
                }
                // Wait for the data to be sent. Data will be sent when transmission is available.
            }
        }

        bool HeatPumpController::sendControl(settings::ctrlSettings settings)
        {
            uint8_t frame[HP_FRAME_LEN];
            frame[0] = 0xCC; // header
            frame[1] = 0x0C; // header
            frame[2] = settings.targetTemp;
            frame[3] = settings.defrostAutoEnableTime;
            frame[4] = settings.defrostEnableTemp;
            frame[5] = settings.defrostDisableTemp;
            frame[6] = settings.defrostMaxDuration * 20;
            // MODE 1
            frame[7] = 0x00; // Clear any current flag
            frame[7] |= (settings.specialCtrlMode ? 0x80 : 0x00);
            frame[7] |= (settings.on ? 0x40 : 0x00);
            frame[7] |= ((settings.action == settings::HEAT) ? 0x20 : 0x00);
            frame[7] |= (settings.autoRestart ? 0x08 : 0x00);
            frame[7] |= ((int)settings.opMode & 0x03) << 1;
            frame[8] = 0x1D; // MODE 2 (NOT A HYBRID PUMP)
            frame[9] = settings.restartOffsetTemp;
            frame[10] = settings.compressorStopMarginTemp;
            frame[11] = settings.thermalProtection;
            frame[12] = settings.maximumTemp;
            frame[13] = settings.stopWhenReachedDelay;
            frame[14] = 0x00; // SCHEDULE SETTING OFF
            frame[15] = this->computeChecksum(frame, HP_FRAME_LEN);
            return swi::sendFrame(frame, HP_FRAME_LEN);
        }

        bool HeatPumpController::decode(uint8_t frame[])
        {
            if (frame[0] == 0xD2)
            {
                this->hpData.targetTemp = frame[2];
                this->hpData.defrostAutoEnableTime = frame[3];
                this->hpData.defrostEnableTemp = frame[4];
                this->hpData.defrostDisableTemp = frame[5];
                this->hpData.defrostMaxDuration = frame[6] / 20;
                this->hpData.on = frame[7] & 0x40;
                this->hpData.autoRestart = frame[7] & 0x08;
                this->hpData.opMode = static_cast<settings::modeEnum>(frame[7] & 0x6);
                this->hpData.action = static_cast<settings::actionEnum>(frame[7] & 0x20);
                this->hpData.specialCtrlMode = frame[7] & 0x80;
                this->hpData.restartOffsetTemp = frame[9];
                this->hpData.compressorStopMarginTemp = frame[10];
                this->hpData.thermalProtection = frame[11];
                this->hpData.maximumTemp = frame[12];
                this->hpData.stopWhenReachedDelay = frame[13];
                // this->hpData.targetTemp = frame[14];
                lastDataType = 0xD2;
                return true;
            }
            else if (frame[0] == 0xDD)
            {
                this->hpData.waterTempIn = frame[1];
                this->hpData.waterTempOut = frame[2];
                this->hpData.coilTemp = frame[3];
                this->hpData.airOutletTemp = frame[4];
                this->hpData.outdoorAirTemp = frame[5];
                this->hpData.errorCode = static_cast<settings::hpErrorEnum>(frame[7]);
                this->hpData.timeSinceFan = frame[10];
                this->hpData.timeSincePump = frame[11];
                this->hpData.maximumTemp = frame[12];
                this->hpData.stopWhenReachedDelay = frame[13];
                lastDataType = 0xDD;
                return true;
            }
            ESP_LOGW("HPCI", "UNKNOWN MESSAGE !");
            return false;
        }

        void HeatPumpController::setOn(bool value)
        {
            this->hpSettings.on = value;
            this->data_to_send = true;
        }
        void HeatPumpController::setTargetTemp(uint16_t value)
        {
            this->hpSettings.targetTemp = value;
            this->data_to_send = true;
        }
        float HeatPumpController::getTargetTemp()
        {
            return (float)this->hpData.targetTemp;
        }
        float HeatPumpController::getWaterInTemp()
        {

            return (float)this->hpData.waterTempIn;
        }

        float HeatPumpController::getWaterOutTemp()
        {
            return this->hpData.waterTempOut;
        }

        float HeatPumpController::getOutdoorTemp()
        {
            return (float)this->hpData.outdoorAirTemp;
        }

        bool HeatPumpController::getOn()
        {
            return this->hpData.on;
        }

        bool HeatPumpController::getRunning()
        {
            return (this->hpData.timeSinceFan > 0);
        }

        uint16_t HeatPumpController::getErrorCode()
        {
            return this->hpData.errorCode;
        }

        bool HeatPumpController::frameIsValid(uint8_t frame[], uint8_t size)
        {
            if (size == HP_FRAME_LEN)
            {
                unsigned char computed_checksum = this->computeChecksum(frame, size);
                unsigned char checksum = frame[size - 1];
                ESP_LOGD("HPCI", "Checksum: %d, Computed: %d", checksum, computed_checksum);
                return computed_checksum == checksum;
            }
            else
            {
                ESP_LOGW("HPCI", "Frame size is not valid (%d)", size);
                return false;
            }
            return false;
        }

        uint8_t HeatPumpController::computeChecksum(uint8_t frame[], uint8_t size)
        {
            unsigned int total = 0;
            for (byte i = 1; i < size - 1; i++)
            {
                total += frame[i];
            }
            byte checksum = total % 256;
            return checksum;
        }

    }
}
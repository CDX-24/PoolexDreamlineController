#ifndef __HPCI_H__
#define __HPCI_H__
#include "settings.h"
#include "esphome.h"
#include "esphome/core/component.h"
#include "swi.h"
namespace esphome
{
  namespace hpci
  {
#define HP_FRAME_LEN 16 // Heat pump frame length

    class HeatPumpController : public Component
    {
    public:
      void setup() override;
      volatile bool data_to_send;

      void loop() override;
      void setTargetTemp(uint16_t value);
      void setOn(bool value);
      bool sendControl(settings::ctrlSettings settings);
      float getTargetTemp();
      float getWaterInTemp();
      float getWaterOutTemp();
      float getOutdoorTemp();
      bool getOn();
      bool getRunning();
      uint16_t getErrorCode();

    protected:
      settings::hpInfo hpData;
      settings::ctrlSettings hpSettings;
      HighFrequencyLoopRequester high_freq_;
      uint8_t computeChecksum(uint8_t frame[], uint8_t size);
      bool frameIsValid(uint8_t frame[], uint8_t size);

      bool decode(uint8_t frame[]);
    };

  }
}

#endif // __HPCI_H__
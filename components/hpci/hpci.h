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
      float get_setup_priority() const override { return setup_priority::BUS; }

      void loop() override;
      void setTargetTemp(uint16_t value);
      void setOn(bool value);
      float getTargetTemp();
      float getWaterInTemp();
      float getWaterOutTemp();
      float getOutdoorTemp();
      bool getOn();
      bool getRunning();
      uint16_t getErrorCode();

      settings::hpInfo hpData;
      settings::ctrlSettings hpSettings;
      HighFrequencyLoopRequester high_freq_;

    protected:
      bool checksumIsValid(uint8_t frame[], uint8_t size);
      uint8_t computeChecksum(uint8_t frame[], uint8_t size);
      bool frameIsValid(uint8_t frame[], uint8_t size);
      void sendControl(settings::ctrlSettings settings);

      bool decode(uint8_t frame[]);
    };

  }
}

#endif // __HPCI_H__
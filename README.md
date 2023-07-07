# PoolexDreamlineController

Credit goes to [Njanik](https://github.com/njanik/hayward-pool-heater-mqtt)

Work in progress, does work well but could definitely be improved.
Works with Poolex Dreamline 125.

Made for ESPHome, template configuration provided below.

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/CDX-24/PoolexDreamlineController
    refresh: 5s
    components: [hpci]

hpci:
  id: hpci_dev

sensor:
  - platform: template
    name: "Target Temperature"
    unit_of_measurement: "°C"
    accuracy_decimals: 0
    icon: "mdi:water-thermometer"
    device_class: "temperature"
    state_class: "measurement"
    lambda: |-
      return id(hpci_dev).getTargetTemp();
    update_interval: 30s
  - platform: template
    name: "Water Inlet Temperature"
    unit_of_measurement: "°C"
    accuracy_decimals: 0
    icon: "mdi:water-thermometer"
    device_class: "temperature"
    state_class: "measurement"
    lambda: |-
      return id(hpci_dev).getWaterInTemp();
    update_interval: 30s
  - platform: template
    name: "Water Outlet Temperature"
    unit_of_measurement: "°C"
    accuracy_decimals: 0
    icon: "mdi:water-thermometer"
    device_class: "temperature"
    state_class: "measurement"
    lambda: |-
      return id(hpci_dev).getWaterOutTemp();
    update_interval: 30s
  - platform: template
    name: "Outdoor Temperature"
    accuracy_decimals: 0
    unit_of_measurement: "°C"
    icon: "mdi:sun-thermometer"
    device_class: "temperature"
    state_class: "measurement"
    lambda: |-
      return id(hpci_dev).getOutdoorTemp();
    update_interval: 30s
  - platform: template
    name: "Error Code"
    icon: "mdi:hammer-wrench"
    accuracy_decimals: 0
    lambda: |-
      return id(hpci_dev).getErrorCode();
    update_interval: 30s

number:
  - platform: template
    name: "Target Temperature"
    icon: "mdi:water-thermometer"
    step: 1
    min_value: 20
    max_value: 35
    set_action:
      lambda: |-
        id(hpci_dev).setTargetTemp((uint16_t)x);
    lambda: |-
      return id(hpci_dev).getTargetTemp();
    update_interval: 30s

switch:
  - platform: template
    name: "Power"
    icon: "mdi:power"
    restore_mode: "ALWAYS_OFF"
    turn_on_action:
      lambda: |-
        id(hpci_dev).setOn(true);
    turn_off_action:
      lambda: |-
        id(hpci_dev).setOn(false);
    lambda: |-
        return id(hpci_dev).getOn();
```

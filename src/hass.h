#ifndef _HASS_H_
#define _HASS_H_

#include "types.h"
#include <ArduinoHA.h>
#include <WiFi.h>

namespace HASS {

void begin(uint8_t core, uint8_t priority);

} // namespace HASS

#endif

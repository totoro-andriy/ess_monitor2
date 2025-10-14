#ifndef _WEB_H_
#define _WEB_H_

#include <stdint.h>

namespace WEB {

void begin(uint8_t core, uint8_t priority);

void backToWebRoot();

} // namespace WEB

#endif

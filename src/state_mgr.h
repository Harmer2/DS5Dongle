#ifndef STATE_MGR_H
#define STATE_MGR_H

#include <cstdint>  // <-- ADD THIS LINE TO FIX THE ERROR

void state_init();
void state_set(uint8_t *data, const uint8_t size);
void state_update(const uint8_t *data, const uint8_t size);

#endif // STATE_MGR_H

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Pin definitions
// LED_BUILTIN is already defined in pins_arduino.h

// ESP-NOW configuration
#define MAX_PEERS 20

// Mode configuration
#define IS_MASTER true  // Thay đổi thành false để nạp cho thiết bị slave

// External declarations
extern bool is_master;

#endif

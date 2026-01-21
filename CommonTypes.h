#pragma once

#include "esp_psram.h"
#include <ArduinoJson.h>
#include <Arduino.h>
#include <vector>

#ifdef ESP32
#include <WiFi.h>
#include <SPIFFS.h>
#elif defined(ESP8266)
#include <FS.h>
#include <ESP8266WiFi.h>
#endif

#ifndef PSRAM_ALLOCATOR_DEFINED
#define PSRAM_ALLOCATOR_DEFINED

struct PsramAllocator {
  void* allocate(size_t size) {

    #ifdef ESP32
      return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    #else
      return malloc(size);
    #endif
  }

  void deallocate(void* pointer) {
    free(pointer);
  }

  void* reallocate(void* ptr, size_t new_size) {
    #ifdef ESP32
      return heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    #else
      return realloc(ptr, new_size);
    #endif
  }
};

#endif

using PsramJsonDocument = BasicJsonDocument<PsramAllocator>;

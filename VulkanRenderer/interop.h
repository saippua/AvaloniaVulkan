#pragma once
#ifndef INTEROP_H
#define INTEROP_H

#define FRAME_DELTA_COUNT 100

struct sPerf {
  double frameDelta[FRAME_DELTA_COUNT];
  int currentIndex = 0;
  // double frameDelta = 0;
};

typedef void (__stdcall *BufferCallback)(const char *buf, int len);
typedef void (__stdcall *DebugCallback)(const char * msg);
typedef void (__stdcall *PerformanceMonitorCallback)(sPerf data);
// typedef void (__stdcall *PerformanceMonitorCallback)(double frameDelta);
typedef void (__stdcall *SimpleCallback)();

#endif

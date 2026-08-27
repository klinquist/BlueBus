#ifndef BLUEBUS_SIM_H
#define BLUEBUS_SIM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void BlueBusSimInit(void);
void BlueBusSimReset(void);
void BlueBusSimAdvance(uint32_t milliseconds);
void BlueBusSimStartEngine(void);
void BlueBusSimStopEngine(void);
void BlueBusSimKeyOff(void);
void BlueBusSimSetRPM(uint16_t rpm);
void BlueBusSimSetSpeed(uint16_t kmh);
void BlueBusSimSetCoolantTemperature(int16_t celsius);
void BlueBusSimSetOilTemperature(int16_t celsius);
void BlueBusSimSetVoltage(uint16_t tenths);
void BlueBusSimSetSetting(uint8_t setting, uint8_t value);
uint8_t BlueBusSimGetSetting(uint8_t setting);
void BlueBusSimSetMetadata(const char *artist, const char *title, const char *album);
void BlueBusSimSetPlayback(uint8_t playing);
void BlueBusSimSetPairedDevice(uint8_t index, const char *name);
void BlueBusSimSetPairedDeviceCount(uint8_t count);
void BlueBusSimPressCD(uint8_t button);
void BlueBusSimPressNext(void);
void BlueBusSimPressPrevious(void);
const char *BlueBusSimGetDisplay(void);
const char *BlueBusSimGetTrace(void);
uint8_t BlueBusSimGetEngineRunning(void);
uint16_t BlueBusSimGetRPM(void);
uint16_t BlueBusSimGetSpeed(void);
uint16_t BlueBusSimGetVoltage(void);
int16_t BlueBusSimGetCoolantTemperature(void);
int16_t BlueBusSimGetOilTemperature(void);

#ifdef __cplusplus
}
#endif

#endif

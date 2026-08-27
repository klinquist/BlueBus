#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "lib/bt.h"
#include "lib/bt/bt_bc127.h"
#include "lib/bt/bt_bm83.h"
#include "lib/char_queue.h"
#include "lib/event.h"
#include "lib/log.h"
#include "lib/pcm51xx.h"
#include "lib/uart.h"
#include "lib/utils.h"

volatile BlueBusSimPORTDBits_t PORTDbits = {0};
int8_t BTBC127MicGainTable[22] = {0};
int8_t BTBM83MicGainTable[16] = {0};

UtilsAbstractDisplayValue_t UtilsDisplayValueInit(char *text, uint8_t status)
{
    UtilsAbstractDisplayValue_t value;
    memset(&value, 0, sizeof(value));
    UtilsStrncpy(value.text, text, sizeof(value.text));
    value.length = strlen(value.text);
    value.status = status;
    return value;
}

char *UtilsStrncpy(char *destination, const char *source, size_t size)
{
    if (size == 0) return destination;
    strncpy(destination, source, size - 1);
    destination[size - 1] = 0;
    return destination;
}

int16_t UtilsConvertCelsiusToFahrenheit(int16_t value) { return (value * 9) / 5 + 32; }
uint16_t UtilsConvertKmToMi(uint16_t value) { return (uint16_t) ((value * 621UL) / 1000UL); }
uint8_t UtilsStrToInt(char *value) { return (uint8_t) strtoul(value, 0, 10); }

UART_t UARTInit(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f, uint8_t g)
{
    UART_t uart;
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g;
    memset(&uart, 0, sizeof(uart));
    return uart;
}
void UARTReportErrors(UART_t *uart) { (void) uart; }
uint16_t CharQueueGetSize(volatile CharQueue_t *queue) { (void) queue; return 0; }
uint8_t CharQueueNext(volatile CharQueue_t *queue) { (void) queue; return 0; }
void CharQueueReset(volatile CharQueue_t *queue) { (void) queue; }

void LogMessage(const char *a, const char *b) { (void)a; (void)b; }
void LogRaw(const char *format, ...) { (void)format; }
void LogError(const char *format, ...) { (void)format; }
void LogDebug(uint8_t source, const char *format, ...) { (void)source; (void)format; }
void LogDebugByteArray(uint8_t source, uint8_t *data, uint16_t size, const char *suffix, const char *format, ...) { (void)source; (void)data; (void)size; (void)suffix; (void)format; }
void LogInfo(uint8_t source, const char *format, ...) { (void)source; (void)format; }
void LogWarning(const char *format, ...) { (void)format; }

void PCM51XXSetVolume(unsigned char volume) { (void) volume; }
void BTCommandCallAccept(BT_t *bt) { bt->callStatus = BT_CALL_ACTIVE; }
void BTCommandCallEnd(BT_t *bt) { bt->callStatus = BT_CALL_INACTIVE; }
void BTCommandPause(BT_t *bt) { bt->playbackStatus = BT_AVRCP_STATUS_PAUSED; }
void BTCommandPlay(BT_t *bt) { bt->playbackStatus = BT_AVRCP_STATUS_PLAYING; }
void BTCommandPlaybackToggle(BT_t *bt) { bt->playbackStatus = !bt->playbackStatus; }
void BTCommandPlaybackTrackNext(BT_t *bt) { (void) bt; }
void BTCommandPlaybackTrackPrevious(BT_t *bt) { (void) bt; }
void BTCommandSetDiscoverable(BT_t *bt, unsigned char state) { bt->discoverable = state; }
void BTCommandToggleVoiceRecognition(BT_t *bt) { bt->vrStatus = !bt->vrStatus; }
void BTPairedDeviceClearRecords(void) {}
void BC127CommandClose(BT_t *bt, uint8_t id) { (void)bt; (void)id; }
void BC127CommandProfileOpen(BT_t *bt, BTPairedDevice_t *device, char *profile) { (void)bt; (void)device; (void)profile; }
void BC127CommandSetMicGain(BT_t *bt, unsigned char gain, unsigned char a, unsigned char b) { (void)bt; (void)gain; (void)a; (void)b; }
void BC127CommandUnpair(BT_t *bt) { (void)bt; }
void BM83CommandConnect(BT_t *bt, BTPairedDevice_t *device, uint8_t profiles) { (void)profiles; bt->activeDevice.deviceId = device->number; }
void BM83CommandDisconnect(BT_t *bt, uint8_t profile) { (void)profile; bt->activeDevice.deviceId = 0; }
void BM83CommandMicGainDown(BT_t *bt) { (void)bt; }
void BM83CommandMicGainUp(BT_t *bt) { (void)bt; }
void BM83CommandRestore(BT_t *bt) { (void)bt; }

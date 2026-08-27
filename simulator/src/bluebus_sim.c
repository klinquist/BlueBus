#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "bluebus_sim.h"
#include "lib/bt.h"
#include "lib/config.h"
#include "lib/event.h"
#include "lib/ibus.h"
#include "ui/cd53.h"

#define SIM_TRACE_SIZE 16384
#define SIM_ENGINE_FRAME_INTERVAL_MS 100

static IBus_t SimIBus;
static BT_t SimBT;
static uint8_t SimInitialized;
static uint8_t SimEngineRunning;
static uint16_t SimRPM;
static uint16_t SimSpeed;
static uint16_t SimVoltage;
static int16_t SimCoolant;
static int16_t SimOil;
static uint32_t SimEngineFrameElapsed;
static char SimDisplay[CD53_DISPLAY_TEXT_LEN + 1];
static char SimTrace[SIM_TRACE_SIZE];
static size_t SimTraceLength;

void BlueBusSimHostTimerReset(void);
void BlueBusSimHostTimerAdvance(uint32_t milliseconds);
void BlueBusSimHostConfigReset(void);

static void SimTraceAppend(const char *format, ...)
{
    if (SimTraceLength >= sizeof(SimTrace) - 512) {
        size_t keepFrom = sizeof(SimTrace) / 3;
        while (keepFrom < SimTraceLength && SimTrace[keepFrom] != '\n') {
            keepFrom++;
        }
        if (keepFrom < SimTraceLength) {
            keepFrom++;
        }
        memmove(SimTrace, SimTrace + keepFrom, SimTraceLength - keepFrom);
        SimTraceLength -= keepFrom;
        SimTrace[SimTraceLength] = 0;
    }
    va_list args;
    va_start(args, format);
    int written = vsnprintf(
        SimTrace + SimTraceLength,
        sizeof(SimTrace) - SimTraceLength,
        format,
        args
    );
    va_end(args);
    if (written > 0) {
        size_t amount = (size_t) written;
        size_t remaining = sizeof(SimTrace) - SimTraceLength - 1;
        SimTraceLength += amount < remaining ? amount : remaining;
    }
}

static uint8_t SimChecksum(uint8_t *frame, size_t frameSizeWithoutChecksum)
{
    uint8_t checksum = 0;
    size_t idx;
    for (idx = 0; idx < frameSizeWithoutChecksum; idx++) {
        checksum ^= frame[idx];
    }
    return checksum;
}

static void SimTraceFrame(const char *direction, const uint8_t *frame, size_t size)
{
    size_t idx;
    SimTraceAppend("%s", direction);
    for (idx = 0; idx < size; idx++) {
        SimTraceAppend(" %02X", frame[idx]);
    }
    SimTraceAppend("\n");
}

static uint8_t SimFeedFrame(uint8_t *frame, size_t size)
{
    SimTraceFrame("RX", frame, size);
    return IBusProcessFrame(&SimIBus, frame, size);
}

static void SimBuildAndFeed(
    uint8_t source,
    uint8_t destination,
    const uint8_t *data,
    size_t dataSize
) {
    uint8_t frame[IBUS_MAX_MSG_LENGTH] = {0};
    size_t size = dataSize + 4;
    frame[0] = source;
    frame[1] = dataSize + 2;
    frame[2] = destination;
    memcpy(frame + 3, data, dataSize);
    frame[size - 1] = SimChecksum(frame, size - 1);
    SimFeedFrame(frame, size);
}

static void SimDrainTransmitFrames(void)
{
    while (SimIBus.txBufferReadIdx != SimIBus.txBufferWriteIdx) {
        uint8_t *frame = SimIBus.txBuffer[SimIBus.txBufferReadIdx];
        size_t frameSize = frame[IBUS_PKT_LEN] + 2;
        SimTraceFrame("TX", frame, frameSize);
        if (
            frame[IBUS_PKT_SRC] == IBUS_DEVICE_TEL &&
            frame[IBUS_PKT_DST] == IBUS_DEVICE_IKE &&
            frame[IBUS_PKT_CMD] == IBUS_CMD_GT_WRITE_TITLE &&
            frame[IBUS_PKT_DB1] == 0x42 &&
            frame[IBUS_PKT_DB2] == 0x32 &&
            frameSize >= 7
        ) {
            size_t textLength = frameSize - 7;
            if (textLength > CD53_DISPLAY_TEXT_LEN) {
                textLength = CD53_DISPLAY_TEXT_LEN;
            }
            memset(SimDisplay, 0, sizeof(SimDisplay));
            memcpy(SimDisplay, frame + 6, textLength);
        }
        memset(frame, 0, IBUS_MAX_MSG_LENGTH);
        SimIBus.txBufferReadIdx = (SimIBus.txBufferReadIdx + 1) % IBUS_TX_BUFFER_SIZE;
        SimIBus.txBufferReadbackIdx = SimIBus.txBufferReadIdx;
    }
}

static void SimSendIgnition(uint8_t status)
{
    uint8_t data[] = {IBUS_CMD_IKE_IGN_STATUS_RESP, status};
    SimBuildAndFeed(IBUS_DEVICE_IKE, IBUS_DEVICE_GLO, data, sizeof(data));
}

static void SimSendEngineFrame(void)
{
    uint8_t speedByte = SimSpeed > 510 ? 255 : (uint8_t) (SimSpeed / 2);
    uint8_t rpmByte = SimEngineRunning ? (uint8_t) (SimRPM / 100) : 0;
    if (SimEngineRunning && rpmByte == 0) {
        rpmByte = 1;
    }
    uint8_t data[] = {IBUS_CMD_IKE_SPEED_RPM_UPDATE, speedByte, rpmByte};
    SimBuildAndFeed(IBUS_DEVICE_IKE, IBUS_DEVICE_GLO, data, sizeof(data));
}

static void SimSendCoolantFrame(void)
{
    uint8_t coolant = SimCoolant < 0 ? 0 : (SimCoolant > 127 ? 127 : (uint8_t) SimCoolant);
    uint8_t data[] = {IBUS_CMD_IKE_TEMP_UPDATE, 20, coolant};
    SimBuildAndFeed(IBUS_DEVICE_IKE, IBUS_DEVICE_GLO, data, sizeof(data));
}

static void SimFindOilBytes(int16_t target, uint8_t *highResolution, uint8_t *coarse)
{
    double bestError = 10000.0;
    uint16_t first;
    uint16_t second;
    *highResolution = 1;
    *coarse = 1;
    for (second = 0; second <= 255; second++) {
        for (first = 1; first <= 255; first++) {
            double raw = first * 0.00005 + second * 0.01275;
            double value = 67.2529 * log(raw) + 310.0;
            double error = fabs(value - (target + 0.5));
            if (error < bestError) {
                bestError = error;
                *highResolution = (uint8_t) first;
                *coarse = (uint8_t) second;
            }
        }
    }
}

static void SimSendLCMFrame(void)
{
    uint8_t frame[37] = {0};
    uint8_t oilFine;
    uint8_t oilCoarse;
    SimFindOilBytes(SimOil, &oilFine, &oilCoarse);
    frame[IBUS_PKT_SRC] = IBUS_DEVICE_LCM;
    frame[IBUS_PKT_LEN] = 0x23;
    frame[IBUS_PKT_DST] = IBUS_DEVICE_DIA;
    frame[IBUS_PKT_CMD] = IBUS_CMD_DIA_DIAG_RESPONSE;
    frame[IBUS_PKT_DB10] = (uint8_t) ((SimVoltage * IBUS_LM_BATTERY_SCALE_DEFAULT + 99) / 100);
    frame[23] = oilFine;
    frame[24] = oilCoarse;
    frame[36] = SimChecksum(frame, 36);
    SimFeedFrame(frame, sizeof(frame));
}

static void SimSendCDCCommand(uint8_t command, uint8_t parameter, uint8_t withParameter)
{
    uint8_t data[3] = {IBUS_COMMAND_CDC_REQUEST, command, parameter};
    SimBuildAndFeed(
        IBUS_DEVICE_RAD,
        IBUS_DEVICE_CDC,
        data,
        withParameter ? sizeof(data) : sizeof(data) - 1
    );
}

static void SimConnectDevice(void *context, uint8_t *deviceIndex)
{
    BT_t *bt = (BT_t *) context;
    if (deviceIndex == 0 || *deviceIndex >= bt->pairedDevicesCount) {
        return;
    }
    bt->activeDevice.status = BT_DEVICE_STATUS_CONNECTED;
    bt->activeDevice.deviceIndex = *deviceIndex;
    bt->activeDevice.deviceId = *deviceIndex + 1;
    bt->activeDevice.a2dpId = 1;
}

void BlueBusSimInit(void)
{
    if (SimInitialized) {
        CD53Destroy();
    }
    EventReset();
    BlueBusSimHostTimerReset();
    BlueBusSimHostConfigReset();
    memset(&SimIBus, 0, sizeof(SimIBus));
    memset(&SimBT, 0, sizeof(SimBT));
    memset(SimDisplay, 0, sizeof(SimDisplay));
    memset(SimTrace, 0, sizeof(SimTrace));
    SimTraceLength = 0;
    SimIBus.cdChangerFunction = IBUS_CDC_FUNC_NOT_PLAYING;
    SimIBus.ignitionStatus = IBUS_IGNITION_OFF;
    SimIBus.vehicleType = IBUS_VEHICLE_TYPE_E38_E39_E52_E53;
    SimBT.type = BT_BTM_TYPE_BM83;
    SimBT.playbackStatus = BT_AVRCP_STATUS_PLAYING;
    SimRPM = 800;
    SimSpeed = 0;
    SimVoltage = 140;
    SimCoolant = 90;
    SimOil = 90;
    SimEngineRunning = 0;
    SimEngineFrameElapsed = 0;
    CD53Init(&SimBT, &SimIBus);
    EventRegisterCallback(UI_EVENT_INITIATE_CONNECTION, &SimConnectDevice, &SimBT);
    SimInitialized = 1;
    SimSendIgnition(IBUS_IGNITION_KL15);
    SimSendCDCCommand(IBUS_CDC_CMD_START_PLAYING, 0, 0);
    SimSendCoolantFrame();
    SimSendLCMFrame();
    BlueBusSimHostTimerAdvance(CD53_DISPLAY_TIMER_INT);
    SimDrainTransmitFrames();
}

void BlueBusSimReset(void) { BlueBusSimInit(); }

void BlueBusSimAdvance(uint32_t milliseconds)
{
    uint32_t remaining = milliseconds;
    while (remaining > 0) {
        uint32_t untilEngineFrame = SIM_ENGINE_FRAME_INTERVAL_MS - SimEngineFrameElapsed;
        uint32_t step = remaining < untilEngineFrame ? remaining : untilEngineFrame;
        BlueBusSimHostTimerAdvance(step);
        SimEngineFrameElapsed += step;
        remaining -= step;
        if (SimEngineFrameElapsed >= SIM_ENGINE_FRAME_INTERVAL_MS) {
            SimEngineFrameElapsed = 0;
            if (SimEngineRunning) {
                SimSendEngineFrame();
            }
        }
        SimDrainTransmitFrames();
    }
}

void BlueBusSimStartEngine(void)
{
    SimEngineRunning = 1;
    if (SimRPM == 0) {
        SimRPM = 800;
    }
    SimSendIgnition(IBUS_IGNITION_KL15);
    SimSendCDCCommand(IBUS_CDC_CMD_START_PLAYING, 0, 0);
    SimSendEngineFrame();
    BlueBusSimAdvance(CD53_DISPLAY_TIMER_INT);
}

void BlueBusSimStopEngine(void)
{
    SimEngineRunning = 0;
    SimSendEngineFrame();
    BlueBusSimAdvance(CD53_DISPLAY_TIMER_INT);
}

void BlueBusSimKeyOff(void)
{
    SimEngineRunning = 0;
    SimSendIgnition(IBUS_IGNITION_OFF);
    SimDrainTransmitFrames();
}

void BlueBusSimSetRPM(uint16_t rpm) { SimRPM = rpm > 25500 ? 25500 : rpm; if (SimEngineRunning) SimSendEngineFrame(); }
void BlueBusSimSetSpeed(uint16_t kmh) { SimSpeed = kmh > 510 ? 510 : (kmh / 2) * 2; SimSendEngineFrame(); BlueBusSimAdvance(CD53_DISPLAY_TIMER_INT); }
void BlueBusSimSetCoolantTemperature(int16_t celsius) { SimCoolant = celsius < 0 ? 0 : (celsius > 127 ? 127 : celsius); SimSendCoolantFrame(); BlueBusSimAdvance(CD53_DISPLAY_TIMER_INT); }
void BlueBusSimSetOilTemperature(int16_t celsius) { SimOil = celsius; SimSendLCMFrame(); SimOil = SimIBus.oilTemperature; BlueBusSimAdvance(CD53_DISPLAY_TIMER_INT); }
void BlueBusSimSetVoltage(uint16_t tenths) { SimVoltage = tenths > 187 ? 187 : tenths; SimSendLCMFrame(); SimVoltage = SimIBus.batteryVoltage; BlueBusSimAdvance(CD53_DISPLAY_TIMER_INT); }
void BlueBusSimSetSetting(uint8_t setting, uint8_t value)
{
    ConfigSetSetting(setting, value);
    if (setting == CONFIG_SETTING_COLD_OIL_DISPLAY) {
        SimIBus.oilTemperature = 0;
        SimSendLCMFrame();
    }
    BlueBusSimAdvance(CD53_DISPLAY_TIMER_INT);
}
uint8_t BlueBusSimGetSetting(uint8_t setting) { return ConfigGetSetting(setting); }

void BlueBusSimSetMetadata(const char *artist, const char *title, const char *album)
{
    snprintf(SimBT.artist, sizeof(SimBT.artist), "%s", artist == 0 ? "" : artist);
    snprintf(SimBT.title, sizeof(SimBT.title), "%s", title == 0 ? "" : title);
    snprintf(SimBT.album, sizeof(SimBT.album), "%s", album == 0 ? "" : album);
    EventTriggerCallback(BT_EVENT_METADATA_UPDATE, 0);
    BlueBusSimAdvance(CD53_DISPLAY_TIMER_INT);
}

void BlueBusSimSetPlayback(uint8_t playing)
{
    SimBT.playbackStatus = playing ? BT_AVRCP_STATUS_PLAYING : BT_AVRCP_STATUS_PAUSED;
    EventTriggerCallback(BT_EVENT_PLAYBACK_STATUS_CHANGE, 0);
    BlueBusSimAdvance(CD53_DISPLAY_TIMER_INT);
}

void BlueBusSimSetPairedDevice(uint8_t index, const char *name)
{
    if (index >= BT_MAX_PAIRINGS) return;
    memset(&SimBT.pairedDevices[index], 0, sizeof(BTPairedDevice_t));
    SimBT.pairedDevices[index].number = index + 1;
    snprintf(
        SimBT.pairedDevices[index].deviceName,
        sizeof(SimBT.pairedDevices[index].deviceName),
        "%s",
        name == 0 ? "" : name
    );
}

void BlueBusSimSetPairedDeviceCount(uint8_t count)
{
    SimBT.pairedDevicesCount = count > BT_MAX_PAIRINGS ? BT_MAX_PAIRINGS : count;
}

void BlueBusSimPressCD(uint8_t button)
{
    if (button < 1 || button > 6) return;
    SimSendCDCCommand(IBUS_CDC_CMD_CD_CHANGE, button, 1);
    BlueBusSimAdvance(CD53_DISPLAY_TIMER_INT);
}

void BlueBusSimPressNext(void) { SimSendCDCCommand(IBUS_CDC_CMD_CHANGE_TRACK, 0, 1); BlueBusSimAdvance(CD53_DISPLAY_TIMER_INT); }
void BlueBusSimPressPrevious(void) { SimSendCDCCommand(IBUS_CDC_CMD_CHANGE_TRACK, 1, 1); BlueBusSimAdvance(CD53_DISPLAY_TIMER_INT); }
const char *BlueBusSimGetDisplay(void) { return SimDisplay; }
const char *BlueBusSimGetTrace(void) { return SimTrace; }
uint8_t BlueBusSimGetEngineRunning(void) { return SimEngineRunning; }
uint16_t BlueBusSimGetRPM(void) { return SimRPM; }
uint16_t BlueBusSimGetSpeed(void) { return SimSpeed; }
uint16_t BlueBusSimGetVoltage(void) { return SimIBus.batteryVoltage; }
int16_t BlueBusSimGetCoolantTemperature(void) { return SimIBus.coolantTemperature; }
int16_t BlueBusSimGetOilTemperature(void) { return SimIBus.oilTemperature; }

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "bluebus_sim.h"
#include "lib/config.h"

static void SelectOBC(void)
{
    BlueBusSimPressCD(2);
    BlueBusSimPressCD(2);
}

static void AssertDisplay(const char *expected)
{
    const char *actual = BlueBusSimGetDisplay();
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "expected display '%s', got '%s'\n%s", expected, actual, BlueBusSimGetTrace());
        assert(0);
    }
}

int main(void)
{
    BlueBusSimInit();
    AssertDisplay("Bluetooth");
    assert(BlueBusSimGetEngineRunning() == 0);
    assert(BlueBusSimGetRPM() == 0);

    SelectOBC();
    assert(strncmp(BlueBusSimGetDisplay(), "C:", 2) == 0);

    BlueBusSimSetOilTemperature(71);
    assert(strcmp(BlueBusSimGetDisplay(), "OIL COLD") != 0);

    BlueBusSimSetSetting(CONFIG_SETTING_COLD_OIL_DISPLAY, CONFIG_SETTING_ON);
    AssertDisplay("OIL COLD");

    BlueBusSimSetOilTemperature(72);
    assert(strcmp(BlueBusSimGetDisplay(), "OIL COLD") != 0);
    BlueBusSimSetOilTemperature(71);
    AssertDisplay("OIL COLD");

    BlueBusSimSetVoltage(132);
    AssertDisplay("OIL COLD");
    BlueBusSimStartEngine();
    assert(BlueBusSimGetEngineRunning() == 1);
    assert(BlueBusSimGetRPM() == 800);
    AssertDisplay("LOW V13.2");

    BlueBusSimSetSetting(CONFIG_SETTING_LOW_VOLT_WARNING, CONFIG_SETTING_OFF);
    AssertDisplay("OIL COLD");
    BlueBusSimSetSetting(CONFIG_SETTING_LOW_VOLT_WARNING, CONFIG_SETTING_ON);
    AssertDisplay("LOW V13.2");

    BlueBusSimSetVoltage(133);
    AssertDisplay("OIL COLD");
    BlueBusSimStopEngine();
    assert(BlueBusSimGetEngineRunning() == 0);
    assert(BlueBusSimGetRPM() == 0);
    BlueBusSimSetVoltage(120);
    AssertDisplay("OIL COLD");

    BlueBusSimKeyOff();
    AssertDisplay("");
    BlueBusSimStartEngine();
    assert(strlen(BlueBusSimGetDisplay()) > 0);

    BlueBusSimReset();
    BlueBusSimSetPairedDevice(0, "Alice Phone");
    BlueBusSimSetPairedDevice(1, "Bob Phone");
    BlueBusSimSetPairedDeviceCount(2);
    BlueBusSimPressCD(5);
    BlueBusSimAdvance(1125);
    AssertDisplay("Alice Phone");
    BlueBusSimPressNext();
    AssertDisplay("Bob Phone");

    BlueBusSimReset();
    BlueBusSimStartEngine();
    BlueBusSimSetVoltage(129);
    AssertDisplay("LOW V12.9");

    assert(strstr(BlueBusSimGetTrace(), "RX 80") != 0);
    assert(strstr(BlueBusSimGetTrace(), "TX C8") != 0);
    puts("CD53 simulator checks passed");
    return 0;
}

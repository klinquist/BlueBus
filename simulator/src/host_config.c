#include <stdio.h>
#include <string.h>
#include "lib/config.h"
#include "lib/ibus.h"
#include "mappings.h"
#include "ui/menu/menu_singleline.h"

static uint8_t Settings[256];
static uint8_t UIMode;
static uint8_t TempUnit;
static uint8_t DistUnit;

void BlueBusSimHostConfigReset(void)
{
    memset(Settings, 0, sizeof(Settings));
    UIMode = CONFIG_UI_CD53;
    TempUnit = CONFIG_SETTING_TEMP_FAHRENHEIT;
    DistUnit = 1;
    Settings[CONFIG_SETTING_METADATA_MODE] = MENU_SINGLELINE_SETTING_METADATA_MODE_STATIC;
    Settings[CONFIG_SETTING_COLD_OIL_DISPLAY] = CONFIG_SETTING_OFF;
    Settings[CONFIG_SETTING_LOW_VOLT_WARNING] = CONFIG_SETTING_ON;
    Settings[CONFIG_SETTING_HFP] = CONFIG_SETTING_ON;
}

uint8_t ConfigGetSetting(uint8_t setting) { return Settings[setting]; }
void ConfigSetSetting(uint8_t setting, uint8_t value) { Settings[setting] = value; }
uint8_t ConfigGetUIMode(void) { return UIMode; }
void ConfigSetUIMode(uint8_t mode) { UIMode = mode; }
uint8_t ConfigGetTempUnit(void) { return TempUnit; }
uint8_t ConfigGetDistUnit(void) { return DistUnit; }
void BlueBusSimHostSetTempUnit(uint8_t unit) { TempUnit = unit; }
void BlueBusSimHostSetDistUnit(uint8_t unit) { DistUnit = unit; }
uint8_t ConfigGetNavType(void) { return 0; }
uint8_t ConfigGetVehicleType(void) { return IBUS_VEHICLE_TYPE_E38_E39_E52_E53; }
uint8_t ConfigGetLMVariant(void) { return 0; }
uint8_t ConfigGetBuildWeek(void) { return 1; }
uint8_t ConfigGetBuildYear(void) { return 26; }
uint16_t ConfigGetSerialNumber(void) { return 1; }
void ConfigGetFirmwareVersionString(char *version)
{
    snprintf(
        version,
        9,
        "%d.%d.%d",
        FIRMWARE_VERSION_MAJOR,
        FIRMWARE_VERSION_MINOR,
        FIRMWARE_VERSION_PATCH
    );
}
uint8_t ConfigGetComfortLock(void) { return Settings[CONFIG_SETTING_COMFORT_LOCKS]; }
uint8_t ConfigGetComfortUnlock(void) { return Settings[CONFIG_SETTING_COMFORT_UNLOCK]; }
void ConfigSetComfortLock(uint8_t value) { Settings[CONFIG_SETTING_COMFORT_LOCKS] = value; }
void ConfigSetComfortUnlock(uint8_t value) { Settings[CONFIG_SETTING_COMFORT_UNLOCK] = value; }

#include <ctype.h>
#include <curses.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bluebus_sim.h"
#include "lib/config.h"

#define UI_TICK_MS 50

static int FahrenheitToCelsius(double fahrenheit)
{
    return (int) lround((fahrenheit - 32.0) * 5.0 / 9.0);
}

static int MPHToKMH(double mph)
{
    return (int) lround(mph * 1.609344);
}

static double Clamp(double value, double minimum, double maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static double PromptNumber(const char *label, double current)
{
    char input[32] = {0};
    int row = LINES - 2;
    timeout(-1);
    echo();
    curs_set(1);
    move(row, 0);
    clrtoeol();
    mvprintw(row, 0, "%s [%.1f]: ", label, current);
    refresh();
    getnstr(input, sizeof(input) - 1);
    noecho();
    curs_set(0);
    timeout(UI_TICK_MS);
    if (input[0] == 0) {
        return current;
    }
    return strtod(input, 0);
}

static void PromptMetadata(void)
{
    char artist[64] = {0};
    char title[64] = {0};
    timeout(-1);
    echo();
    curs_set(1);
    move(LINES - 3, 0);
    clrtoeol();
    mvprintw(LINES - 3, 0, "Artist: ");
    getnstr(artist, sizeof(artist) - 1);
    move(LINES - 2, 0);
    clrtoeol();
    mvprintw(LINES - 2, 0, "Title:  ");
    getnstr(title, sizeof(title) - 1);
    noecho();
    curs_set(0);
    timeout(UI_TICK_MS);
    BlueBusSimSetMetadata(artist, title, "BlueBus Simulator");
}

static const char *TraceTail(const char *trace, int lineCount)
{
    const char *cursor = trace + strlen(trace);
    int lines = 0;
    while (cursor > trace && lines <= lineCount) {
        cursor--;
        if (*cursor == '\n') {
            lines++;
        }
    }
    if (cursor > trace) {
        cursor++;
    }
    return cursor;
}

static void DrawTrace(int row, int height, int width)
{
    const char *cursor = TraceTail(BlueBusSimGetTrace(), height);
    int line = 0;
    while (*cursor != 0 && line < height) {
        char buffer[256] = {0};
        size_t length = 0;
        while (cursor[length] != 0 && cursor[length] != '\n' && length < sizeof(buffer) - 1) {
            length++;
        }
        memcpy(buffer, cursor, length);
        mvaddnstr(row + line, 0, buffer, width - 1);
        clrtoeol();
        cursor += length;
        if (*cursor == '\n') cursor++;
        line++;
    }
    while (line < height) {
        move(row + line, 0);
        clrtoeol();
        line++;
    }
}

static void DrawWorkbench(int showTrace)
{
    int width = COLS;
    int traceRow = 16;
    erase();
    attron(A_BOLD);
    mvprintw(0, 0, "BlueBus CD53 Firmware Simulator");
    attroff(A_BOLD);
    mvprintw(2, 0, "+-------------+");
    mvprintw(3, 0, "| %-11.11s |", BlueBusSimGetDisplay());
    mvprintw(4, 0, "+-------------+");

    mvprintw(2, 18, "Engine: %-7s  RPM: %-5u  Speed: %3u km/h / %3.0f mph",
        BlueBusSimGetEngineRunning() ? "RUNNING" : "STOPPED",
        BlueBusSimGetRPM(),
        BlueBusSimGetSpeed(),
        BlueBusSimGetSpeed() * 0.621371
    );
    mvprintw(3, 18, "Oil: %3d C / %3.0f F   Coolant: %3d C / %3.0f F",
        BlueBusSimGetOilTemperature(),
        BlueBusSimGetOilTemperature() * 9.0 / 5.0 + 32.0,
        BlueBusSimGetCoolantTemperature(),
        BlueBusSimGetCoolantTemperature() * 9.0 / 5.0 + 32.0
    );
    mvprintw(4, 18, "Voltage: %.1f V   ColdOilDisp: %-3s   LowVoltWrn: %-3s",
        BlueBusSimGetVoltage() / 10.0,
        BlueBusSimGetSetting(CONFIG_SETTING_COLD_OIL_DISPLAY) ? "ON" : "OFF",
        BlueBusSimGetSetting(CONFIG_SETTING_LOW_VOLT_WARNING) ? "ON" : "OFF"
    );

    mvprintw(6, 0, "Radio: [1-6] CD buttons  [n]/[p] seek  [m] metadata  [d] two phones");
    mvprintw(8, 0, "Car:   [e] start  [x] stop  [k] key off  [r] RPM  [s] speed (mph)");
    mvprintw(9, 0, "       [o] oil (F)  [c] coolant (F)  [v] voltage");
    mvprintw(11, 0, "Config:[O] ColdOilDisp  [V] LowVoltWrn");
    mvprintw(12, 0, "Preset:[a] cold oil  [l] low voltage");
    mvprintw(13, 0, "System:[R] reset  [t] trace  [q] quit");

    if (showTrace && LINES > traceRow + 2) {
        attron(A_BOLD);
        mvprintw(15, 0, "I-Bus trace (generated RX and firmware TX frames)");
        attroff(A_BOLD);
        DrawTrace(traceRow, LINES - traceRow - 2, width);
    }
    refresh();
}

static void ColdOilPreset(void)
{
    BlueBusSimReset();
    BlueBusSimSetSetting(CONFIG_SETTING_COLD_OIL_DISPLAY, CONFIG_SETTING_ON);
    BlueBusSimSetOilTemperature(FahrenheitToCelsius(150));
    BlueBusSimStartEngine();
    BlueBusSimPressCD(2);
    BlueBusSimPressCD(2);
}

static void LowVoltagePreset(void)
{
    BlueBusSimReset();
    BlueBusSimStartEngine();
    BlueBusSimSetVoltage(132);
}

int main(void)
{
    int running = 1;
    int showTrace = 1;
    BlueBusSimInit();
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    timeout(UI_TICK_MS);

    while (running) {
        int key;
        DrawWorkbench(showTrace);
        key = getch();
        if (key == ERR) {
            BlueBusSimAdvance(UI_TICK_MS);
            continue;
        }
        if (key >= '1' && key <= '6') BlueBusSimPressCD((uint8_t) (key - '0'));
        else if (key == 'q') running = 0;
        else if (key == 'e') BlueBusSimStartEngine();
        else if (key == 'x') BlueBusSimStopEngine();
        else if (key == 'k') BlueBusSimKeyOff();
        else if (key == 'n') BlueBusSimPressNext();
        else if (key == 'p') BlueBusSimPressPrevious();
        else if (key == 'm') PromptMetadata();
        else if (key == 'd') {
            BlueBusSimSetPairedDevice(0, "Alice Phone");
            BlueBusSimSetPairedDevice(1, "Bob Phone");
            BlueBusSimSetPairedDeviceCount(2);
        }
        else if (key == 'r') BlueBusSimSetRPM((uint16_t) Clamp(PromptNumber("RPM", BlueBusSimGetRPM()), 0, 25500));
        else if (key == 's') BlueBusSimSetSpeed((uint16_t) MPHToKMH(Clamp(PromptNumber("Speed in mph", BlueBusSimGetSpeed() * 0.621371), 0, 317)));
        else if (key == 'o') BlueBusSimSetOilTemperature((int16_t) FahrenheitToCelsius(PromptNumber("Oil temperature in F", BlueBusSimGetOilTemperature() * 9.0 / 5.0 + 32.0)));
        else if (key == 'c') BlueBusSimSetCoolantTemperature((int16_t) FahrenheitToCelsius(PromptNumber("Coolant temperature in F", BlueBusSimGetCoolantTemperature() * 9.0 / 5.0 + 32.0)));
        else if (key == 'v') BlueBusSimSetVoltage((uint16_t) lround(Clamp(PromptNumber("Voltage", BlueBusSimGetVoltage() / 10.0), 0, 18.7) * 10.0));
        else if (key == 'O') BlueBusSimSetSetting(CONFIG_SETTING_COLD_OIL_DISPLAY, !BlueBusSimGetSetting(CONFIG_SETTING_COLD_OIL_DISPLAY));
        else if (key == 'V') BlueBusSimSetSetting(CONFIG_SETTING_LOW_VOLT_WARNING, !BlueBusSimGetSetting(CONFIG_SETTING_LOW_VOLT_WARNING));
        else if (key == 'a') ColdOilPreset();
        else if (key == 'l') LowVoltagePreset();
        else if (key == 'R') BlueBusSimReset();
        else if (key == 't') showTrace = !showTrace;
        BlueBusSimAdvance(UI_TICK_MS);
    }

    endwin();
    return 0;
}

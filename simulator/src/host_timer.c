#include <stdint.h>
#include <string.h>
#include "lib/timer.h"

static uint32_t CurrentMillis;
static TimerScheduledTask_t Tasks[TIMER_TASKS_MAX];
static uint8_t TaskCount;

void BlueBusSimHostTimerReset(void)
{
    CurrentMillis = 0;
    TaskCount = 0;
    memset(Tasks, 0, sizeof(Tasks));
}

void BlueBusSimHostTimerAdvance(uint32_t milliseconds)
{
    uint32_t tick;
    for (tick = 0; tick < milliseconds; tick++) {
        uint8_t idx;
        CurrentMillis++;
        for (idx = 0; idx < TaskCount; idx++) {
            TimerScheduledTask_t *task = &Tasks[idx];
            if (task->task != 0 && task->interval > 0) {
                task->ticks++;
                if (task->ticks >= task->interval) {
                    task->ticks = 0;
                    task->task(task->context);
                }
            }
        }
    }
}

void TimerInit(void)
{
    BlueBusSimHostTimerReset();
}

void TimerDelayMicroseconds(uint16_t delay)
{
    (void) delay;
}

uint32_t TimerGetMillis(void)
{
    return CurrentMillis;
}

void TimerProcessScheduledTasks(void)
{
}

uint8_t TimerRegisterScheduledTask(void *task, void *context, uint16_t interval)
{
    uint8_t idx;
    for (idx = 0; idx < TaskCount; idx++) {
        if (Tasks[idx].task == 0) {
            break;
        }
    }
    if (idx == TaskCount && TaskCount < TIMER_TASKS_MAX) {
        TaskCount++;
    }
    if (idx >= TIMER_TASKS_MAX) {
        return 0;
    }
    Tasks[idx].task = task;
    Tasks[idx].context = context;
    Tasks[idx].interval = interval;
    Tasks[idx].ticks = 0;
    return idx;
}

uint8_t TimerUnregisterScheduledTask(void *task)
{
    uint8_t idx;
    for (idx = 0; idx < TaskCount; idx++) {
        if (Tasks[idx].task == task) {
            memset(&Tasks[idx], 0, sizeof(Tasks[idx]));
            return 0;
        }
    }
    return 1;
}

void TimerUnregisterScheduledTaskById(uint8_t taskId)
{
    if (taskId < TaskCount) {
        memset(&Tasks[taskId], 0, sizeof(Tasks[taskId]));
    }
}

void TimerResetScheduledTask(uint8_t taskId)
{
    if (taskId < TaskCount && Tasks[taskId].task != 0) {
        Tasks[taskId].ticks = 0;
    }
}

void TimerSetTaskInterval(uint8_t taskId, uint16_t interval)
{
    if (taskId < TaskCount && Tasks[taskId].task != 0) {
        Tasks[taskId].interval = interval;
    }
}

void TimerTriggerScheduledTask(uint8_t taskId)
{
    if (taskId < TaskCount && Tasks[taskId].task != 0) {
        Tasks[taskId].ticks = 0;
        Tasks[taskId].task(Tasks[taskId].context);
        Tasks[taskId].ticks = 0;
    }
}

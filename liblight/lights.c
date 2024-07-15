/*
 * Copyright (C) 2016 The CyanogenMod Project
 * Copyright (C) 2018 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "venice_lights"
#define LOG_NDEBUG 0
#include <log/log.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#include <sys/ioctl.h>
#include <sys/types.h>

#include <hardware/lights.h>

#define LCD_FILE "/sys/class/leds/lcd-backlight/brightness"

#define RED_LED_FILE "/sys/class/leds/led:rgb_red/brightness"
#define RED_DUTY_PCTS_FILE "/sys/class/leds/led:rgb_red/duty_pcts"
#define RED_START_IDX_FILE "/sys/class/leds/led:rgb_red/start_idx"
#define RED_PAUSE_LO_FILE "/sys/class/leds/led:rgb_red/pause_lo"
#define RED_PAUSE_HI_FILE "/sys/class/leds/led:rgb_red/pause_hi"
#define RED_RAMP_STEP_MS_FILE "/sys/class/leds/led:rgb_red/ramp_step_ms"
#define RED_BLINK_FILE "/sys/class/leds/led:rgb_red/blink"

#define GREEN_LED_FILE "/sys/class/leds/led:rgb_green/brightness"
#define GREEN_DUTY_PCTS_FILE "/sys/class/leds/led:rgb_green/duty_pcts"
#define GREEN_START_IDX_FILE "/sys/class/leds/led:rgb_green/start_idx"
#define GREEN_PAUSE_LO_FILE "/sys/class/leds/led:rgb_green/pause_lo"
#define GREEN_PAUSE_HI_FILE "/sys/class/leds/led:rgb_green/pause_hi"
#define GREEN_RAMP_STEP_MS_FILE "/sys/class/leds/led:rgb_green/ramp_step_ms"
#define GREEN_BLINK_FILE "/sys/class/leds/led:rgb_green/blink"

#define BLUE_LED_FILE "/sys/class/leds/led:rgb_blue/brightness"
#define BLUE_DUTY_PCTS_FILE "/sys/class/leds/led:rgb_blue/duty_pcts"
#define BLUE_START_IDX_FILE "/sys/class/leds/led:rgb_blue/start_idx"
#define BLUE_PAUSE_LO_FILE "/sys/class/leds/led:rgb_blue/pause_lo"
#define BLUE_PAUSE_HI_FILE "/sys/class/leds/led:rgb_blue/pause_hi"
#define BLUE_RAMP_STEP_MS_FILE "/sys/class/leds/led:rgb_blue/ramp_step_ms"
#define BLUE_BLINK_FILE "/sys/class/leds/led:rgb_blue/blink"

#define KEYBOARD_INFO_FILE "/sys/devices/soc.0/f9923000.i2c/i2c-1/1-0020/input/input1/info"
#define KEYBOARD_FILE "/sys/class/leds/kpd-backlight/brightness"

#define RAMP_SIZE 8
#define RAMP_STEP_DURATION 50

static const int BRIGHTNESS_RAMP[RAMP_SIZE]
        = { 0, 12, 25, 37, 50, 72, 85, 100 };

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static struct light_state_t g_battery;
static struct light_state_t g_notification;
static struct light_state_t g_attention;

static int write_int(const char *path, int value)
{
    int fd = open(path, O_WRONLY);

    if (fd >= 0) {
        char buffer[20];
        int bytes = sprintf(buffer, "%d\n", value);
        int amt = write(fd, buffer, bytes);
        close(fd);
        return amt == -1 ? -errno : 0;
    } else {
        ALOGE("write_int failed to open %s\n", path);
        return -errno;
    }
}

static int write_str(const char *path, char *value)
{
    int fd = open(path, O_WRONLY);

    if (fd >= 0) {
        char buffer[1024];
        int bytes = snprintf(buffer, sizeof(buffer), "%s\n", value);
        ssize_t amt = write(fd, buffer, (size_t)bytes);
        close(fd);
        return amt == -1 ? -errno : 0;
    } else {
        ALOGE("write_str failed to open %s\n", path);
        return -errno;
    }
}

static int is_lit(const struct light_state_t *state)
{
    return state->color & 0x00ffffff;
}

static int rgb_to_brightness(const struct light_state_t *state)
{
    // frameworks sends brightness as rgb grayscale
    return state->color & 0x000000ff;
}

static char *get_scaled_duty_pcts(int brightness)
{
    char *buf = calloc(RAMP_SIZE, 5 * sizeof(char));
    char *pad = "";
    int i = 0;

    if (!buf)
        return NULL;

    for (i = 0; i < RAMP_SIZE; i++) {
        char temp[5];
        snprintf(temp, sizeof(temp), "%s%d", pad, (BRIGHTNESS_RAMP[i] * brightness / 255));
        strcat(buf, temp);
        pad = ",";
    }
    ALOGV("%s: brightness=%d duty=%s", __func__, brightness, buf);

    return buf;
}

static int set_speaker_light_locked(struct light_device_t *dev,
        const struct light_state_t *state)
{
    int red, green, blue, blink;
    int onMS, offMS, stepDuration, pauseHi;
    unsigned int colorRGB;
    char *duty;

    if(!dev) {
        return -1;
    }

    switch (state->flashMode) {
        case LIGHT_FLASH_TIMED:
            onMS = state->flashOnMS;
            offMS = state->flashOffMS;
            break;
        case LIGHT_FLASH_NONE:
        default:
            onMS = 0;
            offMS = 0;
            break;
    }

    colorRGB = state->color;

    ALOGV("set_speaker_light_locked mode %d, colorRGB=%08X, onMS=%d, offMS=%d\n",
            state->flashMode, colorRGB, onMS, offMS);

    red = (colorRGB >> 16) & 0xFF;
    green = (colorRGB >> 8) & 0xFF;
    blue = colorRGB & 0xFF;
    blink = onMS > 0 && offMS > 0;

    // disable all blinking to start
    write_int(RED_BLINK_FILE, 0);
    write_int(GREEN_BLINK_FILE, 0);
    write_int(BLUE_BLINK_FILE, 0);

    if (blink) {
        stepDuration = RAMP_STEP_DURATION;
        pauseHi = onMS - (stepDuration * RAMP_SIZE * 2);
        if (stepDuration * RAMP_SIZE * 2 > onMS) {
            stepDuration = onMS / (RAMP_SIZE * 2);
            pauseHi = 0;
        }

        // red
        write_int(RED_START_IDX_FILE, 0);
        duty = get_scaled_duty_pcts(red);    
        write_str(RED_DUTY_PCTS_FILE, duty);
        write_int(RED_PAUSE_LO_FILE, offMS);
        // The led driver is configured to ramp up then ramp
        // down the lut. This effectively doubles the ramp duration.
        write_int(RED_PAUSE_HI_FILE, pauseHi);
        write_int(RED_RAMP_STEP_MS_FILE, stepDuration);
        free(duty);

        // green
        write_int(GREEN_START_IDX_FILE, RAMP_SIZE);
        duty = get_scaled_duty_pcts(green);
        write_str(GREEN_DUTY_PCTS_FILE, duty);
        write_int(GREEN_PAUSE_LO_FILE, offMS);
        // The led driver is configured to ramp up then ramp
        // down the lut. This effectively doubles the ramp duration.
        write_int(GREEN_PAUSE_HI_FILE, pauseHi);
        write_int(GREEN_RAMP_STEP_MS_FILE, stepDuration);
        free(duty);

        // blue
        write_int(BLUE_START_IDX_FILE, RAMP_SIZE * 2);
        duty = get_scaled_duty_pcts(blue);
        write_str(BLUE_DUTY_PCTS_FILE, duty);
        write_int(BLUE_PAUSE_LO_FILE, offMS);
        // The led driver is configured to ramp up then ramp
        // down the lut. This effectively doubles the ramp duration.
        write_int(BLUE_PAUSE_HI_FILE, pauseHi);
        write_int(BLUE_RAMP_STEP_MS_FILE, stepDuration);
        free(duty);

        // start the party
        write_int(RED_BLINK_FILE, 1);
        write_int(GREEN_BLINK_FILE, 1);
        write_int(BLUE_BLINK_FILE, 1);

    } else {
        write_int(RED_LED_FILE, red);
        write_int(GREEN_LED_FILE, green);
        write_int(BLUE_LED_FILE, blue);
    }


    return 0;
}

static void handle_speaker_light_locked(struct light_device_t *dev)
{
    if (is_lit(&g_attention)) {
        set_speaker_light_locked(dev, &g_attention);
    } else if (is_lit(&g_notification)) {
        set_speaker_light_locked(dev, &g_notification);
    } else {
        set_speaker_light_locked(dev, &g_battery);
    }
}

static int set_light_backlight(struct light_device_t *dev,
        const struct light_state_t *state)
{
    int err = 0;
    int brightness = rgb_to_brightness(state);

    if (!dev)
        return -ENODEV;

    pthread_mutex_lock(&g_lock);

    err = write_int(LCD_FILE, brightness);
    
    int fd = open(KEYBOARD_INFO_FILE, O_RDONLY);
    if (fd >= 0)
    {
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));
        read(fd, buffer, sizeof(buffer));

        bool is_closed = strstr(buffer, "slider_state=0") != NULL;
        err = write_int(KEYBOARD_FILE, is_closed ? 0 : brightness);

        close(fd);
    }
    else
        ALOGE("set_light_backlight failed to open info %d\n", fd);

    pthread_mutex_unlock(&g_lock);

    return err;
}

static int set_light_keyboard(struct light_device_t *dev,
        const struct light_state_t *state)
{
    int err = 0;
    int brightness = rgb_to_brightness(state);

    if (!dev)
        return -ENODEV;

    pthread_mutex_lock(&g_lock);

    err = write_int(KEYBOARD_FILE, brightness);

    pthread_mutex_unlock(&g_lock);

    return err;
}

static int set_light_battery(struct light_device_t *dev,
        const struct light_state_t *state)
{
    pthread_mutex_lock(&g_lock);

    g_battery = *state;
    handle_speaker_light_locked(dev);

    pthread_mutex_unlock(&g_lock);

    return 0;
}

static int set_light_notifications(struct light_device_t *dev,
        const struct light_state_t *state)
{
    pthread_mutex_lock(&g_lock);

    g_notification = *state;
    handle_speaker_light_locked(dev);

    pthread_mutex_unlock(&g_lock);

    return 0;
}

static int set_light_attention(struct light_device_t *dev,
        const struct light_state_t *state)
{
    pthread_mutex_lock(&g_lock);

    g_attention = *state;
    handle_speaker_light_locked(dev);

    pthread_mutex_unlock(&g_lock);

    return 0;
}

static int close_lights(struct hw_device_t *dev)
{
    if (dev)
        free(dev);

    return 0;
}

static int open_lights(const struct hw_module_t *module, const char *name,
        struct hw_device_t **device)
{
    int (*set_light)(struct light_device_t *dev,
            const struct light_state_t *state);

    if (!strcmp(LIGHT_ID_BACKLIGHT, name))
        set_light = set_light_backlight;
    else if (!strcmp(LIGHT_ID_KEYBOARD, name))
        set_light = set_light_keyboard;
    else if (!strcmp(LIGHT_ID_BATTERY, name))
        set_light = set_light_battery;
    else if (!strcmp(LIGHT_ID_NOTIFICATIONS, name))
        set_light = set_light_notifications;
    else if (!strcmp(LIGHT_ID_ATTENTION, name))
        set_light = set_light_attention;
    else
        return -EINVAL;

    struct light_device_t *dev = calloc(1, sizeof(struct light_device_t));

    if (!dev)
        return -ENOMEM;

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (struct hw_module_t*)module;
    dev->common.close = (int (*)(struct hw_device_t*))close_lights;
    dev->set_light = set_light;

    *device = (struct hw_device_t*)dev;

    return 0;
}

static struct hw_module_methods_t lights_module_methods = {
    .open =  open_lights,
};

struct hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .module_api_version = 1,
    .hal_api_version = HARDWARE_HAL_API_VERSION,
    .id = LIGHTS_HARDWARE_MODULE_ID,
    .name = "Venice Lights HAL",
    .author = "The LineageOS Project",
    .methods = &lights_module_methods,
};

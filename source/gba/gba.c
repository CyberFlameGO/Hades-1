/******************************************************************************\
**
**  This file is part of the Hades GBA Emulator, and is made available under
**  the terms of the GNU General Public License version 2.
**
**  Copyright (C) 2021-2022 - The Hades Authors
**
\******************************************************************************/

#include <string.h>
#include "hades.h"
#include "gba/core/arm.h"
#include "gba/core/thumb.h"
#include "gba/gba.h"
#include "gba/db.h"
#include "utils/time.h"

/*
** Initialize the `gba` structure with sane, default values.
*/
void
gba_init(
    struct gba *gba
) {
    memset(gba, 0, sizeof(*gba));

    /* Initialize the ARM decoder */
    core_arm_decode_insns();
    core_thumb_decode_insns();

    pthread_mutex_init(&gba->message_queue.lock, NULL);
}

/*
** Reset the GBA system to its initial state.
*/
void
gba_reset(
    struct gba *gba
) {
    sched_cleanup(gba);

    sched_init(gba);
    mem_reset(&gba->memory);
    io_init(&gba->io);
    ppu_init(gba);
    apu_init(gba);
    core_init(gba);
    gpio_init(gba);
    gba->started = false;
}

/*
** Run the emulator, consuming messages that dictate what the emulator should do.
**
** Messages are used as a mono-directional communication between the frontend and the emulator.
**
** Those messages can be:
**   - A new key was pressed
**   - The user requested a quickload/quicksave
**   - The emulator must run until the next frame, for one instruction, etc.
**   - The emulator must pause, reset, etc.
*/
void
gba_run(
    struct gba *gba
) {
    uint64_t last_measured_time;
    uint64_t accumulated_time;
    uint64_t time_per_frame;

    last_measured_time = hs_tick_count();
    accumulated_time = 0;
    time_per_frame = 0;
    while (true) {
        struct message_queue *mqueue;
        struct message *message;

        pthread_mutex_lock(&gba->message_queue.lock);

        mqueue = &gba->message_queue;
        message = mqueue->messages;
        while (mqueue->length) {
            switch (message->type) {
                case MESSAGE_EXIT: {
                    pthread_mutex_unlock(&gba->message_queue.lock);
                    return ;
                };
                case MESSAGE_LOAD_BIOS: {
                    struct message_data *message_data;

                    message_data = (struct message_data *)message;
                    memset(gba->memory.bios, 0, BIOS_MASK);
                    memcpy(gba->memory.bios, message_data->data, min(message_data->size, BIOS_MASK));
                    if (message_data->cleanup) {
                        message_data->cleanup(message_data->data);
                    }
                    break;
                };
                case MESSAGE_LOAD_ROM: {
                    struct message_data *message_data;

                    message_data = (struct message_data *)message;
                    memset(gba->memory.rom, 0, CART_SIZE);
                    gba->memory.rom_size = min(message_data->size, CART_SIZE);
                    memcpy(gba->memory.rom, message_data->data, gba->memory.rom_size);
                    if (message_data->cleanup) {
                        message_data->cleanup(message_data->data);
                    }
                    db_lookup_game(gba);
                    break;
                };
                case MESSAGE_LOAD_BACKUP: {
                    struct message_data *message_data;

                    message_data = (struct message_data *)message;
                    memset(gba->memory.backup_storage_data, 0, backup_storage_sizes[gba->memory.backup_storage_type]);
                    memcpy(
                        gba->memory.backup_storage_data,
                        message_data->data,
                        min(message_data->size, backup_storage_sizes[gba->memory.backup_storage_type])
                    );
                    if (message_data->cleanup) {
                        message_data->cleanup(message_data->data);
                    }
                    break;
                };
                case MESSAGE_BACKUP_TYPE: {
                    struct message_backup_type *message_backup_type;

                    /* Ignore if emulation is already started. */
                    if (gba->started) {
                        break;
                    }

                    message_backup_type = (struct message_backup_type *)message;
                    if (message_backup_type->type == BACKUP_AUTO_DETECT) {
                        mem_backup_storage_detect(gba);
                    } else {
                        gba->memory.backup_storage_type = message_backup_type->type;
                        gba->memory.backup_storage_source = BACKUP_SOURCE_MANUAL;
                    }
                    mem_backup_storage_init(gba);
                    break;
                };
                case MESSAGE_RESET: {
                    gba_reset(gba);
                    break;
                };
                case MESSAGE_RUN: {
                    struct message_run *message_run;

                    message_run = (struct message_run *)message;
                    gba->started = true;
                    gba->state = GBA_STATE_RUN;
                    gba->speed = message_run->speed;
                    if (message_run->speed) {
                        time_per_frame = 1.f/59.737f * 1000.f * 1000.f / (float)gba->speed;
                        accumulated_time = 0;
                    } else {
                        time_per_frame = 0.f;
                    }
                    break;
                };
                case MESSAGE_PAUSE: {
                    gba->state = GBA_STATE_PAUSE;
                    break;
                };
                case MESSAGE_KEYINPUT: {
                    struct message_keyinput *message_keyinput;

                    message_keyinput = (struct message_keyinput *)message;
                    switch (message_keyinput->key) {
                        case KEY_A:         gba->io.keyinput.a = !message_keyinput->pressed; break;
                        case KEY_B:         gba->io.keyinput.b = !message_keyinput->pressed; break;
                        case KEY_L:         gba->io.keyinput.l = !message_keyinput->pressed; break;
                        case KEY_R:         gba->io.keyinput.r = !message_keyinput->pressed; break;
                        case KEY_UP:        gba->io.keyinput.up = !message_keyinput->pressed; break;
                        case KEY_DOWN:      gba->io.keyinput.down = !message_keyinput->pressed; break;
                        case KEY_RIGHT:     gba->io.keyinput.right = !message_keyinput->pressed; break;
                        case KEY_LEFT:      gba->io.keyinput.left = !message_keyinput->pressed; break;
                        case KEY_START:     gba->io.keyinput.start = !message_keyinput->pressed; break;
                        case KEY_SELECT:    gba->io.keyinput.select = !message_keyinput->pressed; break;
                    };

                    io_scan_keypad_irq(gba);
                    break;
                };
                case MESSAGE_QUICKLOAD: {
                    struct message_data *message_data;

                    message_data = (struct message_data *)message;
                    quickload(gba, (char const *)message_data->data);
                    if (message_data->cleanup) {
                        message_data->cleanup(message_data->data);
                    }
                    break;
                };
                case MESSAGE_QUICKSAVE: {
                    struct message_data *message_data;

                    message_data = (struct message_data *)message;
                    quicksave(gba, (char const *)message_data->data);
                    if (message_data->cleanup) {
                        message_data->cleanup(message_data->data);
                    }
                    break;
                };
                case MESSAGE_AUDIO_RESAMPLE_FREQ: {
                    struct message_audio_freq *message_audio_freq;

                    message_audio_freq = (struct message_audio_freq *)message;
                    gba->apu.resample_frequency = message_audio_freq->refill_frequency;
                    break;
                };
                case MESSAGE_COLOR_CORRECTION: {
                    struct message_color_correction *message_color_correction;

                    message_color_correction = (struct message_color_correction *)message;
                    gba->color_correction = message_color_correction->color_correction;
                    break;
                };
                case MESSAGE_RTC: {
                    struct message_device_state *message_device_state;

                    /* Ignore if emulation is already started. */
                    if (gba->started) {
                        break;
                    }

                    message_device_state = (struct message_device_state *)message;
                    switch (message_device_state->state) {
                        case DEVICE_AUTO_DETECT: {
                            gba->rtc_auto_detect = true;
                            gba->rtc_enabled = false;
                            break;
                        };
                        case DEVICE_ENABLED: {
                            gba->rtc_auto_detect = false;
                            gba->rtc_enabled = true;
                            break;
                        };
                        case DEVICE_DISABLED: {
                            gba->rtc_auto_detect = false;
                            gba->rtc_enabled = false;
                            break;
                        };
                    }
                    break;
                };
            }
            mqueue->allocated_size -= message->size;
            --mqueue->length;
            message = (struct message *)((uint8_t *)message + message->size);
        }
        free(mqueue->messages);
        mqueue->messages = NULL;

        pthread_mutex_unlock(&gba->message_queue.lock);

        if (gba->state == GBA_STATE_RUN) {
            sched_run_for(gba, CYCLES_PER_FRAME);
        }

        /* Limit FPS */
        if (gba->speed) {
            uint64_t now;

            now = hs_tick_count();
            accumulated_time += now - last_measured_time;
            last_measured_time = now;

            if (accumulated_time < time_per_frame) {
                hs_usleep(time_per_frame - accumulated_time);
                now = hs_tick_count();
                accumulated_time += now - last_measured_time;
                last_measured_time = now;
            }
            accumulated_time -= time_per_frame;
        } else {
            last_measured_time = hs_tick_count();
            accumulated_time = 0;
        }
    }
}

/*
** Put the given message in the message queue.
*/
void
gba_message_push(
    struct gba *gba,
    struct message *message
) {
    size_t new_size;
    struct message_queue *mqueue;

    mqueue = &gba->message_queue;
    pthread_mutex_lock(&gba->message_queue.lock);

    new_size = mqueue->allocated_size + message->size;

    mqueue->messages = realloc(mqueue->messages, new_size);
    hs_assert(mqueue->messages);
    memcpy((uint8_t *)mqueue->messages + mqueue->allocated_size, message, message->size);

    mqueue->length += 1;
    mqueue->allocated_size = new_size;

    pthread_mutex_unlock(&gba->message_queue.lock);
}
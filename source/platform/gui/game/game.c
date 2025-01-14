/******************************************************************************\
**
**  This file is part of the Hades GBA Emulator, and is made available under
**  the terms of the GNU General Public License version 2.
**
**  Copyright (C) 2021-2022 - The Hades Authors
**
\******************************************************************************/

#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include "hades.h"
#include "platform/gui.h"
#include "gba/gba.h"

static
bool
load_bios(
    struct app *app
) {
    FILE *file;
    void *data;
    char *error_msg;

    file = fopen(app->emulation.bios_path, "rb");
    if (!file) {
        hs_assert(-1 != asprintf(
            &error_msg,
            "failed to open %s: %s.\n\nPlease download and select a valid Nintendo GBA Bios using \"File\" -> \"Open BIOS\".",
            app->emulation.bios_path,
            strerror(errno)
        ));
        gui_new_error(app, error_msg);
        return (true);
    }

    fseek(file, 0, SEEK_END);
    if (ftell(file) != 0x4000) {
        error_msg = strdup("the BIOS is invalid.");
        gui_new_error(app, error_msg);
        return (true);
    }

    rewind(file);

    data = calloc(1, BIOS_SIZE);
    hs_assert(data);

    if (fread(data, 1, BIOS_SIZE, file) != BIOS_SIZE) {
        hs_assert(-1 != asprintf(
            &error_msg,
            "failed to read %s: %s.",
            app->emulation.bios_path,
            strerror(errno)
        ));
        gui_new_error(app, error_msg);
        free(data);
        return (true);
    }

    gba_message_push(app->emulation.gba, NEW_MESSAGE_LOAD_BIOS(data, free));

    return (false);
}

static
bool
load_rom(
    struct app *app
) {
    FILE *file;
    size_t file_len;
    void *data;
    char *error_msg;
    enum device_state rtc_state;

    file = fopen(app->emulation.game_path, "rb");
    if (!file) {
        hs_assert(-1 != asprintf(
            &error_msg,
            "failed to open %s: %s.",
            app->emulation.game_path,
            strerror(errno)
        ));
        gui_new_error(app, error_msg);
        return (true);
    }

    fseek(file, 0, SEEK_END);
    file_len = ftell(file);
    if (file_len > CART_SIZE || file_len < 192) {
        error_msg = strdup("the ROM is invalid.");
        gui_new_error(app, error_msg);
        return (true);
    }

    rewind(file);

    data = calloc(1, file_len);
    hs_assert(data);

    if (fread(data, 1, file_len, file) != file_len) {
        hs_assert(-1 != asprintf(
            &error_msg,
            "failed to read %s: %s.",
            app->emulation.game_path,
            strerror(errno)
        ));
        gui_new_error(app, error_msg);
        free(data);
        return (true);
    }

    if (app->emulation.rtc_autodetect) {
        rtc_state = DEVICE_AUTO_DETECT;
    } else {
        rtc_state = app->emulation.rtc_enabled ? DEVICE_ENABLED : DEVICE_DISABLED;
    }

    gba_message_push(app->emulation.gba, NEW_MESSAGE_LOAD_ROM(data, file_len, free));
    gba_message_push(app->emulation.gba, NEW_MESSAGE_BACKUP_TYPE(app->emulation.backup_type));
    gba_message_push(app->emulation.gba, NEW_MESSAGE_RTC(rtc_state));

    return (false);
}

static
bool
load_save(
    struct app *app
) {
    size_t file_len;
    char *error_msg;

    if (app->emulation.backup_file) {
        fclose(app->emulation.backup_file);
    }

    app->emulation.backup_file = fopen(app->emulation.backup_path, "rb+");
    if (app->emulation.backup_file) {
        void *data;
        size_t read_len;

        fseek(app->emulation.backup_file, 0, SEEK_END);
        file_len = ftell(app->emulation.backup_file);
        rewind(app->emulation.backup_file);

        data = calloc(1, file_len);
        hs_assert(data);
        read_len = fread(data, 1, file_len, app->emulation.backup_file);

        if (read_len != file_len) {
            logln(HS_WARNING, "Failed to read the save file. Is it corrupted?");
        } else {
            logln(HS_GLOBAL, "Save data successfully loaded.");
            gba_message_push(app->emulation.gba, NEW_MESSAGE_LOAD_BACKUP(data, file_len, free));
        }
    } else {
        logln(HS_WARNING, "Failed to open the save file. A new one is created instead.");

        app->emulation.backup_file = fopen(app->emulation.backup_path, "wb+");

        if (!app->emulation.backup_file) {
            hs_assert(-1 != asprintf(
                &error_msg,
                "failed to create %s: %s.",
                app->emulation.backup_path,
                strerror(errno)
            ));
            gui_new_error(app, error_msg);
            return (true);
        }
    }
    return (false);
}

/*
** Stop the emulation and return to a neutral state.
*/
void
gui_game_stop(
    struct app *app
) {
    app->emulation.enabled = false;
    app->emulation.pause = true;
    gba_message_push(app->emulation.gba, NEW_MESSAGE_PAUSE());
    gba_message_push(app->emulation.gba, NEW_MESSAGE_RESET());
}


/*
** Load the BIOS/ROM into the emulator's memory and reset it.
**
** This function also sets the `qsave_path` and `backup_path` variables of `app.emulation`
** depending on the content of `app.emulation.game_path`.
*/
void
gui_game_reset(
    struct app *app
) {
    char *extension;
    size_t base_len;

    gui_push_recent_roms(app);

    free(app->emulation.qsave_path);
    free(app->emulation.backup_path);

    extension = strrchr(app->emulation.game_path, '.');

    if (extension) {
        base_len = extension - app->emulation.game_path;
    } else {
        base_len = strlen(app->emulation.game_path);
    }

    hs_assert(-1 != asprintf(
        &app->emulation.qsave_path,
        "%.*s.hds",
        (int)base_len,
        app->emulation.game_path
    ));

    hs_assert(-1 != asprintf(
        &app->emulation.backup_path,
        "%.*s.sav",
        (int)base_len,
        app->emulation.game_path
    ));

    gba_message_push(app->emulation.gba, NEW_MESSAGE_PAUSE());
    gba_message_push(app->emulation.gba, NEW_MESSAGE_RESET());
    if (!load_bios(app) && !load_rom(app) && !load_save(app)) {
        app->emulation.enabled = true;
        app->emulation.pause = false;
        gba_message_push(app->emulation.gba, NEW_MESSAGE_RESET());
        gui_game_run(app);
    } else {
        app->emulation.enabled = false;
    }
}

/*
** Write the content of the backup storage on the disk.
*/
void
gui_game_write_backup(
    struct app *app
) {
    if (   app->emulation.backup_file
        && app->emulation.gba->memory.backup_storage_data
        && app->emulation.gba->memory.backup_storage_dirty
    ) {
        fseek(app->emulation.backup_file, 0, SEEK_SET);
        fwrite(
            app->emulation.gba->memory.backup_storage_data,
            backup_storage_sizes[app->emulation.gba->memory.backup_storage_type],
            1,
            app->emulation.backup_file
        );
    }
    app->emulation.gba->memory.backup_storage_dirty = false;
}

void
gui_game_run(
    struct app *app
) {
    gba_message_push(app->emulation.gba, NEW_MESSAGE_RUN(app->emulation.speed * !app->emulation.unbounded));
}

void
gui_game_pause(
    struct app *app
) {
    gba_message_push(app->emulation.gba, NEW_MESSAGE_PAUSE());
}

void
gui_game_quicksave(
    struct app *app
) {
    gba_message_push(app->emulation.gba, NEW_MESSAGE_QUICKSAVE(app->emulation.qsave_path));
}

void
gui_game_quickload(
    struct app *app
) {
    gba_message_push(app->emulation.gba, NEW_MESSAGE_QUICKLOAD(app->emulation.qsave_path));
}

void
gui_game_set_audio_settings(
    struct app *app,
    uint64_t resample_freq
) {
    gba_message_push(app->emulation.gba, NEW_MESSAGE_AUDIO_RESAMPLE_FREQ(resample_freq));
}

void
gui_game_set_color_correction(
    struct app *app
) {
    gba_message_push(app->emulation.gba, NEW_MESSAGE_COLOR_CORRECTION(app->emulation.color_correction));
}

void
gui_game_set_backup_type(
    struct app *app
) {
    gba_message_push(app->emulation.gba, NEW_MESSAGE_BACKUP_TYPE(app->emulation.backup_type));
}

void
gui_game_handle_events(
    struct app *app,
    SDL_Event *event
) {
    switch (event->type) {
        case SDL_KEYDOWN: {

            /* Ignore repeat keys */
            if (event->key.repeat) {
                break;
            }

            switch (event->key.keysym.sym) {
                case SDLK_UP:
                case SDLK_w:                gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_UP, true)); break;
                case SDLK_DOWN:
                case SDLK_s:                gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_DOWN, true)); break;
                case SDLK_LEFT:
                case SDLK_a:                gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_LEFT, true)); break;
                case SDLK_RIGHT:
                case SDLK_d:                gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_RIGHT, true)); break;
                case SDLK_p:                gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_A, true)); break;
                case SDLK_l:                gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_B, true)); break;
                case SDLK_e:                gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_L, true)); break;
                case SDLK_o:                gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_R, true)); break;
                case SDLK_BACKSPACE:        gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_SELECT, true)); break;
                case SDLK_RETURN:           gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_START, true)); break;
            }
            break;
        };
        case SDL_KEYUP: {

            /* Ignore repeat keys */
            if (event->key.repeat) {
                break;
            }

            switch (event->key.keysym.sym) {
                case SDLK_UP:
                case SDLK_w:                gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_UP, false)); break;
                case SDLK_DOWN:
                case SDLK_s:                gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_DOWN, false)); break;
                case SDLK_LEFT:
                case SDLK_a:                gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_LEFT, false)); break;
                case SDLK_RIGHT:
                case SDLK_d:                gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_RIGHT, false)); break;
                case SDLK_p:                gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_A, false)); break;
                case SDLK_l:                gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_B, false)); break;
                case SDLK_e:                gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_L, false)); break;
                case SDLK_o:                gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_R, false)); break;
                case SDLK_BACKSPACE:        gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_SELECT, false)); break;
                case SDLK_RETURN:           gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_START, false)); break;
                case SDLK_F1: {
                    app->emulation.unbounded ^= 1;
                    gba_message_push(app->emulation.gba, NEW_MESSAGE_RUN(app->emulation.speed * !app->emulation.unbounded));
                    break;
                };
                case SDLK_F2:               gui_game_screenshot(app); break;
                case SDLK_F5:               gui_game_quicksave(app); break;
                case SDLK_F8:               gui_game_quickload(app); break;
                default:
                    break;
            }
            break;
        };
        case SDL_CONTROLLERBUTTONDOWN: {
            switch (event->cbutton.button) {
                case SDL_CONTROLLER_BUTTON_B:               gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_B, true)); break;
                case SDL_CONTROLLER_BUTTON_A:               gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_A, true)); break;
                case SDL_CONTROLLER_BUTTON_Y:               gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_A, true)); break;
                case SDL_CONTROLLER_BUTTON_X:               gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_B, true)); break;
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:       gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_LEFT, true)); break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:      gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_RIGHT, true)); break;
                case SDL_CONTROLLER_BUTTON_DPAD_UP:         gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_UP, true)); break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:       gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_DOWN, true)); break;
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:    gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_L, true)); break;
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:   gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_R, true)); break;
                case SDL_CONTROLLER_BUTTON_START:           gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_START, true)); break;
                case SDL_CONTROLLER_BUTTON_BACK:            gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_SELECT, true)); break;
            }
            break;
        };
        case SDL_CONTROLLERBUTTONUP: {
            switch (event->cbutton.button) {
                case SDL_CONTROLLER_BUTTON_B:               gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_B, false)); break;
                case SDL_CONTROLLER_BUTTON_A:               gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_A, false)); break;
                case SDL_CONTROLLER_BUTTON_Y:               gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_A, false)); break;
                case SDL_CONTROLLER_BUTTON_X:               gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_B, false)); break;
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:       gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_LEFT, false)); break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:      gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_RIGHT, false)); break;
                case SDL_CONTROLLER_BUTTON_DPAD_UP:         gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_UP, false)); break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:       gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_DOWN, false)); break;
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:    gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_L, false)); break;
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:   gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_R, false)); break;
                case SDL_CONTROLLER_BUTTON_START:           gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_START, false)); break;
                case SDL_CONTROLLER_BUTTON_BACK:            gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_SELECT, false)); break;
#if SDL_VERSION_ATLEAST(2, 0, 14)
                case SDL_CONTROLLER_BUTTON_MISC1:           gui_game_screenshot(app); break;
#endif
            }
            break;
        };
        case SDL_CONTROLLERAXISMOTION: {
            bool state_a;
            bool state_b;

            state_a = (event->jaxis.value >= INT16_MAX / 2);  // At least 50% of the axis
            state_b = (event->jaxis.value <= INT16_MIN / 2);
            if (event->jaxis.axis == 0 && state_a != app->joystick_right) {
                gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_RIGHT, state_a));
                app->joystick_right = state_a;
            } else if (event->jaxis.axis == 0 && state_b != app->joystick_left) {
                gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_LEFT, state_b));
                app->joystick_left = state_b;
            } else if (event->jaxis.axis == 1 && state_a != app->joystick_down) {
                gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_DOWN, state_a));
                app->joystick_down = state_a;
            } else if (event->jaxis.axis == 1 && state_b != app->joystick_up) {
                gba_message_push(app->emulation.gba, NEW_MESSAGE_KEYINPUT(KEY_UP, state_b));
                app->joystick_up = state_b;
            }
            break;
        }
    }
}

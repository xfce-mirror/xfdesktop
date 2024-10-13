/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2024 Brian Tarricone, <brian@tarricone.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <glib.h>
#include <gio/gio.h>
#include <gdk/gdk.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4windowing/libxfce4windowing.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <yaml.h>

#include "xfdesktop-icon-position-configs.h"

#define SAVE_DELAY_S 1

#ifdef DEBUG
static char spaces[512];
#define LOG_EVENT(level, msg) DBG("%.*s-- %s", level * 4, spaces, msg)
#define LOG_EVENT_IN(level, msg) \
    G_STMT_START { \
        DBG("%.*s--> %s", level * 4, spaces, msg); \
        level++; \
    } \
G_STMT_END
#define LOG_EVENT_OUT(level, msg) \
    G_STMT_START { \
        DBG("%.*s<-- %s", level * 4, spaces, msg); \
        level--; \
    } \
G_STMT_END
#define STATE_TRANSITION(level, lhs, new_state) \
    G_STMT_START { \
        ParserState _new_state = (new_state); \
        DBG("%.*s%s -> %s", level * 4, spaces, state_str(lhs), state_str(_new_state)); \
        lhs = _new_state; \
    } \
G_STMT_END
#else
#define LOG_EVENT(...) G_STMT_START{}G_STMT_END
#define LOG_EVENT_IN(...) G_STMT_START{}G_STMT_END
#define LOG_EVENT_OUT(...) G_STMT_START{}G_STMT_END
#define STATE_TRANSITION(level, lhs, new_state) G_STMT_START{ lhs = new_state; }G_STMT_END
#endif

#define PARSE_ERROR(level, state_lhs, configs, event, fmt, ...) \
    G_STMT_START { \
        g_set_error(error, \
                    G_IO_ERROR, \
                    G_IO_ERROR_INVALID_DATA, \
                    "%s:%" G_GSIZE_FORMAT ":%" G_GSIZE_FORMAT ": " fmt, \
                    g_file_peek_path(configs->file), \
                    event.start_mark.line, \
                    event.start_mark.column __VA_OPT__(, ) \
                    __VA_ARGS__); \
        STATE_TRANSITION(level, state_lhs, PARSER_ERROR); \
    } \
G_STMT_END

#define EMITTER_ERROR(emitter, event)

struct _XfdesktopIconPositionConfigs {
    GFile *file;
    GList *configs;  // XfdesktopIconPositionConfig (owner)
    GHashTable *config_to_monitor;  // XfdesktopIconPositionConfig -> XfwMonitor

    guint scheduled_save_id;
};

struct _XfdesktopIconPositionConfig {
    XfdesktopIconPositionLevel level;
    GHashTable *monitors;  // string id (owner) -> XfdesktopIconPositionMonitor (owner)
    GHashTable *icon_positions;  // string id (owner) -> XfdesktopIconPosition (owner)
};

typedef struct _XfdesktopIconPositionMonitor {
    gchar *display_name;
    GdkRectangle geometry;
} XfdesktopIconPositionMonitor;

typedef struct _XfdesktopIconPosition {
    guint row;
    guint col;
    guint64 last_seen;
} XfdesktopIconPosition;

typedef enum {
    PARSER_TOP,
    PARSER_TOPLEVEL_MAP,
    PARSER_CONFIGS,
    PARSER_CONFIGS_ARRAY,
    PARSER_CONFIG_MAP,
    PARSER_CONFIG_LEVEL,
    PARSER_CONFIG_MONITORS,
    PARSER_CONFIG_MONITORS_ARRAY,
    PARSER_CONFIG_MONITOR_MAP,
    PARSER_CONFIG_MONITOR_ID,
    PARSER_CONFIG_MONITOR_DISPLAY_NAME,
    PARSER_CONFIG_MONITOR_GEOMETRY,
    PARSER_CONFIG_MONITOR_GEOMETRY_MAP,
    PARSER_CONFIG_MONITOR_GEOMETRY_MAP_X,
    PARSER_CONFIG_MONITOR_GEOMETRY_MAP_Y,
    PARSER_CONFIG_MONITOR_GEOMETRY_MAP_WIDTH,
    PARSER_CONFIG_MONITOR_GEOMETRY_MAP_HEIGHT,
    PARSER_CONFIG_ICONS,
    PARSER_CONFIG_ICONS_MAP,
    PARSER_CONFIG_ICON,
    PARSER_CONFIG_ICON_MAP,
    PARSER_CONFIG_ICON_MAP_ROW,
    PARSER_CONFIG_ICON_MAP_COL,
    PARSER_CONFIG_ICON_MAP_LAST_SEEN,
    PARSER_DONE,
    PARSER_ERROR,
} ParserState;

#ifdef DEBUG
static const gchar *
state_str(ParserState state) {
#define S(val) case val: \
    return #val;

    switch (state) {
        S(PARSER_TOP)
        S(PARSER_TOPLEVEL_MAP)
        S(PARSER_CONFIGS)
        S(PARSER_CONFIGS_ARRAY)
        S(PARSER_CONFIG_MAP)
        S(PARSER_CONFIG_LEVEL)
        S(PARSER_CONFIG_MONITORS)
        S(PARSER_CONFIG_MONITORS_ARRAY)
        S(PARSER_CONFIG_MONITOR_MAP)
        S(PARSER_CONFIG_MONITOR_ID)
        S(PARSER_CONFIG_MONITOR_DISPLAY_NAME)
        S(PARSER_CONFIG_MONITOR_GEOMETRY)
        S(PARSER_CONFIG_MONITOR_GEOMETRY_MAP)
        S(PARSER_CONFIG_MONITOR_GEOMETRY_MAP_X)
        S(PARSER_CONFIG_MONITOR_GEOMETRY_MAP_Y)
        S(PARSER_CONFIG_MONITOR_GEOMETRY_MAP_WIDTH)
        S(PARSER_CONFIG_MONITOR_GEOMETRY_MAP_HEIGHT)
        S(PARSER_CONFIG_ICONS)
        S(PARSER_CONFIG_ICONS_MAP)
        S(PARSER_CONFIG_ICON)
        S(PARSER_CONFIG_ICON_MAP)
        S(PARSER_CONFIG_ICON_MAP_ROW)
        S(PARSER_CONFIG_ICON_MAP_COL)
        S(PARSER_CONFIG_ICON_MAP_LAST_SEEN)
        S(PARSER_DONE)
        S(PARSER_ERROR)

        default:
            g_assert_not_reached();
            return "INVALID";
    }

#undef S
}
#endif

static const gchar *
libyaml_strerror(yaml_error_type_t error) {
    switch (error) {
        case YAML_NO_ERROR:
            return "Success";
        case YAML_MEMORY_ERROR:
            g_error("Out of memory allocating for YAML parser/emitter");
            abort();
            return "Out of memory";
        case YAML_READER_ERROR:
            return "Cannot read or decode YAML input";
        case YAML_SCANNER_ERROR:
            return "Cannot scan YAML input";
        case YAML_PARSER_ERROR:
            return "Cannot parse YAML input stream";
        case YAML_COMPOSER_ERROR:
            return "Cannot compose YAML document";
        case YAML_WRITER_ERROR:
            return "Cannot write YAML ouptut";
        case YAML_EMITTER_ERROR:
            return "Cannot emit YAML stream";
        default:
            return "Unknown YAML error";
    }
}

static gint
compare_configs_by_level(gconstpointer a, gconstpointer b) {
    const XfdesktopIconPositionConfig *ca = a;
    const XfdesktopIconPositionConfig *cb = b;
    // Sort descending; higher level should be sooner in the list
    return cb->level - ca->level;
}

static gint
compare_candidates(gconstpointer a, gconstpointer b, gpointer data) {
    const XfdesktopIconPositionConfig *ca = a;
    const XfdesktopIconPositionConfig *cb = b;
    XfdesktopIconPositionLevel desired_level = GPOINTER_TO_INT(data);
    if ((ca->level == desired_level && cb->level == desired_level) || ca->level == cb->level) {
        return 0;
    } else if (ca->level == desired_level) {
        return -10;
    } else if (cb->level == desired_level) {
        return 10;
    } else {
        return ABS(ca->level - desired_level) - ABS(cb->level - desired_level);
    }
}

static void
xfdesktop_icon_position_monitor_free(XfdesktopIconPositionMonitor *pos_monitor) {
    if (pos_monitor != NULL) {
        g_free(pos_monitor->display_name);
        g_free(pos_monitor);
    }
}

static XfdesktopIconPositionConfig *
xfdesktop_icon_position_config_new_internal(XfdesktopIconPositionLevel level) {
    g_return_val_if_fail(level >= XFDESKTOP_ICON_POSITION_LEVEL_INVALID && level <= XFDESKTOP_ICON_POSITION_LEVEL_OTHER, NULL);

    XfdesktopIconPositionConfig *config = g_new0(XfdesktopIconPositionConfig, 1);
    config->level = level;
    config->monitors = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)xfdesktop_icon_position_monitor_free);
    config->icon_positions = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    return config;
}

static gboolean
save_icons(XfdesktopIconPositionConfigs *configs, GError **error) {
    yaml_emitter_t emitter;
    if (!yaml_emitter_initialize(&emitter)) {
        g_set_error(error,
                    G_IO_ERROR,
                    G_IO_ERROR_FAILED,
                    "Failed to create YAML emitter: %s",
                    libyaml_strerror(emitter.error));
        return FALSE;
    }

    gchar *new_filename = g_strconcat(g_file_peek_path(configs->file), ".new", NULL);
    FILE *output = fopen(new_filename, "wb");
    if (output == NULL) {
        g_set_error(error,
                    G_IO_ERROR,
                    g_io_error_from_errno(errno),
                    "Failed to open '%s' for writing: %s",
                    new_filename,
                    strerror(errno));
        yaml_emitter_delete(&emitter);
        g_free(new_filename);
        return FALSE;
    }

    fputs("#\n# DO NOT EDIT THIS FILE WHILE XFDESKTOP IS RUNNING\n#\n", output);

    yaml_emitter_set_output_file(&emitter, output);

    yaml_event_t event;

#define EMIT(emitter, event) \
    G_STMT_START { \
        if (!yaml_emitter_emit(&emitter, &event)) { \
            g_set_error(error, \
                        G_IO_ERROR, \
                        G_IO_ERROR_FAILED, \
                        "Failed to emit YAML to file: %s", \
                        libyaml_strerror(emitter.error)); \
            goto out_err; \
        } \
    } \
G_STMT_END

#define YC(str) ((yaml_char_t *)(str))
#define EMIT_QSTR_EVENT(emitter, event, str) \
    G_STMT_START { \
        yaml_scalar_event_initialize(&event, NULL, YC("tag:yaml.org,2002:str"), YC(str), strlen(str), TRUE, TRUE, YAML_DOUBLE_QUOTED_SCALAR_STYLE); \
        EMIT(emitter, event); \
    } \
G_STMT_END
#define EMIT_UQSTR_EVENT(emitter, event, str) \
    G_STMT_START { \
        yaml_scalar_event_initialize(&event, NULL, YC("tag:yaml.org,2002:str"), YC(str), strlen(str), TRUE, TRUE, YAML_PLAIN_SCALAR_STYLE); \
        EMIT(emitter, event); \
    } \
G_STMT_END
#define EMIT_INT_EVENT(emitter, event, val) \
    G_STMT_START { \
        gchar *val_str = g_strdup_printf("%d", val); \
        yaml_scalar_event_initialize(&event, NULL, YC("tag:yaml.org,2002:int"), YC(val_str), strlen(val_str), TRUE, TRUE, YAML_PLAIN_SCALAR_STYLE); \
        EMIT(emitter, event); \
        g_free(val_str); \
    } \
G_STMT_END
#define EMIT_UINT_EVENT(emitter, event, val) \
    G_STMT_START { \
        gchar *val_str = g_strdup_printf("%u", val); \
        yaml_scalar_event_initialize(&event, NULL, YC("tag:yaml.org,2002:int"), YC(val_str), strlen(val_str), TRUE, TRUE, YAML_PLAIN_SCALAR_STYLE); \
        EMIT(emitter, event); \
        g_free(val_str); \
    } \
G_STMT_END
#define EMIT_UINT64_EVENT(emitter, event, val) \
    G_STMT_START { \
        gchar *val_str = g_strdup_printf("%" G_GUINT64_FORMAT, val); \
        yaml_scalar_event_initialize(&event, NULL, YC("tag:yaml.org,2002:int"), YC(val_str), strlen(val_str), TRUE, TRUE, YAML_PLAIN_SCALAR_STYLE); \
        EMIT(emitter, event); \
        g_free(val_str); \
    } \
G_STMT_END
#define EMIT_MAP_START_EVENT(emitter, event) \
    G_STMT_START { \
        yaml_mapping_start_event_initialize(&event, NULL, YC("tag:yaml.org,2002:map"), TRUE, YAML_BLOCK_MAPPING_STYLE); \
        EMIT(emitter, event); \
    } \
G_STMT_END
#define EMIT_SEQ_START_EVENT(emitter, event) \
    G_STMT_START { \
        yaml_sequence_start_event_initialize(&event, NULL, YC("tag:yaml.org,2002:seq"), TRUE, YAML_BLOCK_SEQUENCE_STYLE); \
        EMIT(emitter, event); \
    } \
G_STMT_END

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    EMIT(emitter, event);
    {
        yaml_document_start_event_initialize(&event, NULL, NULL, NULL, TRUE);
        EMIT(emitter, event);
        {
            EMIT_MAP_START_EVENT(emitter, event);
            {
                EMIT_UQSTR_EVENT(emitter, event, "configs");
                EMIT_SEQ_START_EVENT(emitter, event);
                {
                    for (GList *l = configs->configs; l != NULL; l = l->next) {
                        XfdesktopIconPositionConfig *config = l->data;

                        EMIT_MAP_START_EVENT(emitter, event);
                        {
                            EMIT_UQSTR_EVENT(emitter, event, "level");
                            EMIT_INT_EVENT(emitter, event, config->level);

                            EMIT_UQSTR_EVENT(emitter, event, "monitors");
                            EMIT_SEQ_START_EVENT(emitter, event);
                            {
                                GHashTableIter iter;
                                g_hash_table_iter_init(&iter, config->monitors);

                                const gchar *id;
                                XfdesktopIconPositionMonitor *pos_monitor;
                                while (g_hash_table_iter_next(&iter, (gpointer)&id, (gpointer)&pos_monitor)) {
                                    EMIT_MAP_START_EVENT(emitter, event);
                                    {
                                        EMIT_UQSTR_EVENT(emitter, event, "id");
                                        EMIT_QSTR_EVENT(emitter, event, id);

                                        EMIT_UQSTR_EVENT(emitter, event, "display_name");
                                        EMIT_QSTR_EVENT(emitter, event, pos_monitor->display_name);

                                        EMIT_UQSTR_EVENT(emitter, event, "geometry");
                                        EMIT_MAP_START_EVENT(emitter, event);
                                        {
                                            EMIT_UQSTR_EVENT(emitter, event, "x");
                                            EMIT_INT_EVENT(emitter, event, pos_monitor->geometry.x);
                                            EMIT_UQSTR_EVENT(emitter, event, "y");
                                            EMIT_INT_EVENT(emitter, event, pos_monitor->geometry.y);
                                            EMIT_UQSTR_EVENT(emitter, event, "width");
                                            EMIT_INT_EVENT(emitter, event, pos_monitor->geometry.width);
                                            EMIT_UQSTR_EVENT(emitter, event, "height");
                                            EMIT_INT_EVENT(emitter, event, pos_monitor->geometry.height);

                                            yaml_mapping_end_event_initialize(&event);
                                            EMIT(emitter, event);
                                        }

                                        yaml_mapping_end_event_initialize(&event);
                                        EMIT(emitter, event);
                                    }
                                }

                                yaml_sequence_end_event_initialize(&event);
                                EMIT(emitter, event);
                            }

                            EMIT_UQSTR_EVENT(emitter, event, "icons");
                            EMIT_MAP_START_EVENT(emitter, event);
                            {
                                GHashTableIter iter;
                                g_hash_table_iter_init(&iter, config->icon_positions);

                                const gchar *id;
                                XfdesktopIconPosition *position;
                                while (g_hash_table_iter_next(&iter, (gpointer)&id, (gpointer)&position)) {
                                    EMIT_QSTR_EVENT(emitter, event, id);
                                    EMIT_MAP_START_EVENT(emitter, event);
                                    {
                                        EMIT_UQSTR_EVENT(emitter, event, "row");
                                        EMIT_UINT_EVENT(emitter, event, position->row);
                                        EMIT_UQSTR_EVENT(emitter, event, "col");
                                        EMIT_UINT_EVENT(emitter, event, position->col);
                                        if (position->last_seen != 0) {
                                            EMIT_UQSTR_EVENT(emitter, event, "last_seen");
                                            EMIT_UINT64_EVENT(emitter, event, position->last_seen);
                                        }

                                        yaml_mapping_end_event_initialize(&event);
                                        EMIT(emitter, event);
                                    }
                                }

                                yaml_mapping_end_event_initialize(&event);
                                EMIT(emitter, event);
                            }

                            yaml_mapping_end_event_initialize(&event);
                            EMIT(emitter, event);
                        }
                    }

                    yaml_sequence_end_event_initialize(&event);
                    EMIT(emitter, event);
                }

                yaml_mapping_end_event_initialize(&event);
                EMIT(emitter, event);
            }

            yaml_document_end_event_initialize(&event, TRUE);
            EMIT(emitter, event);
        }

        yaml_stream_end_event_initialize(&event);
        EMIT(emitter, event);
    }

#undef EMIT_SEQ_START_EVENT
#undef EMIT_MAP_START_EVENT
#undef EMIT_QSTR_EVENT
#undef EMIT_UQSTR_EVENT
#undef EMIT_INT_EVENT
#undef EMIT_UINT_EVENT
#undef EMIT_UINT64_EVENT
#undef EMIT
#undef YC

    yaml_emitter_delete(&emitter);
    int ret = fclose(output);
    output = NULL;

    if (ret) {
        g_set_error(error,
                    G_IO_ERROR,
                    g_io_error_from_errno(errno),
                    "Failed to close written file '%s': %s",
                    new_filename,
                    strerror(errno));
        goto out_err;
    }

    if (rename(new_filename, g_file_peek_path(configs->file))) {
        g_set_error(error,
                    G_IO_ERROR,
                    g_io_error_from_errno(errno),
                    "Failed to rename '%s' to '%s': %s",
                    new_filename,
                    g_file_peek_path(configs->file),
                    strerror(errno));
        goto out_err;
    }

    g_free(new_filename);
    return TRUE;

out_err:
    if (output != NULL) {
        yaml_emitter_delete(&emitter);
        fclose(output);
    }
    unlink(new_filename);
    g_free(new_filename);

    return FALSE;
}

static gboolean
save_icons_idled(gpointer data) {
    XfdesktopIconPositionConfigs *configs = data;
    configs->scheduled_save_id = 0;

    GError *error = NULL;
    if (!save_icons(configs, &error)) {
        g_message("Failed to save desktop icon positions: %s", error->message);
        g_error_free(error);
    }

    return G_SOURCE_REMOVE;
}

static void
schedule_save(XfdesktopIconPositionConfigs *configs) {
    if (configs->scheduled_save_id == 0) {
        configs->scheduled_save_id = g_timeout_add_seconds(SAVE_DELAY_S, save_icons_idled, configs);
    }
}

static gboolean
scalar_equal(yaml_event_t *event, const gchar *value) {
    g_assert(event->type == YAML_SCALAR_EVENT);
    return event->data.scalar.value != NULL
        && strncmp((const char *)event->data.scalar.value, value, event->data.scalar.length) == 0;
}

static gchar *
scalar_to_str(yaml_event_t *event) {
    g_assert(event->type == YAML_SCALAR_EVENT);
    return event->data.scalar.value != NULL
        ? g_strndup((char *)event->data.scalar.value, event->data.scalar.length)
        : NULL;
}

static gchar *  // NULL means success
scalar_to_int(yaml_event_t *event, const gchar *key, gint *value) {
    g_assert(event->type == YAML_SCALAR_EVENT);

    switch (event->data.scalar.style) {
        case YAML_PLAIN_SCALAR_STYLE:
        case YAML_LITERAL_SCALAR_STYLE: {
            errno = 0;
            char *endptr = NULL;
            gchar *str = scalar_to_str(event);
            *value = strtol(str, &endptr, 10);

            if (errno != 0 || endptr == NULL || *endptr != '\0') {
                gchar *err = g_strdup_printf("Invalid value for %s: '%s'", key, str);
                g_free(str);
                return err;
            } else {
                g_free(str);
                return NULL;
            }
            break;
        }

        default:
            return g_strdup_printf("Bad scalar style %d for %s", event->data.scalar.style, key);
            break;
    }
}

static gchar *  // NULL means success
scalar_to_uint(yaml_event_t *event, const gchar *key, guint *value) {
    g_assert(event->type == YAML_SCALAR_EVENT);

    switch (event->data.scalar.style) {
        case YAML_PLAIN_SCALAR_STYLE:
        case YAML_LITERAL_SCALAR_STYLE: {
            errno = 0;
            char *endptr = NULL;
            gchar *str = scalar_to_str(event);
            *value = strtoul(str, &endptr, 10);

            if (errno != 0 || endptr == NULL || *endptr != '\0') {
                gchar *err = g_strdup_printf("Invalid value for %s: '%s'", key, str);
                g_free(str);
                return err;
            } else {
                g_free(str);
                return NULL;
            }
            break;
        }

        default:
            return g_strdup_printf("Bad scalar style %d for %s", event->data.scalar.style, key);
            break;
    }
}

static gchar *  // NULL means success
scalar_to_uint64(yaml_event_t *event, const gchar *key, guint64 *value) {
    g_assert(event->type == YAML_SCALAR_EVENT);

    switch (event->data.scalar.style) {
        case YAML_PLAIN_SCALAR_STYLE:
        case YAML_LITERAL_SCALAR_STYLE: {
            errno = 0;
            char *endptr = NULL;
            gchar *str = scalar_to_str(event);
            *value = strtoull(str, &endptr, 10);

            if (errno != 0 || endptr == NULL || *endptr != '\0') {
                gchar *err = g_strdup_printf("Invalid value for %s: '%s'", key, str);
                g_free(str);
                return err;
            } else {
                g_free(str);
                return NULL;
            }
            break;
        }

        default:
            return g_strdup_printf("Bad scalar style %d for %s", event->data.scalar.style, key);
            break;
    }
}

static const gchar *
validate_position_config(XfdesktopIconPositionConfig *config) {
    if (config->level == XFDESKTOP_ICON_POSITION_LEVEL_INVALID) {
        return "Missing config->level";
    } else {
        return NULL;
    }
}

static const gchar *
validate_monitor_config(XfdesktopIconPositionMonitor *pos_monitor) {
    if (pos_monitor->display_name == NULL || pos_monitor->display_name[0] == '\0') {
        return "Missing or empty config->monitors[]->display_name";
    } else if (pos_monitor->geometry.x == G_MININT) {
        return "Missing config->monitors[]->geometry->x";
    } else if (pos_monitor->geometry.y == G_MININT) {
        return "Missing config->monitors[]->geometry->y";
    } else if (pos_monitor->geometry.width == G_MININT) {
        return "Missing config->monitors[]->geometry->width";
    } else if (pos_monitor->geometry.height == G_MININT) {
        return "Missing config->monitors[]->geometry->height";
    } else {
        return NULL;
    }
}

static const gchar *
validate_position(XfdesktopIconPosition *position) {
    if (position->row == G_MAXINT) {
        return "Missing row in icon position";
    } else if (position->col == G_MAXINT) {
        return "Missing col in icon position";
    } else {
        return NULL;
    }
}

XfdesktopIconPositionConfigs *
xfdesktop_icon_position_configs_new(GFile *file) {
    g_return_val_if_fail(G_IS_FILE(file), NULL);

    XfdesktopIconPositionConfigs *configs = g_new0(XfdesktopIconPositionConfigs, 1);
    configs->file = g_object_ref(file);
    configs->config_to_monitor = g_hash_table_new(g_direct_hash, g_direct_equal);

    return configs;
}

gboolean
xfdesktop_icon_position_configs_load(XfdesktopIconPositionConfigs *configs, GError **error) {
    g_return_val_if_fail(configs != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    yaml_parser_t parser;
    if (!yaml_parser_initialize(&parser)) {
        g_set_error(error,
                    G_IO_ERROR,
                    G_IO_ERROR_NOT_INITIALIZED,
                    "Failed to initialize YAML parser: %s",
                    libyaml_strerror(parser.error));
        return FALSE;
    }

    const gchar *filename = g_file_peek_path(configs->file);
    FILE *input = fopen(filename, "rb");
    if (input == NULL) {
        g_set_error(error,
                    G_IO_ERROR,
                    g_io_error_from_errno(errno),
                    "Failed to open '%s': %s",
                    filename,
                    strerror(errno));
        yaml_parser_delete(&parser);
        return FALSE;
    }

    yaml_parser_set_input_file(&parser, input);

    g_hash_table_remove_all(configs->config_to_monitor);
    g_list_free_full(configs->configs, (GDestroyNotify)xfdesktop_icon_position_config_free);
    configs->configs = NULL;

#ifdef DEBUG
    guint level = 0;
    memset(spaces, ' ', sizeof(spaces) - 1);
    spaces[sizeof(spaces) - 1] = '\0';
#endif

    ParserState state = PARSER_TOP;
    XfdesktopIconPositionConfig *cur_config = NULL;
    XfdesktopIconPosition *cur_position = NULL;
    gchar *cur_monitor_id = NULL;
    XfdesktopIconPositionMonitor *cur_pos_monitor = NULL;

    while (state != PARSER_DONE && state != PARSER_ERROR) {
        yaml_event_t event;
        if (!yaml_parser_parse(&parser, &event)) {
            g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED, libyaml_strerror(parser.error));
            break;
        }

        switch (event.type) {
            case YAML_NO_EVENT:
                break;

            case YAML_STREAM_START_EVENT:
                LOG_EVENT(level, "STREAM_START");
                break;

            case YAML_DOCUMENT_START_EVENT:
                LOG_EVENT_IN(level, "DOC_START");
                break;

            case YAML_DOCUMENT_END_EVENT:
                LOG_EVENT_OUT(level, "DOC_END");
                break;

            case YAML_ALIAS_EVENT:
                LOG_EVENT(level, "ALIAS");
                PARSE_ERROR(level, state, configs, event, "YAML aliases not supported");
                break;

            case YAML_SCALAR_EVENT:
                LOG_EVENT(level, "SCALAR");
                switch (state) {
                    case PARSER_TOPLEVEL_MAP:
                        if (scalar_equal(&event, "configs")) {
                            STATE_TRANSITION(level, state, PARSER_CONFIGS);
                        } else {
                            gchar *key = scalar_to_str(&event);
                            PARSE_ERROR(level, state, configs, event, "Unexpected toplevel map key %s", key);
                            g_free(key);
                        }
                        break;

                    case PARSER_CONFIG_MAP:
                        if (scalar_equal(&event, "level")) {
                            STATE_TRANSITION(level, state, PARSER_CONFIG_LEVEL);
                        } else if (scalar_equal(&event, "monitors")) {
                            STATE_TRANSITION(level, state, PARSER_CONFIG_MONITORS);
                        } else if (scalar_equal(&event, "icons")) {
                            STATE_TRANSITION(level, state, PARSER_CONFIG_ICONS);
                        } else {
                            gchar *scalar = scalar_to_str(&event);
                            PARSE_ERROR(level, state, configs, event, "Unexpected scalar in config '%s'", scalar);
                            g_free(scalar);
                        }
                        break;

                    case PARSER_CONFIG_LEVEL: {
                        g_assert(cur_config != NULL);
                        gchar *err_msg = scalar_to_int(&event, "level", &cur_config->level);
                        if (err_msg != NULL) {
                            PARSE_ERROR(level, state, configs, event, "%s", err_msg);
                            g_free(err_msg);
                        } else {
                            STATE_TRANSITION(level, state, PARSER_CONFIG_MAP);
                        }
                        break;
                    }

                    case PARSER_CONFIG_MONITOR_MAP:
                        if (scalar_equal(&event, "id")) {
                            STATE_TRANSITION(level, state, PARSER_CONFIG_MONITOR_ID);
                        } else if (scalar_equal(&event, "display_name")) {
                            STATE_TRANSITION(level, state, PARSER_CONFIG_MONITOR_DISPLAY_NAME);
                        } else if (scalar_equal(&event, "geometry")) {
                            STATE_TRANSITION(level, state, PARSER_CONFIG_MONITOR_GEOMETRY);
                        } else {
                            gchar *scalar = scalar_to_str(&event);
                            PARSE_ERROR(level, state, configs, event, "Unexpected scalar in monitors '%s'", scalar);
                            g_free(scalar);
                        }
                        break;

                    case PARSER_CONFIG_MONITOR_ID:
                        g_free(cur_monitor_id);
                        cur_monitor_id = scalar_to_str(&event);
                        if (cur_monitor_id == NULL) {
                            PARSE_ERROR(level, state, configs, event, "Empty monitor ID");
                        } else {
                            STATE_TRANSITION(level, state, PARSER_CONFIG_MONITOR_MAP);
                        }
                        break;

                    case PARSER_CONFIG_MONITOR_DISPLAY_NAME:
                        g_assert(cur_pos_monitor != NULL);
                        g_free(cur_pos_monitor->display_name);
                        cur_pos_monitor->display_name = scalar_to_str(&event);
                        if (cur_pos_monitor->display_name == NULL) {
                            PARSE_ERROR(level, state, configs, event, "Empty monitor display name");
                        } else {
                            STATE_TRANSITION(level, state, PARSER_CONFIG_MONITOR_MAP);
                        }
                        break;

                    case PARSER_CONFIG_MONITOR_GEOMETRY_MAP:
                        if (scalar_equal(&event, "x")) {
                            STATE_TRANSITION(level, state, PARSER_CONFIG_MONITOR_GEOMETRY_MAP_X);
                        } else if (scalar_equal(&event, "y")) {
                            STATE_TRANSITION(level, state, PARSER_CONFIG_MONITOR_GEOMETRY_MAP_Y);
                        } else if (scalar_equal(&event, "width")) {
                            STATE_TRANSITION(level, state, PARSER_CONFIG_MONITOR_GEOMETRY_MAP_WIDTH);
                        } else if (scalar_equal(&event, "height")) {
                            STATE_TRANSITION(level, state, PARSER_CONFIG_MONITOR_GEOMETRY_MAP_HEIGHT);
                        } else {
                            gchar *scalar = scalar_to_str(&event);
                            PARSE_ERROR(level, state, configs, event, "Unexpected scalar in monitors[]->geometry '%s'", scalar);
                            g_free(scalar);
                        }
                        break;

                    case PARSER_CONFIG_MONITOR_GEOMETRY_MAP_X: {
                        g_assert(cur_pos_monitor != NULL);
                        gchar *err_msg = scalar_to_int(&event, "geometry->x", &cur_pos_monitor->geometry.x);
                        if (err_msg != NULL) {
                            PARSE_ERROR(level, state, configs, event, "%s", err_msg);
                            g_free(err_msg);
                        } else {
                            STATE_TRANSITION(level, state, PARSER_CONFIG_MONITOR_GEOMETRY_MAP);
                        }
                        break;
                    }

                    case PARSER_CONFIG_MONITOR_GEOMETRY_MAP_Y: {
                        g_assert(cur_pos_monitor != NULL);
                        gchar *err_msg = scalar_to_int(&event, "geometry->y", &cur_pos_monitor->geometry.y);
                        if (err_msg != NULL) {
                            PARSE_ERROR(level, state, configs, event, "%s", err_msg);
                            g_free(err_msg);
                        } else {
                            STATE_TRANSITION(level, state, PARSER_CONFIG_MONITOR_GEOMETRY_MAP);
                        }
                        break;
                    }

                    case PARSER_CONFIG_MONITOR_GEOMETRY_MAP_WIDTH: {
                        g_assert(cur_pos_monitor != NULL);
                        gchar *err_msg = scalar_to_int(&event, "geometry->width", &cur_pos_monitor->geometry.width);
                        if (err_msg != NULL) {
                            PARSE_ERROR(level, state, configs, event, "%s", err_msg);
                            g_free(err_msg);
                        } else {
                            STATE_TRANSITION(level, state, PARSER_CONFIG_MONITOR_GEOMETRY_MAP);
                        }
                        break;
                    }

                    case PARSER_CONFIG_MONITOR_GEOMETRY_MAP_HEIGHT: {
                        g_assert(cur_pos_monitor != NULL);
                        gchar *err_msg = scalar_to_int(&event, "geometry->height", &cur_pos_monitor->geometry.height);
                        if (err_msg != NULL) {
                            PARSE_ERROR(level, state, configs, event, "%s", err_msg);
                            g_free(err_msg);
                        } else {
                            STATE_TRANSITION(level, state, PARSER_CONFIG_MONITOR_GEOMETRY_MAP);
                        }
                        break;
                    }

                    case PARSER_CONFIG_ICONS_MAP:
                        if (cur_position != NULL) {
                            PARSE_ERROR(level, state, configs, event, "Got new icon position when old position not closed");
                        } else {
                            g_assert(cur_config != NULL);
                            gchar *id = scalar_to_str(&event);
                            if (id == NULL) {
                                PARSE_ERROR(level, state, configs, event, "Empty icon ID");
                            } else {
                                cur_position = g_new0(XfdesktopIconPosition, 1);
                                cur_position->row = G_MAXINT;
                                cur_position->col = G_MAXINT;
                                cur_position->last_seen = 0;
                                g_hash_table_insert(cur_config->icon_positions, id, cur_position);
                                STATE_TRANSITION(level, state, PARSER_CONFIG_ICON);
                            }
                        }
                        break;

                    case PARSER_CONFIG_ICON_MAP: {
                        if (scalar_equal(&event, "row")) {
                            STATE_TRANSITION(level, state, PARSER_CONFIG_ICON_MAP_ROW);
                        } else if (scalar_equal(&event, "col")) {
                            STATE_TRANSITION(level, state, PARSER_CONFIG_ICON_MAP_COL);
                        } else if (scalar_equal(&event, "last_seen")) {
                            STATE_TRANSITION(level, state, PARSER_CONFIG_ICON_MAP_LAST_SEEN);
                        } else {
                            gchar *scalar = scalar_to_str(&event);
                            PARSE_ERROR(level, state, configs, event, "Unexpected scalar in icon '%s'", scalar);
                            g_free(scalar);
                        }
                        break;
                    }

                    case PARSER_CONFIG_ICON_MAP_ROW: {
                        g_assert(cur_config != NULL);
                        gchar *err_msg = scalar_to_uint(&event, "icon->row", &cur_position->row);
                        if (err_msg != NULL) {
                            PARSE_ERROR(level, state, configs, event, "%s", err_msg);
                            g_free(err_msg);
                        } else {
                            STATE_TRANSITION(level, state, PARSER_CONFIG_ICON_MAP);
                        }
                        break;
                    }

                    case PARSER_CONFIG_ICON_MAP_COL: {
                        g_assert(cur_config != NULL);
                        gchar *err_msg = scalar_to_uint(&event, "icon->col", &cur_position->col);
                        if (err_msg != NULL) {
                            PARSE_ERROR(level, state, configs, event, "%s", err_msg);
                            g_free(err_msg);
                        } else {
                            STATE_TRANSITION(level, state, PARSER_CONFIG_ICON_MAP);
                        }
                        break;
                    }

                    case PARSER_CONFIG_ICON_MAP_LAST_SEEN: {
                        g_assert(cur_config != NULL);
                        gchar *err_msg = scalar_to_uint64(&event, "icon->last_seen", &cur_position->last_seen);
                        if (err_msg != NULL) {
                            PARSE_ERROR(level, state, configs, event, "%s", err_msg);
                            g_free(err_msg);
                        } else {
                            STATE_TRANSITION(level, state, PARSER_CONFIG_ICON_MAP);
                        }
                        break;
                    }

                    default: {
                        gchar *scalar = scalar_to_str(&event);
                        PARSE_ERROR(level, state, configs, event, "Unexpected scalar '%s'", scalar);
                        g_free(scalar);
                        break;
                    }
                }
                break;

            case YAML_SEQUENCE_START_EVENT:
                LOG_EVENT_IN(level, "SEQUENCE_START");
                switch (state) {
                    case PARSER_CONFIGS:
                        STATE_TRANSITION(level, state, PARSER_CONFIGS_ARRAY);
                        break;

                    case PARSER_CONFIG_MONITORS:
                        STATE_TRANSITION(level, state, PARSER_CONFIG_MONITORS_ARRAY);
                        break;

                    default:
                        PARSE_ERROR(level, state, configs, event, "Unexpected sequence");
                        break;
                }
                break;

            case YAML_SEQUENCE_END_EVENT:
                LOG_EVENT_OUT(level, "SEQUENCE_END");
                switch (state) {
                    case PARSER_CONFIG_MONITORS_ARRAY:
                        STATE_TRANSITION(level, state, PARSER_CONFIG_MAP);
                        break;

                    case PARSER_CONFIGS_ARRAY:
                        STATE_TRANSITION(level, state, PARSER_TOPLEVEL_MAP);
                        break;

                    default:
                        PARSE_ERROR(level, state, configs, event, "Unexpected sequence end");
                        break;
                }
                break;

            case YAML_MAPPING_START_EVENT:
                LOG_EVENT_IN(level, "MAPPING_START");
                switch (state) {
                    case PARSER_TOP:
                        STATE_TRANSITION(level, state, PARSER_TOPLEVEL_MAP);
                        break;

                    case PARSER_CONFIGS_ARRAY:
                        if (cur_config != NULL) {
                            PARSE_ERROR(level, state, configs, event, "Got new config when old config not closed");
                        } else {
                            STATE_TRANSITION(level, state, PARSER_CONFIG_MAP);
                            cur_config = xfdesktop_icon_position_config_new_internal(XFDESKTOP_ICON_POSITION_LEVEL_INVALID);
                            configs->configs = g_list_prepend(configs->configs, cur_config);
                        }
                        break;

                    case PARSER_CONFIG_MONITORS_ARRAY:
                        if (cur_pos_monitor != NULL) {
                            PARSE_ERROR(level, state, configs, event, "Got new position monitor when old one not closed");
                        } else {
                            g_assert(cur_config != NULL);
                            cur_pos_monitor = g_new0(XfdesktopIconPositionMonitor, 1);
                            cur_pos_monitor->geometry.x = G_MININT;
                            cur_pos_monitor->geometry.y = G_MININT;
                            cur_pos_monitor->geometry.width = G_MININT;
                            cur_pos_monitor->geometry.height = G_MININT;
                            STATE_TRANSITION(level, state, PARSER_CONFIG_MONITOR_MAP);
                        }
                        break;

                    case PARSER_CONFIG_MONITOR_GEOMETRY:
                        STATE_TRANSITION(level, state, PARSER_CONFIG_MONITOR_GEOMETRY_MAP);
                        break;

                    case PARSER_CONFIG_ICON:
                        STATE_TRANSITION(level, state, PARSER_CONFIG_ICON_MAP);
                        break;

                    case PARSER_CONFIG_ICONS:
                        STATE_TRANSITION(level, state, PARSER_CONFIG_ICONS_MAP);
                        break;

                    default:
                        PARSE_ERROR(level, state, configs, event, "Unexpected map");
                        break;
                }
                break;

            case YAML_MAPPING_END_EVENT:
                LOG_EVENT_OUT(level, "MAPPING_END");
                switch (state) {
                    case PARSER_CONFIG_MONITOR_GEOMETRY_MAP:
                        STATE_TRANSITION(level, state, PARSER_CONFIG_MONITOR_MAP);
                        break;

                    case PARSER_CONFIG_ICON_MAP: {
                        const gchar *err_msg = validate_position(cur_position);
                        if (err_msg != NULL) {
                            PARSE_ERROR(level, state, configs, event, "%s", err_msg);
                        } else {
                            STATE_TRANSITION(level, state, PARSER_CONFIG_ICONS_MAP);
                            cur_position = NULL;
                        }
                        break;
                    }

                    case PARSER_CONFIG_MONITOR_MAP:
                        g_assert(cur_config != NULL);
                        if (cur_monitor_id == NULL) {
                            PARSE_ERROR(level, state, configs, event, "Monitor is missing ID");
                        } else {
                            const gchar *err_msg = validate_monitor_config(cur_pos_monitor);
                            if (err_msg != NULL) {
                                PARSE_ERROR(level, state, configs, event, "%s", err_msg);
                            } else {
                                g_hash_table_insert(cur_config->monitors, cur_monitor_id, cur_pos_monitor);
                                cur_monitor_id = NULL;
                                cur_pos_monitor = NULL;
                                STATE_TRANSITION(level, state, PARSER_CONFIG_MONITORS_ARRAY);
                            }
                        }
                        break;

                    case PARSER_CONFIG_ICONS_MAP:
                        STATE_TRANSITION(level, state, PARSER_CONFIG_MAP);
                        break;

                    case PARSER_CONFIG_MAP: {
                        const gchar *err_msg = validate_position_config(cur_config);
                        if (err_msg != NULL) {
                            PARSE_ERROR(level, state, configs, event, "%s", err_msg);
                        } else {
                            STATE_TRANSITION(level, state, PARSER_CONFIGS_ARRAY);
                            cur_config = NULL;
                        }
                        break;
                    }

                    case PARSER_TOPLEVEL_MAP:
                        STATE_TRANSITION(level, state, PARSER_TOP);
                        break;

                    case PARSER_CONFIG_ICONS:
                        STATE_TRANSITION(level, state, PARSER_CONFIG_MAP);
                        break;

                    default:
                        PARSE_ERROR(level, state, configs, event, "Unexpected end of map");
                        break;
                }
                break;

            case YAML_STREAM_END_EVENT:
                LOG_EVENT(level, "STREAM_END");
                switch (state) {
                    case PARSER_TOP:
                        STATE_TRANSITION(level, state, PARSER_DONE);
                        break;

                    default:
                        PARSE_ERROR(level, state, configs, event, "Unexpected end of stream");
                        break;
                }
                break;
        }

        yaml_event_delete(&event);
    }

    yaml_parser_delete(&parser);
    fclose(input);

    if (state == PARSER_ERROR) {
        g_hash_table_remove_all(configs->config_to_monitor);
        g_list_free_full(configs->configs, (GDestroyNotify)xfdesktop_icon_position_config_free);
        configs->configs = NULL;

        g_free(cur_monitor_id);
        xfdesktop_icon_position_monitor_free(cur_pos_monitor);

        return FALSE;
    } else {
        configs->configs = g_list_sort(configs->configs, compare_configs_by_level);
        return TRUE;
    }
}

gboolean
xfdesktop_icon_position_configs_lookup(XfdesktopIconPositionConfigs *configs,
                                       const gchar *icon_id,
                                       XfwMonitor **monitor,
                                       gint *row,
                                       gint *col)
{
    g_return_val_if_fail(configs != NULL, FALSE);
    g_return_val_if_fail(icon_id != NULL, FALSE);

    gint best_level = XFDESKTOP_ICON_POSITION_LEVEL_INVALID;
    XfwMonitor *best_monitor = NULL;
    XfdesktopIconPosition *best_position = NULL;

    for (GList *l = configs->configs; l != NULL; l = l->next) {
        XfdesktopIconPositionConfig *config = l->data;
        XfwMonitor *config_monitor = g_hash_table_lookup(configs->config_to_monitor, config);
        if (config_monitor != NULL) {
            XfdesktopIconPosition *position = g_hash_table_lookup(config->icon_positions, icon_id);
            if (position != NULL && config->level > best_level) {
                best_level = config->level;
                best_monitor = config_monitor;
                best_position = position;
            }
        }
    }

    if (best_level != XFDESKTOP_ICON_POSITION_LEVEL_INVALID) {
        if (monitor != NULL) {
            *monitor = best_monitor;
        }
        if (row != NULL) {
            *row = best_position->row;
        }
        if (col != NULL) {
            *col = best_position->col;
        }
        return TRUE;
    } else {
        return FALSE;
    }
}

void
xfdesktop_icon_position_configs_remove_icon(XfdesktopIconPositionConfigs *configs,
                                            XfdesktopIconPositionConfig *config,
                                            const gchar *icon_id)
{
    g_return_if_fail(configs != NULL);
    g_return_if_fail(config != NULL);
    g_return_if_fail(icon_id != NULL);

    g_hash_table_remove(config->icon_positions, icon_id);
    schedule_save(configs);
}

void
xfdesktop_icon_position_configs_set_icon_position(XfdesktopIconPositionConfigs *configs,
                                                  XfdesktopIconPositionConfig *config,
                                                  const gchar *identifier,
                                                  guint row,
                                                  guint col,
                                                  guint64 last_seen_timestamp)
{
    g_return_if_fail(configs != NULL);
    g_return_if_fail(config != NULL);
    g_return_if_fail(identifier != NULL);

    XfdesktopIconPosition *position = g_hash_table_lookup(config->icon_positions, identifier);
    if (position == NULL) {
        position = g_new0(XfdesktopIconPosition, 1);
        g_hash_table_insert(config->icon_positions, g_strdup(identifier), position);
    }

    position->row = row;
    position->col = col;
    position->last_seen = last_seen_timestamp;

    for (GList *l = configs->configs; l != NULL; l = l->next) {
        XfdesktopIconPositionConfig *a_config = l->data;
        if (a_config != config && a_config->level >= config->level) {
            DBG("removing icon from higher or equal prio config");
            // XXX: Do we want to do this for all configs, or only assigned
            // configs?  I could make an argument either way.
            g_hash_table_remove(a_config->icon_positions, identifier);
        }
    }

    schedule_save(configs);
}

XfdesktopIconPositionConfig *
xfdesktop_icon_position_configs_add_monitor(XfdesktopIconPositionConfigs *configs,
                                            XfwMonitor *monitor,
                                            XfdesktopIconPositionLevel level,
                                            GList **candidates)
{
    g_return_val_if_fail(configs != NULL, NULL);
    g_return_val_if_fail(XFW_IS_MONITOR(monitor), NULL);
    g_return_val_if_fail(candidates != NULL && *candidates == NULL, NULL);

    const gchar *monitor_id = xfw_monitor_get_identifier(monitor);
    GdkRectangle geom;
    xfw_monitor_get_logical_geometry(monitor, &geom);

    XfdesktopIconPositionConfig *best_config = NULL;

    for (GList *l = configs->configs; l != NULL; l = l->next) {
        XfdesktopIconPositionConfig *config = l->data;
        if (!g_hash_table_contains(configs->config_to_monitor, config)) {
            if (g_hash_table_contains(config->monitors, monitor_id)) {
                // If the monitor ID matches and the config is currently
                // unused, then (almost) certainly we have the right config.
                best_config = config;
                break;
            } else {
                GHashTableIter iter;
                g_hash_table_iter_init(&iter, config->monitors);

                XfdesktopIconPositionMonitor *pos_monitor;
                while (g_hash_table_iter_next(&iter, NULL, (gpointer)&pos_monitor)) {
                    if (pos_monitor->geometry.width == geom.width && pos_monitor->geometry.height == geom.height) {
                        // If it's the same level and width/height, it *could*
                        // match, but we'll (possibly) ask the user to choose.
                        *candidates = g_list_prepend(*candidates, config);
                    }
                }
            }
        }
    }

    if (best_config == NULL) {
        gint n_candidates = g_list_length(*candidates);
        if (n_candidates == 0) {
            // Usually if there are no candidates we'd just create a new config
            // and move on, but for now we want to return nothing and let the
            // migration code have a crack at it.
            //best_config = xfdesktop_icon_position_config_new(level);
        } else if (n_candidates == 1) {
            best_config = g_list_nth_data(*candidates, 0);
        } else {
            // We're going to look over them again, to see if (even though no
            // monitor ID matched) there might be a config that has the exact same
            // geometry (including x/y) as this monitor.
            GList *same_geom_candidates = NULL;
            for (GList *l = *candidates; l != NULL; l = l->next) {
                XfdesktopIconPositionConfig *config = l->data;

                GHashTableIter iter;
                g_hash_table_iter_init(&iter, config->monitors);

                XfdesktopIconPositionMonitor *pos_monitor;
                while (g_hash_table_iter_next(&iter, NULL, (gpointer)&pos_monitor)) {
                    if (gdk_rectangle_equal(&geom, &pos_monitor->geometry)) {
                        same_geom_candidates = g_list_prepend(same_geom_candidates, config);
                    }
                }
            }

            best_config = g_list_nth_data(same_geom_candidates, 0);
            gint n_same_geom_candidates = g_list_length(same_geom_candidates);
            g_list_free(same_geom_candidates);

            if (n_same_geom_candidates != 1) {
                // If more than one candidate has the same geometry, don't
                // auto-assign.
                best_config = NULL;
            }
        }
    }

    if (best_config != NULL) {
        xfdesktop_icon_position_configs_assign_monitor(configs, best_config, monitor);
        g_clear_pointer(candidates, g_list_free);
        return best_config;
    } else {
        *candidates = g_list_sort_with_data(*candidates, compare_candidates, GINT_TO_POINTER(level));
        return NULL;
    }
}

void
xfdesktop_icon_position_configs_assign_monitor(XfdesktopIconPositionConfigs *configs,
                                               XfdesktopIconPositionConfig *config,
                                               XfwMonitor *monitor)
{
    g_return_if_fail(configs != NULL);
    g_return_if_fail(config != NULL);
    g_return_if_fail(XFW_IS_MONITOR(monitor));

    const gchar *monitor_id = xfw_monitor_get_identifier(monitor);
    if (!g_hash_table_contains(config->monitors, monitor_id)) {
        XfdesktopIconPositionMonitor *pos_monitor = g_new0(XfdesktopIconPositionMonitor, 1);
        pos_monitor->display_name = g_strdup(xfw_monitor_get_description(monitor));
        xfw_monitor_get_logical_geometry(monitor, &pos_monitor->geometry);
        g_hash_table_insert(config->monitors, g_strdup(monitor_id), pos_monitor);
    }

    if (g_list_find(configs->configs, config) == NULL) {
        configs->configs = g_list_insert_sorted(configs->configs, config, compare_configs_by_level);
    }
    g_hash_table_insert(configs->config_to_monitor, config, monitor);

    schedule_save(configs);
}

void
xfdesktop_icon_position_configs_unassign_monitor(XfdesktopIconPositionConfigs *configs, XfwMonitor *monitor) {
    g_return_if_fail(configs != NULL);
    g_return_if_fail(XFW_IS_MONITOR(monitor));

    GHashTableIter iter;
    g_hash_table_iter_init(&iter, configs->config_to_monitor);

    XfwMonitor *a_monitor;
    while (g_hash_table_iter_next(&iter, NULL, (gpointer)&a_monitor)) {
        if (a_monitor == monitor) {
            g_hash_table_iter_remove(&iter);
            break;
        }
    }
}

void
xfdesktop_icon_position_configs_delete_config(XfdesktopIconPositionConfigs *configs, XfdesktopIconPositionConfig *config) {
    g_return_if_fail(configs != NULL);
    g_return_if_fail(config != NULL);

    g_hash_table_remove(configs->config_to_monitor, config);
    configs->configs = g_list_remove(configs->configs, config);

    xfdesktop_icon_position_config_free(config);

    schedule_save(configs);
}

gboolean
xfdesktop_icon_position_configs_save(XfdesktopIconPositionConfigs *configs, GError **error) {
    g_return_val_if_fail(configs != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    return save_icons(configs, error);
}

void
xfdesktop_icon_position_configs_free(XfdesktopIconPositionConfigs *configs) {
    if (configs != NULL) {
        if (configs->scheduled_save_id != 0) {
            g_source_remove(configs->scheduled_save_id);

            GError *error = NULL;
            if (!save_icons(configs, &error)) {
                g_message("Failed to save desktop icon positions: %s", error->message);
                g_error_free(error);
            }
        }

        g_hash_table_destroy(configs->config_to_monitor);
        g_list_free_full(configs->configs, (GDestroyNotify)xfdesktop_icon_position_config_free);
        g_object_unref(configs->file);
        g_free(configs);
    }
}

XfdesktopIconPositionConfig *
xfdesktop_icon_position_config_new(XfdesktopIconPositionLevel level) {
    g_return_val_if_fail(level >= XFDESKTOP_ICON_POSITION_LEVEL_PRIMARY && level <= XFDESKTOP_ICON_POSITION_LEVEL_OTHER, NULL);
    return xfdesktop_icon_position_config_new_internal(level);
}

void
_xfdesktop_icon_position_config_set_icon_position(XfdesktopIconPositionConfig *config,
                                                  const gchar *identifier,
                                                  guint row,
                                                  guint col)
{
    g_return_if_fail(config != NULL);
    g_return_if_fail(identifier != NULL);

    XfdesktopIconPosition *position = g_hash_table_lookup(config->icon_positions, identifier);
    if (position == NULL) {
        position = g_new0(XfdesktopIconPosition, 1);
        g_hash_table_insert(config->icon_positions, g_strdup(identifier), position);
    }

    position->row = row;
    position->col = col;
}

GList *  // caller owned container, callee owned data
xfdesktop_icon_position_config_get_monitor_display_names(XfdesktopIconPositionConfig *config) {
    GHashTableIter iter;
    g_hash_table_iter_init(&iter, config->monitors);

    GList *display_names = NULL;
    XfdesktopIconPositionMonitor *pos_monitor;
    while (g_hash_table_iter_next(&iter, NULL, (gpointer)&pos_monitor)) {
        display_names = g_list_prepend(display_names, pos_monitor->display_name);
    }
    return g_list_reverse(display_names);
}

void
xfdesktop_icon_position_config_free(XfdesktopIconPositionConfig *config) {
    if (config != NULL) {
        g_hash_table_destroy(config->icon_positions);
        g_hash_table_destroy(config->monitors);
        g_free(config);
    }
}

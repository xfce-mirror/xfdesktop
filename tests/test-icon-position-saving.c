#include <gtk/gtk.h>
#include <libxfce4windowing/libxfce4windowing.h>
#include <stdio.h>
#include <stdlib.h>

#include "xfdesktop-icon-position-configs.h"

int
main(int argc, char **argv) {
    gtk_init(&argc, &argv);

    if (argc != 2) {
        fprintf(stderr, "Usage: %s YAML_FILE\n", argv[0]);
        return EXIT_FAILURE;
    }

    XfwScreen *screen = xfw_screen_get_default();

    GFile *file = g_file_new_for_path(argv[1]);
    XfdesktopIconPositionConfigs *configs = xfdesktop_icon_position_configs_new(file);
    g_object_unref(file);

    for (GList *l = xfw_screen_get_monitors(screen); l != NULL; l = l->next) {
        XfwMonitor *monitor = XFW_MONITOR(l->data);

        XfdesktopIconPositionConfig *config = xfdesktop_icon_position_config_new(XFDESKTOP_ICON_POSITION_LEVEL_PRIMARY);
        xfdesktop_icon_position_configs_assign_monitor(configs, config, monitor);

        xfdesktop_icon_position_configs_set_icon_position(configs,
                                                          config,
                                                          "foo.desktop",
                                                          1,
                                                          2,
                                                          g_get_real_time());

        xfdesktop_icon_position_configs_set_icon_position(configs,
                                                          config,
                                                          "somefolder",
                                                          8,
                                                          9,
                                                          0);

        xfdesktop_icon_position_configs_set_icon_position(configs,
                                                          config,
                                                          "blah.jpg",
                                                          22,
                                                          2,
                                                          g_get_real_time());
    }

    GError *error = NULL;
    if (!xfdesktop_icon_position_configs_save(configs, &error)) {
        g_printerr("Failed to save configs: %s\n", error->message);
        g_error_free(error);
        xfdesktop_icon_position_configs_free(configs);
        g_object_unref(screen);
        return EXIT_FAILURE;
    }

    xfdesktop_icon_position_configs_free(configs);
    g_object_unref(screen);

    return EXIT_SUCCESS;
}

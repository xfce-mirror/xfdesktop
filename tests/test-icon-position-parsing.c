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
    GError *error = NULL;
    XfdesktopIconPositionConfigs *configs = xfdesktop_icon_position_configs_new(file);
    g_object_unref(file);

    if (!xfdesktop_icon_position_configs_load(configs, &error)) {
        g_printerr("Failed to parse configs: %s\n", error->message);
        g_error_free(error);
        xfdesktop_icon_position_configs_free(configs);
        g_object_unref(screen);
        return EXIT_FAILURE;
    }

    G_BREAKPOINT();

    xfdesktop_icon_position_configs_free(configs);
    g_object_unref(screen);

    return EXIT_SUCCESS;
}

#include <glib.h>
#include <gtk/gtk.h>
#include <time.h>

#include "xfdesktop-common.c"
#include "xfdesktop-backdrop-renderer.c"

#define ITERATIONS 10

int
main(int argc, char **argv) {
    gtk_init(&argc, &argv);

    GdkRGBA color1 = {
        .red = 0.0,
        .green = 1.0,
        .blue = 0.0,
        .alpha = 1.0,
    };
    GdkRGBA color2 = {
        .red = 1.0,
        .green = 0.0,
        .blue = 0.0,
        .alpha = 1.0,
    };

    struct timespec start;
    int ret = clock_gettime(CLOCK_MONOTONIC, &start);
    g_assert(ret == 0);

    for (gsize i = 0; i < ITERATIONS; ++i) {
        GdkPixbuf *pix = create_gradient(&color1, &color2, 2500, 2500, XFCE_BACKDROP_COLOR_HORIZ_GRADIENT);
        g_object_unref(pix);
    }

    struct timespec end;
    ret = clock_gettime(CLOCK_MONOTONIC, &end);
    g_assert(ret == 0);

    gulong elapsed = (end.tv_sec * 1000 + end.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000);
    g_print("Total time: %lu ms\n", elapsed);
    g_print("Average time per iteration: %lu ms\n", elapsed / ITERATIONS);

    return EXIT_SUCCESS;
}

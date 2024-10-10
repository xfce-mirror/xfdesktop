/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2022 Brian Tarricone, <brian@tarricone.org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk-a11y.h>

#include <libxfce4util/libxfce4util.h>

#include "xfdesktop-cell-renderer-icon-label.h"

#define DEFAULT_ALIGNMENT                     PANGO_ALIGN_LEFT
#define DEFAULT_ALIGN_SET                     FALSE
#define DEFAULT_ELLIPSIZE                     PANGO_ELLIPSIZE_NONE
#define DEFAULT_ELLIPSIZE_SET                 FALSE
#define DEFAULT_SIZE_POINTS                   8
#define DEFAULT_SIZE_POINTS_SET               FALSE
#define DEFAULT_SIZE                          (DEFAULT_SIZE_POINTS * PANGO_SCALE)
#define DEFAULT_SIZE_SET                      FALSE
#define DEFAULT_UNDERLINE_WHEN_PRELIT         FALSE
#define DEFAULT_UNSELECTED_HEIGHT             (-1)
#define DEFAULT_UNSELECTED_WIDTH              (-1)
#define DEFAULT_WRAP_MODE                     PANGO_WRAP_WORD
#define DEFAULT_WRAP_WIDTH                    (-1)

struct _XfdesktopCellRendererIconLabel
{
    GtkCellRenderer parent;

    PangoAlignment alignment;
    PangoEllipsizeMode ellipsize;
    PangoAttrList *extra_attrs;
    gint size;
    gdouble size_points;
    gchar *text;
    gint unselected_width;
    gint unselected_height;
    PangoWrapMode wrap_mode;
    gint wrap_width;

    guint align_set: 1;
    guint ellipsize_set: 1;
    guint size_set: 1;
    guint size_points_set: 1;
    guint underline_when_prelit: 1;
};

struct _XfdesktopCellRendererIconLabelClass
{
    GtkCellRendererClass parent_class;
};

enum
{
    PROP0 = 0,
    PROP_ALIGNMENT,
    PROP_ALIGN_SET,
    PROP_ATTRIBUTES,
    PROP_ELLIPSIZE,
    PROP_ELLIPSIZE_SET,
    PROP_SIZE,
    PROP_SIZE_SET,
    PROP_SIZE_POINTS,
    PROP_SIZE_POINTS_SET,
    PROP_TEXT,
    PROP_UNDERLINE_WHEN_PRELIT,
    PROP_UNSELECTED_HEIGHT,
    PROP_UNSELECTED_WIDTH,
    PROP_WRAP_MODE,
    PROP_WRAP_WIDTH,

    N_PROPS,
};

static void xfdesktop_cell_renderer_icon_label_set_property(GObject *obj,
                                                            guint prop_id,
                                                            const GValue *value,
                                                            GParamSpec *pspec);
static void xfdesktop_cell_renderer_icon_label_get_property(GObject *obj,
                                                            guint prop_id,
                                                            GValue *value,
                                                            GParamSpec *pspec);
static void xfdesktop_cell_renderer_icon_label_finalize(GObject *obj);

static void xfdesktop_cell_renderer_icon_label_render(GtkCellRenderer *cell,
                                                      cairo_t *cr,
                                                      GtkWidget *widget,
                                                      const GdkRectangle *background_area,
                                                      const GdkRectangle *cell_area,
                                                      GtkCellRendererState flags);
static void xfdesktop_cell_renderer_icon_label_get_preferred_width(GtkCellRenderer *cell,
                                                                   GtkWidget *widget,
                                                                   gint *minimal_size,
                                                                   gint *natural_size);
static void xfdesktop_cell_renderer_icon_label_get_preferred_height(GtkCellRenderer *cell,
                                                                    GtkWidget *widget,
                                                                    gint *minimal_size,
                                                                    gint *natural_size);
static void xfdesktop_cell_renderer_icon_label_get_preferred_height_for_width(GtkCellRenderer *cell,
                                                                              GtkWidget *widget,
                                                                              gint width,
                                                                              gint *minimum_height,
                                                                              gint *natural_height);
static void xfdesktop_cell_renderer_icon_label_get_preferred_width_for_height(GtkCellRenderer *cell,
                                                                              GtkWidget *widget,
                                                                              gint height,
                                                                              gint *minimum_width,
                                                                              gint *natural_width);
static void xfdesktop_cell_renderer_icon_label_get_aligned_area(GtkCellRenderer *cell,
                                                                GtkWidget *widget,
                                                                GtkCellRendererState flags,
                                                                const GdkRectangle *cell_area,
                                                                GdkRectangle *aligned_area);

static void get_size(XfdesktopCellRendererIconLabel *renderer,
                     GtkWidget *widget,
                     const GdkRectangle *cell_area,
                     PangoLayout *layout,
                     GdkRectangle *area);
static PangoLayout *create_layout(XfdesktopCellRendererIconLabel *renderer,
                                  GtkWidget *widget,
                                  const GdkRectangle *cell_area,
                                  GtkCellRendererState flags);


G_DEFINE_TYPE(XfdesktopCellRendererIconLabel, xfdesktop_cell_renderer_icon_label, GTK_TYPE_CELL_RENDERER)


static GParamSpec *obj_properties[N_PROPS] = { NULL, };


static void
xfdesktop_cell_renderer_icon_label_class_init(XfdesktopCellRendererIconLabelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkCellRendererClass *renderer_class = GTK_CELL_RENDERER_CLASS(klass);

    gobject_class->set_property = xfdesktop_cell_renderer_icon_label_set_property;
    gobject_class->get_property = xfdesktop_cell_renderer_icon_label_get_property;
    gobject_class->finalize = xfdesktop_cell_renderer_icon_label_finalize;

    renderer_class->render = xfdesktop_cell_renderer_icon_label_render;
    renderer_class->get_preferred_width = xfdesktop_cell_renderer_icon_label_get_preferred_width;
    renderer_class->get_preferred_height = xfdesktop_cell_renderer_icon_label_get_preferred_height;
    renderer_class->get_preferred_height_for_width = xfdesktop_cell_renderer_icon_label_get_preferred_height_for_width;
    renderer_class->get_preferred_width_for_height = xfdesktop_cell_renderer_icon_label_get_preferred_width_for_height;
    renderer_class->get_aligned_area = xfdesktop_cell_renderer_icon_label_get_aligned_area;

#define PARAM_FLAGS (G_PARAM_READWRITE \
                     | G_PARAM_STATIC_NAME \
                     | G_PARAM_STATIC_NICK \
                     | G_PARAM_STATIC_BLURB)

    obj_properties[PROP_ALIGNMENT] =
        g_param_spec_enum("alignment",
                          "alignment",
                          "horizontal text alignment in cell",
                          PANGO_TYPE_ALIGNMENT,
                          DEFAULT_ALIGNMENT,
                          PARAM_FLAGS);
    obj_properties[PROP_ATTRIBUTES] =
        g_param_spec_boxed("attributes",
                           "attributes",
                           "extra pango attributes to set",
                           PANGO_TYPE_ATTR_LIST,
                           PARAM_FLAGS);
    obj_properties[PROP_ELLIPSIZE] =
        g_param_spec_enum("ellipsize",
                          "ellipsize",
                          "how to ellipsize when the text is too large for the cell area",
                          PANGO_TYPE_ELLIPSIZE_MODE,
                          DEFAULT_ELLIPSIZE,
                          PARAM_FLAGS);
    obj_properties[PROP_SIZE] =
        g_param_spec_int("size",
                         "size",
                         "font size in pango units",
                         1, G_MAXINT, DEFAULT_SIZE,
                         PARAM_FLAGS);
    obj_properties[PROP_SIZE_POINTS] =
        g_param_spec_double("size-points",
                            "size-points",
                            "font size in points",
                            2, G_MAXINT, DEFAULT_SIZE_POINTS,
                            PARAM_FLAGS);
    obj_properties[PROP_TEXT] =
        g_param_spec_string("text",
                            "text",
                            "text to display in the cell",
                            NULL,
                            PARAM_FLAGS);
    obj_properties[PROP_UNSELECTED_HEIGHT] =
        g_param_spec_int("unselected-height",
                         "unselected-height",
                         "constrained height when item is not selected and mode is height-for-width",
                         -1, G_MAXINT, DEFAULT_UNSELECTED_HEIGHT,
                         PARAM_FLAGS);
    obj_properties[PROP_UNSELECTED_WIDTH] =
        g_param_spec_int("unselected-width",
                         "unselected-width",
                         "constrained width when item is not selected and mode is width-for-height",
                         -1, G_MAXINT, DEFAULT_UNSELECTED_HEIGHT,
                         PARAM_FLAGS);
    obj_properties[PROP_WRAP_MODE] =
        g_param_spec_enum("wrap-mode",
                          "wrap-mode",
                          "mode for text line wrapping",
                          PANGO_TYPE_WRAP_MODE,
                          DEFAULT_WRAP_MODE,
                          PARAM_FLAGS);
    obj_properties[PROP_WRAP_WIDTH] =
        g_param_spec_int("wrap-width",
                         "wrap-width",
                         "width to wrap text in pango units",
                         -1, G_MAXINT, DEFAULT_WRAP_WIDTH,
                         PARAM_FLAGS);

#define DEFINE_BOOL_PROP(prop_id, name, blurb, default_val) G_STMT_START{ \
    obj_properties[prop_id] = g_param_spec_boolean(name, name, blurb, default_val, PARAM_FLAGS); \
}G_STMT_END
    
    DEFINE_BOOL_PROP(PROP_ALIGN_SET, "align-set", "whether or not the 'alignment' property should be used", DEFAULT_ALIGN_SET);
    DEFINE_BOOL_PROP(PROP_ELLIPSIZE_SET, "ellipsize-set", "whether or not the 'ellipsize' property should be used", DEFAULT_ELLIPSIZE_SET);
    DEFINE_BOOL_PROP(PROP_SIZE_SET, "size-set", "whether or not the 'size' property should be used", DEFAULT_SIZE_SET);
    DEFINE_BOOL_PROP(PROP_SIZE_POINTS_SET, "size-points-set", "whether or not the 'size-points' property should be used", DEFAULT_SIZE_POINTS_SET);
    DEFINE_BOOL_PROP(PROP_UNDERLINE_WHEN_PRELIT, "underline-when-prelit", "whether or not to underline text when the PRELIT flag is set", DEFAULT_UNDERLINE_WHEN_PRELIT);
    
#undef DEFINE_SETTER
#undef PARAM_FLAGS

    g_object_class_install_properties(gobject_class, G_N_ELEMENTS(obj_properties), obj_properties);
    gtk_cell_renderer_class_set_accessible_type(renderer_class, GTK_TYPE_TEXT_CELL_ACCESSIBLE);
}

static void
xfdesktop_cell_renderer_icon_label_init(XfdesktopCellRendererIconLabel *renderer)
{
    renderer->alignment = DEFAULT_ALIGNMENT;
    renderer->align_set = DEFAULT_ALIGN_SET;
    renderer->ellipsize = DEFAULT_ELLIPSIZE;
    renderer->ellipsize_set = DEFAULT_ELLIPSIZE_SET;
    renderer->extra_attrs = NULL;
    renderer->size = DEFAULT_SIZE;
    renderer->size_set = DEFAULT_SIZE_SET;
    renderer->text = NULL;
    renderer->unselected_height = DEFAULT_UNSELECTED_HEIGHT;
    renderer->unselected_width = DEFAULT_UNSELECTED_WIDTH;
    renderer->wrap_mode = DEFAULT_WRAP_MODE;
    renderer->wrap_width = DEFAULT_WRAP_WIDTH;
}

static void
xfdesktop_cell_renderer_icon_label_set_property(GObject *obj,
                                                guint prop_id,
                                                const GValue *value,
                                                GParamSpec *pspec)
{
    XfdesktopCellRendererIconLabel *renderer = XFDESKTOP_CELL_RENDERER_ICON_LABEL(obj);

    switch (prop_id) {
        case PROP_ALIGNMENT:
            renderer->alignment = g_value_get_enum(value);
            break;
        case PROP_ALIGN_SET:
            renderer->align_set = g_value_get_boolean(value);
            break;
        case PROP_ATTRIBUTES:
            if (renderer->extra_attrs != NULL) {
                pango_attr_list_unref(renderer->extra_attrs);
            }
            renderer->extra_attrs = g_value_dup_boxed(value);
            break;
        case PROP_ELLIPSIZE:
            renderer->ellipsize = g_value_get_enum(value);
            break;
        case PROP_ELLIPSIZE_SET:
            renderer->ellipsize_set = g_value_get_boolean(value);
            break;
        case PROP_SIZE:
            renderer->size = g_value_get_int(value);
            break;
        case PROP_SIZE_SET:
            renderer->size_set = g_value_get_boolean(value);
            break;
        case PROP_SIZE_POINTS:
            renderer->size_points = g_value_get_double(value);
            break;
        case PROP_SIZE_POINTS_SET:
            renderer->size_points_set = g_value_get_boolean(value);
            break;
        case PROP_TEXT:
            g_free(renderer->text);
            renderer->text = g_value_dup_string(value);
            break;
        case PROP_UNDERLINE_WHEN_PRELIT:
            renderer->underline_when_prelit = g_value_get_boolean(value);
            break;
        case PROP_UNSELECTED_HEIGHT:
            renderer->unselected_height = g_value_get_int(value);
            break;
        case PROP_UNSELECTED_WIDTH:
            renderer->unselected_width = g_value_get_int(value);
            break;
        case PROP_WRAP_MODE:
            renderer->wrap_mode = g_value_get_enum(value);
            break;
        case PROP_WRAP_WIDTH:
            renderer->wrap_width = g_value_get_int(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
            break;
    }
}

static void
xfdesktop_cell_renderer_icon_label_get_property(GObject *obj,
                                                guint prop_id,
                                                GValue *value,
                                                GParamSpec *pspec)
{
    XfdesktopCellRendererIconLabel *renderer = XFDESKTOP_CELL_RENDERER_ICON_LABEL(obj);

    switch (prop_id) {
        case PROP_ALIGNMENT:
            g_value_set_enum(value, renderer->alignment);
            break;
        case PROP_ALIGN_SET:
            g_value_set_boolean(value, renderer->align_set);
            break;
        case PROP_ATTRIBUTES:
            g_value_set_boxed(value, renderer->extra_attrs);
            break;
        case PROP_ELLIPSIZE:
            g_value_set_enum(value, renderer->ellipsize);
            break;
        case PROP_ELLIPSIZE_SET:
            g_value_set_boolean(value, renderer->ellipsize_set);
            break;
        case PROP_SIZE:
            g_value_set_int(value, renderer->size);
            break;
        case PROP_SIZE_SET:
            g_value_set_boolean(value, renderer->size_set);
            break;
        case PROP_SIZE_POINTS:
            g_value_set_double(value, renderer->size_points);
            break;
        case PROP_SIZE_POINTS_SET:
            g_value_set_boolean(value, renderer->size_points_set);
            break;
        case PROP_TEXT:
            g_value_set_string(value, renderer->text);
            break;
        case PROP_UNDERLINE_WHEN_PRELIT:
            g_value_set_boolean(value, renderer->underline_when_prelit);
            break;
        case PROP_UNSELECTED_HEIGHT:
            g_value_set_int(value, renderer->unselected_height);
            break;
        case PROP_UNSELECTED_WIDTH:
            g_value_set_int(value, renderer->unselected_width);
            break;
        case PROP_WRAP_MODE:
            g_value_set_enum(value, renderer->wrap_mode);
            break;
        case PROP_WRAP_WIDTH:
            g_value_set_int(value, renderer->wrap_width);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
            return;
    }

    g_object_notify_by_pspec(obj, pspec);
}

static void
xfdesktop_cell_renderer_icon_label_finalize(GObject *obj)
{
    XfdesktopCellRendererIconLabel *renderer = XFDESKTOP_CELL_RENDERER_ICON_LABEL(obj);

    if (renderer->extra_attrs) {
        pango_attr_list_unref(renderer->extra_attrs);
    }

    g_free(renderer->text);

    G_OBJECT_CLASS(xfdesktop_cell_renderer_icon_label_parent_class)->finalize(obj);
}

static void
xfdesktop_cell_renderer_icon_label_render(GtkCellRenderer *cell,
                                          cairo_t *cr,
                                          GtkWidget *widget,
                                          const GdkRectangle *background_area,
                                          const GdkRectangle *cell_area,
                                          GtkCellRendererState flags)
{
    XfdesktopCellRendererIconLabel *renderer = XFDESKTOP_CELL_RENDERER_ICON_LABEL(cell);
    GtkStyleContext *style_context = gtk_widget_get_style_context(widget);
    PangoLayout *layout;
    GdkRectangle box_area;
    gint xpad, ypad;
    PangoRectangle extents;
    gint rtl_offset;

    gtk_style_context_save(style_context);
    gtk_style_context_add_class(style_context, GTK_STYLE_CLASS_LABEL);

    gtk_cell_renderer_get_padding(cell, &xpad, &ypad);
    layout = create_layout(renderer, widget, cell_area, flags);
    get_size(renderer, widget, cell_area, layout, &box_area);

    pango_layout_get_pixel_extents(layout, NULL, &extents);
    rtl_offset = extents.x;

    box_area.x += cell_area->x;
    box_area.y += cell_area->y;

#if 0
    DBG("style: %s", gtk_style_context_to_string(style_context, 0));
    DBG("cell_area: %dx%d+%d+%d", cell_area->width, cell_area->height, cell_area->x, cell_area->y);
    DBG("box_area:  %dx%d+%d+%d", box_area.width, box_area.height, box_area.x, box_area.y);
    DBG("text_ext:  %dx%d+%d+%d", box_area.width - xpad * 2, box_area.height - ypad * 2, box_area.x + xpad, box_area.y + ypad);
    DBG("text loc:       +%d+%d", box_area.x + xpad, box_area.y + ypad);
    DBG("text ext:  %dx%d+%d+%d", extents.width, extents.height, extents.x, extents.y);
#endif

    gtk_render_background(style_context, cr,
                          box_area.x, box_area.y,
                          box_area.width, box_area.height);

    gtk_render_frame(style_context, cr,
                     box_area.x, box_area.y,
                     box_area.width, box_area.height);

    if (flags & GTK_CELL_RENDERER_FOCUSED) {
        gtk_render_focus(style_context, cr,
                         box_area.x, box_area.y,
                         box_area.width, box_area.height);
    }

    gtk_render_layout(style_context, cr, box_area.x - rtl_offset + xpad, box_area.y + ypad, layout);

#if 0
    cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);
    cairo_rectangle(cr, box_area.x + xpad, box_area.y + xpad, box_area.width - xpad * 2, box_area.height - ypad * 2);
    cairo_stroke(cr);
#endif

    gtk_style_context_restore(style_context);
    g_object_unref(layout);
}

// Inspired by GtkCellRendererText's implementation
static void
xfdesktop_cell_renderer_icon_label_get_preferred_width(GtkCellRenderer *cell,
                                                       GtkWidget *widget,
                                                       gint *minimal_size,
                                                       gint *natural_size)
{
    XfdesktopCellRendererIconLabel *renderer = XFDESKTOP_CELL_RENDERER_ICON_LABEL(cell);
    PangoLayout *layout;
    PangoRectangle extents;
    gint xpad;
    gint min_width;

    layout = create_layout(renderer, widget, NULL, 0);
    gtk_cell_renderer_get_padding(cell, &xpad, NULL);

    pango_layout_set_width(layout, -1);
    pango_layout_get_pixel_extents(layout, NULL, &extents);

    if (renderer->ellipsize_set && renderer->ellipsize != PANGO_ELLIPSIZE_NONE) {
        const gint ellipsize_chars = 3;
        PangoContext *context = pango_layout_get_context(layout);
        PangoFontMetrics *metrics = pango_context_get_metrics(context,
                                                              pango_context_get_font_description(context),
                                                              pango_context_get_language(context));
        gint char_width = pango_font_metrics_get_approximate_char_width(metrics);
        min_width = MIN(extents.width, PANGO_PIXELS(char_width) * ellipsize_chars);
    } else if (renderer->wrap_width > 0) {
        min_width = extents.x + MIN(extents.width, renderer->wrap_width);
    } else {
        min_width = extents.x + extents.width;
    }
    min_width += xpad * 2;

    if (minimal_size != NULL) {
        *minimal_size = min_width;
    }

    if (natural_size != NULL) {
        *natural_size = MAX(min_width, extents.width + xpad * 2);
    }

    g_object_unref(layout);
}

static void
xfdesktop_cell_renderer_icon_label_get_preferred_height(GtkCellRenderer *cell,
                                                        GtkWidget *widget,
                                                        gint *minimal_size,
                                                        gint *natural_size)
{
    XfdesktopCellRendererIconLabel *renderer = XFDESKTOP_CELL_RENDERER_ICON_LABEL(cell);
    PangoLayout *layout;
    PangoRectangle extents;
    gint ypad;
    gint min_height, nat_height;

    layout = create_layout(renderer, widget, NULL, 0);
    gtk_cell_renderer_get_padding(cell, NULL, &ypad);

    pango_layout_get_pixel_extents(layout, NULL, &extents);
    nat_height = extents.height + ypad * 2;

    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_NONE);
    pango_layout_set_height(layout, -1);
    pango_layout_get_extents(layout, NULL, &extents);
    min_height = extents.height + ypad * 2;

    if (*minimal_size) {
        *minimal_size = min_height;
    }

    if (*natural_size) {
        *natural_size = MAX(min_height, nat_height);
    }

    g_object_unref(layout);
}

static void
xfdesktop_cell_renderer_icon_label_get_preferred_height_for_width(GtkCellRenderer *cell,
                                                                  GtkWidget *widget,
                                                                  gint width,
                                                                  gint *minimum_height,
                                                                  gint *natural_height)
{
    XfdesktopCellRendererIconLabel *renderer = XFDESKTOP_CELL_RENDERER_ICON_LABEL(cell);
    PangoLayout *layout;
    gint xpad, ypad;
    gint min_height, nat_height;

    gtk_cell_renderer_get_padding(cell, &xpad, &ypad);

    layout = create_layout(renderer, widget, NULL, 0);
    pango_layout_set_width(layout, (width - xpad * 2) * PANGO_SCALE);
    pango_layout_get_pixel_size(layout, NULL, &min_height);

    pango_layout_set_height(layout, -1);
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_NONE);
    pango_layout_get_pixel_size(layout, NULL, &nat_height);

    if (minimum_height != NULL) {
        *minimum_height = min_height + ypad * 2;
    }
    if (natural_height != NULL) {
        *natural_height = MAX(nat_height, min_height) + ypad * 2;
    }

    g_object_unref(layout);
}

static void
xfdesktop_cell_renderer_icon_label_get_preferred_width_for_height(GtkCellRenderer *cell,
                                                                  GtkWidget *widget,
                                                                  gint height,
                                                                  gint *minimum_width,
                                                                  gint *natural_width)
{
    XfdesktopCellRendererIconLabel *renderer = XFDESKTOP_CELL_RENDERER_ICON_LABEL(cell);
    PangoLayout *layout;
    gint xpad, ypad;
    gint min_width, nat_width;

    gtk_cell_renderer_get_padding(cell, &xpad, &ypad);

    layout = create_layout(renderer, widget, NULL, 0);
    pango_layout_set_height(layout, (height - ypad * 2) * PANGO_SCALE);
    pango_layout_get_pixel_size(layout, &min_width, NULL);

    pango_layout_set_width(layout, -1);
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_NONE);
    pango_layout_get_pixel_size(layout, &nat_width, NULL);

    if (minimum_width != NULL) {
        *minimum_width = min_width + ypad * 2;
    }
    if (natural_width != NULL) {
        *natural_width = MAX(nat_width, min_width) + ypad * 2;
    }

    g_object_unref(layout);
}

static void
xfdesktop_cell_renderer_icon_label_get_aligned_area(GtkCellRenderer *cell,
                                                    GtkWidget *widget,
                                                    GtkCellRendererState flags,
                                                    const GdkRectangle *cell_area,
                                                    GdkRectangle *aligned_area)
{
    XfdesktopCellRendererIconLabel *renderer = XFDESKTOP_CELL_RENDERER_ICON_LABEL(cell);
    PangoLayout *layout;

    layout = create_layout(renderer, widget, cell_area, flags);
    get_size(renderer, widget, cell_area, layout, aligned_area);

#if 0
    DBG("label area: %dx%d+%d+%d", aligned_area->width, aligned_area->height, aligned_area->x, aligned_area->y);
#endif

    g_object_unref(layout);
}

static void
get_size(XfdesktopCellRendererIconLabel *renderer,
         GtkWidget *widget,
         const GdkRectangle *cell_area,
         PangoLayout *layout,
         GdkRectangle *area)
{
    PangoRectangle extents;
    gint xpad, ypad;

    gtk_cell_renderer_get_padding(GTK_CELL_RENDERER(renderer), &xpad, &ypad);
    pango_layout_get_pixel_extents(layout, NULL, &extents);

    if (cell_area != NULL) {
        gfloat xalign, yalign;

        gtk_cell_renderer_get_alignment(GTK_CELL_RENDERER(renderer), &xalign, &yalign);
        
        extents.width = MIN(extents.width, cell_area->width - xpad * 2);
        extents.height = MIN(extents.height, cell_area->height - ypad * 2);

        if (gtk_widget_get_direction(widget) == GTK_TEXT_DIR_RTL) {
            extents.x = (1.0 - xalign) * (cell_area->width - extents.width - xpad * 2);
        } else {
            extents.x = xalign * (cell_area->width - extents.width - xpad * 2);
        }

        extents.y = MAX(0, yalign * (cell_area->height - extents.height - ypad * 2));
    } else {
        extents.x = 0;
        extents.y = 0;
    }

    area->x = extents.x;
    area->y = extents.y;
    area->width = extents.width + xpad * 2;
    area->height = extents.height + ypad * 2;
}

static PangoLayout *
create_layout(XfdesktopCellRendererIconLabel *renderer,
              GtkWidget *widget,
              const GdkRectangle *cell_area,
              GtkCellRendererState flags)
{
    gboolean is_selected = (flags & GTK_CELL_RENDERER_SELECTED) != 0;
    PangoLayout *layout;
    PangoAttrList *attr_list;
    gint xpad, ypad;
    gint fixed_width, fixed_height;

    gtk_cell_renderer_get_padding(GTK_CELL_RENDERER(renderer), &xpad, &ypad);
    gtk_cell_renderer_get_fixed_size(GTK_CELL_RENDERER(renderer), &fixed_width, &fixed_height);

    layout = gtk_widget_create_pango_layout(widget, renderer->text);
    if (renderer->extra_attrs != NULL) {
        attr_list = pango_attr_list_copy(renderer->extra_attrs);
    } else {
        attr_list = pango_attr_list_new();
    }

    if (renderer->underline_when_prelit && (flags & GTK_CELL_RENDERER_PRELIT) != 0) {
        PangoAttribute *uline = pango_attr_underline_new(PANGO_UNDERLINE_SINGLE);
        uline->start_index = 0;
        uline->end_index = -1;
        pango_attr_list_change(attr_list, uline);
    }

    if (renderer->align_set) {
        pango_layout_set_alignment(layout, renderer->alignment);
    } else {
        if (gtk_widget_get_direction(widget) == GTK_TEXT_DIR_RTL) {
            pango_layout_set_alignment(layout, PANGO_ALIGN_RIGHT);
        } else {
            pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);
        }
    }

    if (renderer->ellipsize_set) {
        pango_layout_set_ellipsize(layout, renderer->ellipsize);
    }

    if (renderer->size_set || renderer->size_points_set) {
        PangoFontDescription *pfd = pango_context_get_font_description(gtk_widget_get_pango_context(widget));
        pfd = pango_font_description_copy(pfd);
        if (renderer->size_set) {
            pango_font_description_set_size(pfd, renderer->size);
        } else if (renderer->size_points_set) {
            pango_font_description_set_size(pfd, renderer->size_points * PANGO_SCALE);
        }
        pango_layout_set_font_description(layout, pfd);
        pango_font_description_free(pfd);
    }

    pango_layout_set_attributes(layout, attr_list);
    pango_attr_list_unref(attr_list);

    if (renderer->wrap_width == -1 || (is_selected && gtk_cell_renderer_get_request_mode(GTK_CELL_RENDERER(renderer)) == GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT)) {
        pango_layout_set_width(layout, -1);
        pango_layout_set_wrap(layout, PANGO_WRAP_CHAR);
    } else {
        gint width;
        PangoRectangle extents;

        if (fixed_width < 0) {
            fixed_width = renderer->unselected_width;
        }

        if (fixed_width > 0) {
            pango_layout_set_width(layout, (fixed_width - xpad * 2) * PANGO_SCALE);
        }
        pango_layout_get_extents(layout, NULL, &extents);

        if (cell_area != NULL) {
            width = MIN((cell_area->width - xpad * 2) * PANGO_SCALE, extents.width + xpad * 2);
        } else {
            width = MIN(renderer->wrap_width * PANGO_SCALE, extents.width + xpad * 2);
        }
        pango_layout_set_width(layout, MIN(extents.width, width));
        pango_layout_set_wrap(layout, renderer->wrap_mode);
    }

    if (is_selected && gtk_cell_renderer_get_request_mode(GTK_CELL_RENDERER(renderer)) == GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH) {
        pango_layout_set_height(layout, -1);
        pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_NONE);
    } else {
        PangoRectangle extents;

        if (fixed_height < 0) {
            fixed_height = renderer->unselected_height;
        }

        if (fixed_height > 0) {
            pango_layout_set_height(layout, (fixed_height - ypad * 2) * PANGO_SCALE);
        }
        pango_layout_get_extents(layout, NULL, &extents);

        if (cell_area != NULL) {
            gint height = (cell_area->height - ypad * 2) * PANGO_SCALE;
            pango_layout_set_height(layout, MIN(extents.height, height));
        } else {
            pango_layout_set_height(layout, extents.height);
        }
    }

    return layout;
}

GtkCellRenderer *
xfdesktop_cell_renderer_icon_label_new(void)
{
    return g_object_new(XFDESKTOP_TYPE_CELL_RENDERER_ICON_LABEL, NULL);
}

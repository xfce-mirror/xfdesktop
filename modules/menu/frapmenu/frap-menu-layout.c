/* $Id: frap-menu-layout.c 25219 2007-03-19 12:52:26Z jannis $ */
/* vi:set expandtab sw=2 sts=2: */
/*-
 * Copyright (c) 2007 Jannis Pohlmann <jannis@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <frap-menu-layout.h>



#define FRAP_MENU_LAYOUT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), FRAP_TYPE_MENU_LAYOUT, FrapMenuLayoutPrivate))



/* Property identifiers */
enum
{
  PROP_0,
};



struct _FrapMenuLayoutNode
{
  FrapMenuLayoutNodeType   type;
  union
  {
    gchar                  *filename;
    gchar                  *menuname;
    FrapMenuLayoutMergeType merge_type;
  } data;
};



static void frap_menu_layout_class_init   (FrapMenuLayoutClass *klass);
static void frap_menu_layout_init         (FrapMenuLayout      *layout);
static void frap_menu_layout_finalize     (GObject             *object);
static void frap_menu_layout_get_property (GObject             *object,
                                           guint                prop_id,
                                           GValue              *value,
                                           GParamSpec          *pspec);
static void frap_menu_layout_set_property (GObject             *object,
                                           guint                prop_id,
                                           const GValue        *value,
                                           GParamSpec          *pspec);
static void frap_menu_layout_free_node    (FrapMenuLayoutNode  *node);



struct _FrapMenuLayoutClass
{
  GObjectClass __parent__;
};

struct _FrapMenuLayoutPrivate
{
  GSList *nodes;
};

struct _FrapMenuLayout
{
  GObject __parent__;

  /* < private > */
  FrapMenuLayoutPrivate *priv;
};



static GObjectClass *frap_menu_layout_parent_class = NULL;



GType
frap_menu_layout_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (FrapMenuLayoutClass),
        NULL,
        NULL,
        (GClassInitFunc) frap_menu_layout_class_init,
        NULL,
        NULL,
        sizeof (FrapMenuLayout),
        0,
        (GInstanceInitFunc) frap_menu_layout_init,
        NULL,
      };

      type = g_type_register_static (G_TYPE_OBJECT, "FrapMenuLayout", &info, 0);
    }

  return type;
}



static void
frap_menu_layout_class_init (FrapMenuLayoutClass *klass)
{
  GObjectClass *gobject_class;

  g_type_class_add_private (klass, sizeof (FrapMenuLayoutPrivate));

  /* Determine parent type class */
  frap_menu_layout_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = frap_menu_layout_finalize;
  gobject_class->get_property = frap_menu_layout_get_property;
  gobject_class->set_property = frap_menu_layout_set_property;
}



static void
frap_menu_layout_init (FrapMenuLayout *layout)
{
  layout->priv = FRAP_MENU_LAYOUT_GET_PRIVATE (layout);
  layout->priv->nodes = NULL;
}



static void
frap_menu_layout_finalize (GObject *object)
{
  FrapMenuLayout *layout = FRAP_MENU_LAYOUT (object);

  /* Free nodes */
  g_slist_foreach (layout->priv->nodes, (GFunc) frap_menu_layout_free_node, NULL);
  g_slist_free (layout->priv->nodes);

  (*G_OBJECT_CLASS (frap_menu_layout_parent_class)->finalize) (object);
}



static void
frap_menu_layout_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  FrapMenuLayout *layout = FRAP_MENU_LAYOUT (layout);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
frap_menu_layout_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  FrapMenuLayout *layout = FRAP_MENU_LAYOUT (layout);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



FrapMenuLayout*
frap_menu_layout_new (void)
{
  return g_object_new (FRAP_TYPE_MENU_LAYOUT, NULL);
}



static void
frap_menu_layout_free_node (FrapMenuLayoutNode *node)
{
  if (node->type == FRAP_MENU_LAYOUT_NODE_FILENAME)
    g_free (node->data.filename);
  else if (node->type == FRAP_MENU_LAYOUT_NODE_MENUNAME)
    g_free (node->data.menuname);

  g_free (node);
}



void
frap_menu_layout_add_filename (FrapMenuLayout *layout,
                               const gchar    *filename)
{
  g_return_if_fail (FRAP_IS_MENU_LAYOUT (layout));
  g_return_if_fail (filename != NULL);

  /* Build filename node */
  FrapMenuLayoutNode *node = g_new0 (FrapMenuLayoutNode, 1);
  node->type = FRAP_MENU_LAYOUT_NODE_FILENAME;
  node->data.filename = g_strdup (filename);

  /* Append node to the list */
  layout->priv->nodes = g_slist_append (layout->priv->nodes, node);
}



void
frap_menu_layout_add_menuname (FrapMenuLayout *layout,
                               const gchar    *menuname)
{
  g_return_if_fail (FRAP_IS_MENU_LAYOUT (layout));
  g_return_if_fail (menuname != NULL);

  /* Build menuname node */
  FrapMenuLayoutNode *node = g_new0 (FrapMenuLayoutNode, 1);
  node->type = FRAP_MENU_LAYOUT_NODE_MENUNAME;
  node->data.menuname = g_strdup (menuname);

  /* Append node to the list */
  layout->priv->nodes = g_slist_append (layout->priv->nodes, node);
}



void
frap_menu_layout_add_separator (FrapMenuLayout *layout)
{
  g_return_if_fail (FRAP_IS_MENU_LAYOUT (layout));

  /* Build separator node */
  FrapMenuLayoutNode *node = g_new0 (FrapMenuLayoutNode, 1);
  node->type = FRAP_MENU_LAYOUT_NODE_SEPARATOR;

  /* Append node to the list */
  layout->priv->nodes = g_slist_append (layout->priv->nodes, node);
}



void
frap_menu_layout_add_merge (FrapMenuLayout         *layout,
                            FrapMenuLayoutMergeType type)
{
  g_return_if_fail (FRAP_IS_MENU_LAYOUT (layout));

  /* Build merge node */
  FrapMenuLayoutNode *node = g_new0 (FrapMenuLayoutNode, 1);
  node->type = FRAP_MENU_LAYOUT_NODE_MERGE;
  node->data.merge_type = type;

  /* Append node to the list */
  layout->priv->nodes = g_slist_append (layout->priv->nodes, node);
}



GSList*
frap_menu_layout_get_nodes (FrapMenuLayout *layout)
{
  g_return_val_if_fail (FRAP_IS_MENU_LAYOUT (layout), NULL);
  return layout->priv->nodes;
}



gboolean 
frap_menu_layout_get_filename_used (FrapMenuLayout *layout,
                                    const gchar    *filename)
{
  FrapMenuLayoutNode *node;
  GSList             *iter;
  gboolean            found = FALSE;

  g_return_val_if_fail (FRAP_IS_MENU_LAYOUT (layout), FALSE);
  g_return_val_if_fail (filename != NULL, FALSE);

  for (iter = layout->priv->nodes; iter != NULL; iter = g_slist_next (iter))
    {
      node = (FrapMenuLayoutNode *)iter->data;

      if (G_UNLIKELY (node == NULL))
        continue;

      if (G_UNLIKELY (node->type == FRAP_MENU_LAYOUT_NODE_FILENAME && g_utf8_collate (node->data.filename, filename) == 0))
        {
          found = TRUE;
          break;
        }
    }

  return found;
}



gboolean 
frap_menu_layout_get_menuname_used (FrapMenuLayout *layout,
                                    const gchar    *menuname)
{
  FrapMenuLayoutNode *node;
  GSList             *iter;
  gboolean            found = FALSE;

  g_return_val_if_fail (FRAP_IS_MENU_LAYOUT (layout), FALSE);
  g_return_val_if_fail (menuname != NULL, FALSE);

  for (iter = layout->priv->nodes; iter != NULL; iter = g_slist_next (iter))
    {
      node = (FrapMenuLayoutNode *)iter->data;

      if (G_UNLIKELY (node == NULL))
        continue;

      if (G_UNLIKELY (node->type == FRAP_MENU_LAYOUT_NODE_MENUNAME && g_utf8_collate (node->data.menuname, menuname) == 0))
        {
          found = TRUE;
          break;
        }
    }

  return found;
}



FrapMenuLayoutNodeType
frap_menu_layout_node_get_type (FrapMenuLayoutNode *node)
{
  g_return_val_if_fail (node != NULL, FRAP_MENU_LAYOUT_NODE_INVALID);
  return node->type;
}



const gchar*
frap_menu_layout_node_get_filename (FrapMenuLayoutNode *node)
{
  g_return_val_if_fail (node != NULL && node->type == FRAP_MENU_LAYOUT_NODE_FILENAME, NULL);
  return node->data.filename;
}




const gchar*
frap_menu_layout_node_get_menuname (FrapMenuLayoutNode *node)
{
  g_return_val_if_fail (node != NULL && node->type == FRAP_MENU_LAYOUT_NODE_MENUNAME, NULL);
  return node->data.menuname;
}




FrapMenuLayoutMergeType
frap_menu_layout_node_get_merge_type (FrapMenuLayoutNode *node)
{
  g_return_val_if_fail (node != NULL && node->type == FRAP_MENU_LAYOUT_NODE_MERGE, FRAP_MENU_LAYOUT_MERGE_ALL);
  return node->data.merge_type;
}

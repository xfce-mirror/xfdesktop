/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Copied, renamed, and hacked to pieces by Brian Tarricone <bjt23@cornell.edu>.
 * Original code from Thunar.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>

#include "xfdesktop-file-icon.h"
#include "xfdesktop-clipboard-manager.h"



enum
{
  CHANGED,
  LAST_SIGNAL,
};

enum
{
  TARGET_GNOME_COPIED_FILES,
  TARGET_UTF8_STRING,
};



static void xfdesktop_clipboard_manager_class_init         (XfdesktopClipboardManagerClass *klass);
static void xfdesktop_clipboard_manager_init               (XfdesktopClipboardManager      *manager);
static void xfdesktop_clipboard_manager_finalize           (GObject                        *object);
static void xfdesktop_clipboard_manager_file_destroyed     (XfdesktopClipboardManager      *manager,
                                                            XfdesktopFileIcon              *file);
static void xfdesktop_clipboard_manager_owner_changed      (GtkClipboard                   *clipboard,
                                                            GdkEventOwnerChange            *event,
                                                            XfdesktopClipboardManager      *manager);
static void xfdesktop_clipboard_manager_targets_received   (GtkClipboard                   *clipboard,
                                                            GtkSelectionData               *selection_data,
                                                            gpointer                        user_data);
static void xfdesktop_clipboard_manager_get_callback       (GtkClipboard                   *clipboard,
                                                            GtkSelectionData               *selection_data,
                                                            guint                           info,
                                                            gpointer                        user_data);
static void xfdesktop_clipboard_manager_clear_callback     (GtkClipboard                   *clipboard,
                                                            gpointer                        user_data);
static void xfdesktop_clipboard_manager_transfer_files     (XfdesktopClipboardManager      *manager,
                                                            gboolean                        copy,
                                                            GList                          *files);



struct _XfdesktopClipboardManagerClass
{
  GObjectClass __parent__;

  void (*changed) (XfdesktopClipboardManager *manager);
};

struct _XfdesktopClipboardManager
{
  GObject __parent__;

  GtkClipboard *clipboard;
  gboolean      can_paste;
  GdkAtom       x_special_gnome_copied_files;

  gboolean      files_cutted;
  GList        *files;
};

typedef struct
{
  XfdesktopClipboardManager *manager;
  ThunarVfsPath          *target_path;
  GtkWidget              *widget;
  GClosure               *new_files_closure;
} XfdesktopClipboardPasteRequest;



static const GtkTargetEntry clipboard_targets[] =
{
  { "x-special/gnome-copied-files", 0, TARGET_GNOME_COPIED_FILES },
  { "UTF8_STRING", 0, TARGET_UTF8_STRING }
};

static GObjectClass *xfdesktop_clipboard_manager_parent_class;
static GQuark        xfdesktop_clipboard_manager_quark = 0;
static guint         manager_signals[LAST_SIGNAL];



GType
xfdesktop_clipboard_manager_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (XfdesktopClipboardManagerClass),
        NULL,
        NULL,
        (GClassInitFunc) xfdesktop_clipboard_manager_class_init,
        NULL,
        NULL,
        sizeof (XfdesktopClipboardManager),
        0,
        (GInstanceInitFunc) xfdesktop_clipboard_manager_init,
        NULL,
      };

      type = g_type_register_static (G_TYPE_OBJECT, I_("XfdesktopClipboardManager"), &info, 0);
    }

  return type;
}



static void
xfdesktop_clipboard_manager_class_init (XfdesktopClipboardManagerClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the parent type class */
  xfdesktop_clipboard_manager_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = xfdesktop_clipboard_manager_finalize;

  /**
   * XfdesktopClipboardManager::changed:
   * @manager : a #XfdesktopClipboardManager.
   *
   * This signal is emitted whenever the contents of the
   * clipboard associated with @manager changes.
   **/
  manager_signals[CHANGED] =
    g_signal_new (I_("changed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (XfdesktopClipboardManagerClass, changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}



static void
xfdesktop_clipboard_manager_init (XfdesktopClipboardManager *manager)
{
  manager->x_special_gnome_copied_files = gdk_atom_intern ("x-special/gnome-copied-files", FALSE);
}



static void
xfdesktop_clipboard_manager_finalize (GObject *object)
{
  XfdesktopClipboardManager *manager = XFDESKTOP_CLIPBOARD_MANAGER (object);
  GList                  *lp;

  /* release any pending files */
  for (lp = manager->files; lp != NULL; lp = lp->next)
    {
      g_object_weak_unref(G_OBJECT(lp->data),
                          (GWeakNotify)xfdesktop_clipboard_manager_file_destroyed,
                          manager);
      g_object_unref (G_OBJECT (lp->data));
    }
  g_list_free (manager->files);

  /* disconnect from the clipboard */
  g_signal_handlers_disconnect_by_func (G_OBJECT (manager->clipboard), xfdesktop_clipboard_manager_owner_changed, manager);
  g_object_set_qdata (G_OBJECT (manager->clipboard), xfdesktop_clipboard_manager_quark, NULL);
  g_object_unref (G_OBJECT (manager->clipboard));

  (*G_OBJECT_CLASS (xfdesktop_clipboard_manager_parent_class)->finalize) (object);
}



static void
xfdesktop_clipboard_manager_file_destroyed (XfdesktopClipboardManager *manager,
                                            XfdesktopFileIcon             *file)
{
  g_return_if_fail (XFDESKTOP_IS_CLIPBOARD_MANAGER (manager));
  g_return_if_fail (g_list_find (manager->files, file) != NULL);

  /* remove the file from our list */
  manager->files = g_list_remove (manager->files, file);

  /* disconnect from the file */
  g_object_weak_unref(G_OBJECT (file),
                      (GWeakNotify)xfdesktop_clipboard_manager_file_destroyed,
                      manager);
  g_object_unref (G_OBJECT (file));
}



static void
xfdesktop_clipboard_manager_owner_changed (GtkClipboard           *clipboard,
                                           GdkEventOwnerChange    *event,
                                           XfdesktopClipboardManager *manager)
{
  g_return_if_fail (GTK_IS_CLIPBOARD (clipboard));
  g_return_if_fail (XFDESKTOP_IS_CLIPBOARD_MANAGER (manager));
  g_return_if_fail (manager->clipboard == clipboard);

  /* need to take a reference on the manager, because the clipboards
   * "targets received callback" mechanism is not cancellable.
   */
  g_object_ref (G_OBJECT (manager));

  /* request the list of supported targets from the new owner */
  gtk_clipboard_request_contents (clipboard, gdk_atom_intern ("TARGETS", FALSE),
                                  xfdesktop_clipboard_manager_targets_received, manager);
}



static void
xfdesktop_clipboard_manager_targets_received (GtkClipboard     *clipboard,
                                              GtkSelectionData *selection_data,
                                              gpointer          user_data)
{
  XfdesktopClipboardManager *manager = XFDESKTOP_CLIPBOARD_MANAGER (user_data);
#if 0
  GdkAtom                *targets;
  gint                    n_targets;
  gint                    n;
#endif
  
  g_return_if_fail (GTK_IS_CLIPBOARD (clipboard));
  g_return_if_fail (XFDESKTOP_IS_CLIPBOARD_MANAGER (manager));
  g_return_if_fail (manager->clipboard == clipboard);

#if 0
  /* reset the "can-paste" state */
  manager->can_paste = FALSE;

  /* check the list of targets provided by the owner */
  if (gtk_selection_data_get_targets (selection_data, &targets, &n_targets))
    {
      for (n = 0; n < n_targets; ++n)
        if (targets[n] == manager->x_special_gnome_copied_files)
          {
            manager->can_paste = TRUE;
            break;
          }

      g_free (targets);
    }
#endif
  
  /* notify listeners that we have a new clipboard state */
  g_signal_emit (G_OBJECT (manager), manager_signals[CHANGED], 0);
#if 0
  g_object_notify (G_OBJECT (manager), "can-paste");
#endif

  /* drop the reference taken for the callback */
  g_object_unref (G_OBJECT (manager));
}


static void
xfdesktop_clipboard_manager_get_callback (GtkClipboard     *clipboard,
                                          GtkSelectionData *selection_data,
                                          guint             target_info,
                                          gpointer          user_data)
{
  XfdesktopClipboardManager *manager = XFDESKTOP_CLIPBOARD_MANAGER (user_data);
  GList                  *path_list = NULL;
  gchar                  *string_list;
  gchar                  *data;

  g_return_if_fail (GTK_IS_CLIPBOARD (clipboard));
  g_return_if_fail (XFDESKTOP_IS_CLIPBOARD_MANAGER (manager));
  g_return_if_fail (manager->clipboard == clipboard);

  /* determine the path list from the file list */
  path_list = xfdesktop_file_icon_list_to_path_list (manager->files);

  /* determine the string representation of the path list */
  string_list = thunar_vfs_path_list_to_string (path_list);

  switch (target_info)
    {
    case TARGET_GNOME_COPIED_FILES:
      data = g_strconcat (manager->files_cutted ? "cut\n" : "copy\n", string_list, NULL);
      gtk_selection_data_set (selection_data, selection_data->target, 8, (guchar *) data, strlen (data));
      g_free (data);
      break;

    case TARGET_UTF8_STRING:
      gtk_selection_data_set (selection_data, selection_data->target, 8, (guchar *) string_list, strlen (string_list));
      break;

    default:
      g_assert_not_reached ();
    }

  /* cleanup */
  thunar_vfs_path_list_free (path_list);
  g_free (string_list);
}



static void
xfdesktop_clipboard_manager_clear_callback (GtkClipboard *clipboard,
                                            gpointer      user_data)
{
  XfdesktopClipboardManager *manager = XFDESKTOP_CLIPBOARD_MANAGER (user_data);
  GList                  *lp;

  g_return_if_fail (GTK_IS_CLIPBOARD (clipboard));
  g_return_if_fail (XFDESKTOP_IS_CLIPBOARD_MANAGER (manager));
  g_return_if_fail (manager->clipboard == clipboard);

  /* release the pending files */
  for (lp = manager->files; lp != NULL; lp = lp->next)
    {
      g_object_weak_unref(G_OBJECT (lp->data),
                          (GWeakNotify)xfdesktop_clipboard_manager_file_destroyed,
                          manager);
      g_object_unref (G_OBJECT (lp->data));
    }
  g_list_free (manager->files);
  manager->files = NULL;
}



static void
xfdesktop_clipboard_manager_transfer_files (XfdesktopClipboardManager *manager,
                                            gboolean                copy,
                                            GList                  *files)
{
  XfdesktopFileIcon *file;
  GList      *lp;

  /* release any pending files */
  for (lp = manager->files; lp != NULL; lp = lp->next)
    {
      g_object_weak_unref(G_OBJECT (lp->data),
                          (GWeakNotify)xfdesktop_clipboard_manager_file_destroyed,
                          manager);
      g_object_unref (G_OBJECT (lp->data));
    }
  g_list_free (manager->files);

  /* remember the transfer operation */
  manager->files_cutted = !copy;

  /* setup the new file list */
  for (lp = files, manager->files = NULL; lp != NULL; lp = lp->next)
    {
      file = g_object_ref (G_OBJECT (lp->data));
      manager->files = g_list_prepend (manager->files, file);
      g_object_weak_ref(G_OBJECT(file), 
                        (GWeakNotify)xfdesktop_clipboard_manager_file_destroyed,
                        manager);
    }

  /* acquire the CLIPBOARD ownership */
  gtk_clipboard_set_with_owner (manager->clipboard, clipboard_targets,
                                G_N_ELEMENTS (clipboard_targets),
                                xfdesktop_clipboard_manager_get_callback,
                                xfdesktop_clipboard_manager_clear_callback,
                                G_OBJECT (manager));

  /* Need to fake a "owner-change" event here if the Xserver doesn't support clipboard notification */
#if GTK_CHECK_VERSION(2,6,0)
  if (!gdk_display_supports_selection_notification (gtk_clipboard_get_display (manager->clipboard)))
#endif
    {
      xfdesktop_clipboard_manager_owner_changed (manager->clipboard, NULL, manager);
    }
}



/**
 * xfdesktop_clipboard_manager_get_for_display:
 * @display : a #GdkDisplay.
 *
 * Determines the #XfdesktopClipboardManager that is used to manage
 * the clipboard on the given @display.
 *
 * The caller is responsible for freeing the returned object
 * using g_object_unref() when it's no longer needed.
 *
 * Return value: the #XfdesktopClipboardManager for @display.
 **/
XfdesktopClipboardManager*
xfdesktop_clipboard_manager_get_for_display (GdkDisplay *display)
{
  XfdesktopClipboardManager *manager;
  GtkClipboard           *clipboard;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  /* generate the quark on-demand */
  if (G_UNLIKELY (xfdesktop_clipboard_manager_quark == 0))
    xfdesktop_clipboard_manager_quark = g_quark_from_static_string ("xfdesktop-clipboard-manager");

  /* figure out the clipboard for the given display */
  clipboard = gtk_clipboard_get_for_display (display, GDK_SELECTION_CLIPBOARD);

  /* check if a clipboard manager exists */
  manager = g_object_get_qdata (G_OBJECT (clipboard), xfdesktop_clipboard_manager_quark);
  if (G_LIKELY (manager != NULL))
    {
      g_object_ref (G_OBJECT (manager));
      return manager;
    }

  /* allocate a new manager */
  manager = g_object_new (XFDESKTOP_TYPE_CLIPBOARD_MANAGER, NULL);
  manager->clipboard = g_object_ref (G_OBJECT (clipboard));
  g_object_set_qdata (G_OBJECT (clipboard), xfdesktop_clipboard_manager_quark, manager);

  /* listen for the "owner-change" signal on the clipboard */
  g_signal_connect (G_OBJECT (manager->clipboard), "owner-change",
                    G_CALLBACK (xfdesktop_clipboard_manager_owner_changed), manager);

  return manager;
}



/**
 * xfdesktop_clipboard_manager_has_cutted_file:
 * @manager : a #XfdesktopClipboardManager.
 * @file    : a #XfdesktopFile.
 *
 * Checks whether @file was cutted to the given @manager earlier.
 *
 * Return value: %TRUE if @file is on the cutted list of @manager.
 **/
gboolean
xfdesktop_clipboard_manager_has_cutted_file (XfdesktopClipboardManager *manager,
                                             const XfdesktopFileIcon       *file)
{
  g_return_val_if_fail (XFDESKTOP_IS_CLIPBOARD_MANAGER (manager), FALSE);
  g_return_val_if_fail (XFDESKTOP_IS_FILE_ICON (file), FALSE);

  return (manager->files_cutted && g_list_find (manager->files, file) != NULL);
}



/**
 * xfdesktop_clipboard_manager_copy_files:
 * @manager : a #XfdesktopClipboardManager.
 * @files   : a list of #XfdesktopFile<!---->s.
 *
 * Sets the clipboard represented by @manager to
 * contain the @files and marks them to be copied
 * when the user pastes from the clipboard.
 **/
void
xfdesktop_clipboard_manager_copy_files (XfdesktopClipboardManager *manager,
                                        GList                  *files)
{
  g_return_if_fail (XFDESKTOP_IS_CLIPBOARD_MANAGER (manager));
  xfdesktop_clipboard_manager_transfer_files (manager, TRUE, files);
}



/**
 * xfdesktop_clipboard_manager_cut_files:
 * @manager : a #XfdesktopClipboardManager.
 * @files   : a list of #XfdesktopFile<!---->s.
 *
 * Sets the clipboard represented by @manager to
 * contain the @files and marks them to be moved
 * when the user pastes from the clipboard.
 **/
void
xfdesktop_clipboard_manager_cut_files (XfdesktopClipboardManager *manager,
                                       GList                  *files)
{
  g_return_if_fail (XFDESKTOP_IS_CLIPBOARD_MANAGER (manager));
  xfdesktop_clipboard_manager_transfer_files (manager, FALSE, files);
}

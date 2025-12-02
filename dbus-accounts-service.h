#ifndef __DBUS_ACCOUNTS_SERVICE_H__
#define __DBUS_ACCOUNTS_SERVICE_H__

#include <glib.h>

G_BEGIN_DECLS

/**
 * xfdesktop_accounts_service_init:
 *
 * Initializes the D-Bus helper module state. Must be called after GLib/GIO
 * systems are ready. **This function performs no D-Bus communication** and
 * simply prepares the module's static variables and context flags.
 */
void xfdesktop_accounts_service_init(void);

/**
 * xfdesktop_accounts_service_set_background:
 * @path: The path to the background image to set.
 *
 * Asynchronously sends a request to AccountsService to set the BackgroundFile
 * property for the current user.
 *
 * **If the necessary D-Bus proxies do not exist, they are created asynchronously**
 * as the first step of the request chain.
 *
 * This is a "best-effort" request: it will be silent if AccountsService is
 * unavailable or if it does not support the BackgroundFile property.
 * If a previous request is in progress, it will be cancelled and replaced
 * by this new request.
 */
void xfdesktop_accounts_service_set_background(const gchar *path);

/**
 * xfdesktop_accounts_service_shutdown:
 *
 * Cancels all pending requests and immediately frees all D-Bus resources
 * (including proxies and the background path) and module memory. 
 * It is safe to call this function multiple times.
 */
void xfdesktop_accounts_service_shutdown(void);

G_END_DECLS

#endif /* __DBUS_ACCOUNTS_SERVICE_H__ */
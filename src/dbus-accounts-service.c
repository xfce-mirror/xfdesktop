/*
 * dbus-accounts-service.c
 *
 * Asynchronous D-Bus helper module to communicate with AccountsService for
 * setting the login screen background image. It uses a static context
 * and GCancellable for efficient management of a single active asynchronous chain.
 *
 * NOTE: This uses FULLY ASYNCHRONOUS (On-Demand) initialization.
 */

#include "dbus-accounts-service.h"

#include <gio/gio.h>
#include <glib.h>
#include <unistd.h> /* getuid() */
#include <libxfce4util/libxfce4util.h> /* DBG() */

/* D-Bus Constants */
static const gint ACCOUNTS_TIMEOUT = 500; /* Timeout for D-Bus calls in milliseconds */

/* Static Module Context (Singleton) */

typedef struct
{
    GDBusProxy *manager_proxy;    /* Proxy for /org/freedesktop/Accounts (created ASYNCHRONOUSLY on demand) */
    GCancellable *cancellable;    /* Active GCancellable for the current asynchronous chain */
    gchar *background_path;       /* Duplicated path for the current request */
    gboolean in_flight;           /* Indicator: Is an asynchronous chain currently active? */
    gboolean initialized;         /* Indicator: Has the module been initialized (state ready)? */
    gboolean shutting_down;       /* Indicator: Is the module in the process of shutting down? */
} AccountsServiceCtx;

static AccountsServiceCtx ctx = { 0 };

/* Forward Declarations of Callbacks */

static void on_manager_proxy_new_finished (GObject *source_object, GAsyncResult *res, gpointer user_data);
static void on_find_user_finished         (GObject *source_object, GAsyncResult *res, gpointer user_data);
static void on_user_proxy_new_finished    (GObject *source_object, GAsyncResult *res, gpointer user_data);
static void on_introspect_finished        (GObject *source_object, GAsyncResult *res, gpointer user_data);
static void on_set_finished               (GObject *source_object, GAsyncResult *res, gpointer user_data);

/* Helper Functions */

/**
 * xfce_accountsservice_start_find_user:
 *
 * Starts the asynchronous FindUserById call using the existing manager proxy.
 */
static void
xfce_accountsservice_start_find_user(void)
{
    uid_t uid = getuid();

    g_dbus_proxy_call(ctx.manager_proxy,
                      "FindUserById",
                      g_variant_new("(x)", (gint64)uid),
                      G_DBUS_CALL_FLAGS_NONE,
                      ACCOUNTS_TIMEOUT,
                      ctx.cancellable,
                      on_find_user_finished,
                      NULL);
}

/**
 * xfce_accountsservice_handle_error:
 * @error_ptr: Pointer to the GError object.
 * @call_name: Name of the D-Bus call that failed.
 *
 * Checks if an error occurred and whether it was an expected G_IO_ERROR_CANCELLED.
 * Logs a warning for unexpected errors and clears the error.
 *
 * Returns: TRUE if an error was present, FALSE otherwise.
 */
static gboolean
xfce_accountsservice_handle_error(GError **error_ptr, const gchar *call_name)
{
    GError *error = *error_ptr;
    if (error == NULL) {
        return FALSE;
    }

    if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
        g_warning("dbus-accounts-service: %s failed: %s", call_name, error->message);
    }
    g_clear_error(error_ptr);
    return TRUE;
}

/**
 * xfce_accountsservice_supports_backgroundfile:
 * @xml_data: The XML string received from the D-Bus Introspect call.
 *
 * Parses the XML data to check if AccountsService supports the
 * 'BackgroundFile' property on the user object.
 *
 * Returns: TRUE if the property is found.
 */
static gboolean
xfce_accountsservice_supports_backgroundfile(const gchar *xml_data)
{
    GError *error = NULL;
    GDBusNodeInfo *node_info = g_dbus_node_info_new_for_xml(xml_data, &error);
    gboolean found = FALSE;

    if (!node_info) {
        g_clear_error(&error);
        return FALSE;
    }

    if (node_info->interfaces) {
        for (guint i = 0; node_info->interfaces[i] != NULL; i++) {
            GDBusInterfaceInfo *iface = node_info->interfaces[i];

            if (g_strcmp0(iface->name, "org.freedesktop.DisplayManager.AccountsService") != 0) {
                continue;
            }

            if (!iface->properties) {
                continue;
            }

            for (guint j = 0; iface->properties[j] != NULL; j++) {
                if (g_strcmp0(iface->properties[j]->name, "BackgroundFile") == 0) {
                    found = TRUE;
                    break;
                }
            }

            if (found) {
                break;
            }
        }
    }

    g_dbus_node_info_unref(node_info);
    return found;
}

/**
 * xfce_accountsservice_clear_request:
 *
 * Clears all resources associated with the current in-flight request
 * (path, and GCancellable). Cancels any pending operations.
 */
static void
xfce_accountsservice_clear_request(void)
{
    if (ctx.background_path != NULL) {
        g_free(ctx.background_path);
        ctx.background_path = NULL;
    }

    /* Cancel the object before unref to ensure operations are aborted */
    if (ctx.cancellable != NULL) {
        g_cancellable_cancel(ctx.cancellable);
        g_object_unref(ctx.cancellable);
        ctx.cancellable = NULL;
    }

    ctx.in_flight = FALSE;
}

/* Asynchronous Callbacks */

static void
on_set_finished(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    GVariant *ret = g_dbus_proxy_call_finish(G_DBUS_PROXY(source_object), res, &error);

    xfce_accountsservice_handle_error(&error, "Set");

    if (ret != NULL) {
        g_variant_unref(ret);
    }

    /* Clear the request and finalize the chain */
    xfce_accountsservice_clear_request();
}

static void
on_introspect_finished(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    GVariant *variant = g_dbus_proxy_call_finish(G_DBUS_PROXY(source_object), res, &error);
    const gchar *xml = NULL;

    if (xfce_accountsservice_handle_error(&error, "Introspect")) {
        xfce_accountsservice_clear_request();
        return;
    }

    g_variant_get(variant, "(&s)", &xml);

    if (!xfce_accountsservice_supports_backgroundfile(xml)) {
        /* Not supported: silent no-op */
        DBG("dbus-accounts-service: AccountsService object does not support BackgroundFile. Request cancelled.");
        g_variant_unref(variant);
        xfce_accountsservice_clear_request();
        return;
    }

    g_variant_unref(variant);

    /* Property is supported: Start the asynchronous Properties.Set call */
    g_dbus_proxy_call(G_DBUS_PROXY(source_object),
                      "org.freedesktop.DBus.Properties.Set",
                      g_variant_new("(ssv)",
                                    "org.freedesktop.DisplayManager.AccountsService",
                                    "BackgroundFile",
                                    g_variant_new_string(ctx.background_path)),
                      G_DBUS_CALL_FLAGS_NONE, 
                      -1, /* No additional timeout, rely on system default */
                      ctx.cancellable,
                      on_set_finished,
                      NULL);
}

static void
on_user_proxy_new_finished(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    GDBusProxy *user_proxy = g_dbus_proxy_new_for_bus_finish(res, &error);

    if (xfce_accountsservice_handle_error(&error, "creating user proxy")) {
        xfce_accountsservice_clear_request();
        return;
    }

    if (user_proxy == NULL) {
        xfce_accountsservice_clear_request();
        return;
    }

    /* Introspect (asynchronous) on the user_proxy */
    g_dbus_proxy_call(user_proxy,
                      "org.freedesktop.DBus.Introspectable.Introspect",
                      g_variant_new_tuple(NULL, 0),
                      G_DBUS_CALL_FLAGS_NONE,
                      ACCOUNTS_TIMEOUT,
                      ctx.cancellable,
                      on_introspect_finished,
                      NULL);

    g_object_unref(user_proxy);
}

static void
on_find_user_finished(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    GVariant *variant = g_dbus_proxy_call_finish(G_DBUS_PROXY(source_object), res, &error);

    if (xfce_accountsservice_handle_error(&error, "FindUserById")) {
        xfce_accountsservice_clear_request();
        return;
    }

    gchar *object_path = NULL;
    g_variant_get(variant, "(o)", &object_path);
    g_variant_unref(variant);

    if (object_path == NULL) {
        xfce_accountsservice_clear_request();
        return;
    }

    /* Initiate asynchronous proxy creation for the found user object path */
    g_dbus_proxy_new_for_bus(G_BUS_TYPE_SYSTEM,
                             G_DBUS_PROXY_FLAGS_NONE,
                             NULL,
                             "org.freedesktop.Accounts",
                             object_path,
                             "org.freedesktop.Accounts.User",
                             ctx.cancellable,
                             on_user_proxy_new_finished,
                             NULL);

    g_free(object_path);
}

static void
on_manager_proxy_new_finished(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;

    /* If res is not NULL, the proxy was created async; finish the call */
    if (res != NULL) {
        ctx.manager_proxy = g_dbus_proxy_new_for_bus_finish(res, &error);

        if (xfce_accountsservice_handle_error(&error, "creating manager proxy")) {
            xfce_accountsservice_clear_request();
            return;
        }
    }
    
    if (ctx.manager_proxy == NULL) {
        DBG("dbus-accounts-service: Manager proxy is NULL (service unavailable?)");
        xfce_accountsservice_clear_request();
        return;
    }

    /* Manager Proxy is now ready. Start the next asynchronous call: FindUserById */
    xfce_accountsservice_start_find_user();
}

/* Public API Functions */

/**
 * xfdesktop_accounts_service_init:
 *
 * Initializes the D-Bus helper module state. Does NOT perform D-Bus calls.
 */
void
xfdesktop_accounts_service_init(void)
{
    if (ctx.initialized) {
        return;
    }

    /* Initialize static context fields */
    ctx.manager_proxy = NULL;
    ctx.cancellable = NULL;
    ctx.background_path = NULL;
    ctx.in_flight = FALSE;
    ctx.shutting_down = FALSE;
    
    ctx.initialized = TRUE; 
}

/**
 * xfdesktop_accounts_service_set_background:
 * @path: The path to the background image.
 *
 * Starts the asynchronous D-Bus communication chain, including proxy creation
 * if the manager proxy doesn't already exist.
 */
void
xfdesktop_accounts_service_set_background(const gchar *path)
{
    if (!ctx.initialized || ctx.shutting_down) {
        return;
    }

    if (path == NULL || *path == '\0') {
        return;
    }

    /* 1. Cancel any existing request */
    if (ctx.in_flight) {
        xfce_accountsservice_clear_request();
    }

    /* 2. Prepare the new request state */
    ctx.cancellable = g_cancellable_new();
    ctx.background_path = g_strdup(path);
    ctx.in_flight = TRUE;

    if (ctx.manager_proxy != NULL) {
        /* Proxy exists: skip creation and go straight to FindUserById */
        xfce_accountsservice_start_find_user();
        return;
    }

    /* 3. Proxy does NOT exist: Start ASYNCHRONOUS proxy creation */
    g_dbus_proxy_new_for_bus(G_BUS_TYPE_SYSTEM,
                             G_DBUS_PROXY_FLAGS_NONE,
                             NULL,
                             "org.freedesktop.Accounts",
                             "/org/freedesktop/Accounts",
                             "org.freedesktop.Accounts",
                             ctx.cancellable,
                             on_manager_proxy_new_finished,
                             NULL);
}

/**
 * xfdesktop_accounts_service_shutdown:
 *
 * Frees all module resources.
 */
void
xfdesktop_accounts_service_shutdown(void)
{
    if (!ctx.initialized) {
        return;
    }

    ctx.shutting_down = TRUE;

    /* 1. Cancel and clear the active request state */
    xfce_accountsservice_clear_request();

    /* 2. Free the manager proxy (if it was created) */
    if (ctx.manager_proxy != NULL) {
        g_object_unref(ctx.manager_proxy);
        ctx.manager_proxy = NULL;
    }

    /* Reset module state flags */
    ctx.in_flight = FALSE;
    ctx.initialized = FALSE;
    ctx.shutting_down = FALSE;
}
#include "application.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif


#define SHUTDOWN_MENU_RESPONSE_POWEROFF 1
#define SHUTDOWN_MENU_RESPONSE_RESTART  2
#define SHUTDOWN_MENU_RESPONSE_TIMEOUT  3


// --- private declarations ---

typedef enum {
  automatic = 0,
  light     = 1,
  dark      = 2
} ShutdownMenuThemeOptions;

struct _ShutdownMenuApplication
{
  // required stuff
  GApplication  parent_instance;

  // interface stuff
  GSettings     *settings;
  GtkWidget     *window,
                *dialog;
  char          *secondaryTextFormat;

  // cli option values
  gboolean      subcommandIsPowerOff;
  gboolean      dryRun;

  // gsetting option values
  guint32       countdownLength,
                countdownUpdateInterval;
  // NOTE: the following are unused for now
  ShutdownMenuThemeOptions \
                theme;
  gchar         *dialogMonitor;
  gboolean      dimAllMonitors;

  enum ShutdownMenuFinalAction *finalAction;
};
G_DEFINE_TYPE(ShutdownMenuApplication, shutdown_menu_application, GTK_TYPE_APPLICATION);

static void dialog_response(
  GtkDialog *self,
  gint responseId,
  gpointer userData
);

static gboolean dialog_countdown(
  gpointer data
);

static gboolean dialog_timeout(
  gpointer data
);


static void accelerator_escape_action(
  GSimpleAction *,
  GVariant *,
  gpointer userData
);

static void state_flags_changed(
  GtkWidget *window,
  GtkStateFlags,
  gpointer userData
);

// --- implementations ---


ShutdownMenuApplication *shutdown_menu_application_new(enum ShutdownMenuFinalAction *finalAction)
{
  ShutdownMenuApplication *app = g_object_new(
    SHUTDOWN_MENU_TYPE_APPLICATION,
    "application-id", SHUTDOWN_MENU_APPLICATION_ID,
    "flags", G_APPLICATION_FLAGS_NONE,
    NULL
  );
  app->finalAction = finalAction;

  return app;
}

void shutdown_menu_add_cli_arguments(ShutdownMenuApplication *app)
{
  app->subcommandIsPowerOff = true;
  app->dryRun = false;
  GOptionEntry additional_options[] = {
    {
      .long_name    = "restart",
      .short_name   = 'r',
      .flags        = G_OPTION_FLAG_REVERSE,

      .arg          = G_OPTION_ARG_NONE,
      .arg_data     = &(app->subcommandIsPowerOff),

      .description  = "Restart instead of powering off."
    },
    {
      .long_name    = "dry-run",
      .short_name   = 'd',
      .flags        = G_OPTION_FLAG_NONE,

      .arg          = G_OPTION_ARG_NONE,
      .arg_data     = &(app->dryRun),

      .description  = "Run systemctl with --dry-run. This does not actually poweroff/restart your pc."
    },
    NULL
  };
  g_application_add_main_option_entries(
    G_APPLICATION(app), additional_options
  );
}

static void shutdown_menu_application_dispose(GObject *object)
{
  ShutdownMenuApplication *app = SHUTDOWN_MENU_APPLICATION(object);

  //! NOTE: If settings is NULL, then dialogMonitor is a hard-baked string
  if (app->settings != NULL) {
    g_free(app->dialogMonitor);
  }
  g_free(app->settings);
  //! TODO: if dialog: destroy
  //! TODO: if window: destroy

  G_OBJECT_CLASS(shutdown_menu_application_parent_class)->dispose(object);
}

static void shutdown_menu_application_class_init(ShutdownMenuApplicationClass *_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS(_class);
  object_class->dispose = shutdown_menu_application_dispose;

  GApplicationClass *app_class = G_APPLICATION_CLASS(_class);
  app_class->activate = shutdown_menu_application_activate;
}

static void shutdown_menu_application_init(ShutdownMenuApplication *app)
{
  g_assert(GTK_IS_APPLICATION(app));

  GSettingsSchemaSource *schemaSource = g_settings_schema_source_get_default();
  GSettingsSchema *schema = g_settings_schema_source_lookup(schemaSource, SHUTDOWN_MENU_APPLICATION_ID, false);
  if (schema != NULL) {
    app->settings = g_settings_new(SHUTDOWN_MENU_APPLICATION_ID);
    g_settings_sync();

    app->countdownLength = g_settings_get_uint(app->settings, "countdown-length");
    app->countdownUpdateInterval = g_settings_get_uint(app->settings, "countdown-update-interval");
    app->theme = g_settings_get_enum(app->settings, "theme");
    app->dialogMonitor = g_settings_get_string(app->settings, "dialog-monitor");
    app->dimAllMonitors = g_settings_get_boolean(app->settings, "dim-all-monitors");
  } else {
    fprintf(stderr, "Can not load schema for %s, loading default values.\n", SHUTDOWN_MENU_APPLICATION_ID);

    app->settings = NULL;

    app->countdownLength = 60;
    app->countdownUpdateInterval = 10;
    app->theme = automatic;
    app->dialogMonitor = "primary";
    app->dimAllMonitors = true;
  }

  //! TODO: somehow this free raises an exception but the documentation
  //!       says it needs to be freed...
  // g_free(schema);
}

static void shutdown_menu_application_activate(GApplication *app)
{
  g_assert(GTK_IS_APPLICATION(app));

  ShutdownMenuApplication *smApp = SHUTDOWN_MENU_APPLICATION(app);

  smApp->secondaryTextFormat = (
    smApp->subcommandIsPowerOff ?
    "The system will power off automatically in %d seconds." :
    "The system will restart automatically in %d seconds."
  );

  if (!smApp->window)
    smApp->window = gtk_application_window_new(GTK_APPLICATION(smApp));
  gtk_window_set_icon_name(GTK_WINDOW(smApp->window), "shutdown-menu");

  GtkCssProvider *cssProvider = gtk_css_provider_new();
  gtk_css_provider_load_from_resource(
    cssProvider, SHUTDOWN_MENU_APPLICATION_ID_PATH "/stylesheet.css"
  );
  // gtk_css_provider_load_from_path(
  //   cssProvider, SHUTDOWN_MENU_STYLESHEET_DIR "/stylesheet.css"
  // );
  GdkDisplay *display = gtk_widget_get_display(smApp->window);
  gtk_style_context_add_provider_for_display(
    display, GTK_STYLE_PROVIDER(cssProvider),
    GTK_STYLE_PROVIDER_PRIORITY_USER
  );
  smApp->dialog = gtk_message_dialog_new(
    GTK_WINDOW(smApp->window), 0,
    GTK_MESSAGE_INFO, GTK_BUTTONS_NONE,
    (
      smApp->subcommandIsPowerOff ?
      "Power Off" :
      "Restart"
    )
  );
  g_signal_connect(
    smApp->window, "state-flags-changed",
    G_CALLBACK(state_flags_changed),
    smApp->dialog
  );

  GSimpleAction *escapeAcceleratorAction = g_simple_action_new("escAcc", NULL);
  g_signal_connect(
    escapeAcceleratorAction, "activate",
    G_CALLBACK(accelerator_escape_action),
    smApp->dialog
  );
  g_action_map_add_action(
    G_ACTION_MAP(smApp),
    G_ACTION(escapeAcceleratorAction)
  );
  const char *accelerators[] = {"Escape", NULL};
  gtk_application_set_accels_for_action(
    GTK_APPLICATION(smApp),
    "app.escAcc", accelerators
  );

  gtk_widget_set_name(smApp->window, "background-window");
  gtk_widget_set_name(smApp->dialog, "shutdown-dialog");

  g_timeout_add_full(
    G_PRIORITY_DEFAULT,
    smApp->countdownLength * 1000,
    dialog_timeout,
    smApp,
    NULL
  );
  gtk_message_dialog_format_secondary_text(
    GTK_MESSAGE_DIALOG(smApp->dialog),
    smApp->secondaryTextFormat, smApp->countdownLength
  );
  g_timeout_add_full(
    G_PRIORITY_LOW,
    smApp->countdownUpdateInterval * 1000,
    dialog_countdown,
    smApp,
    NULL
  );

  gtk_dialog_add_button(
    GTK_DIALOG(smApp->dialog),
    "Cancel", GTK_RESPONSE_CANCEL
  );
  GtkWidget *restartButton = gtk_dialog_add_button(
    GTK_DIALOG(smApp->dialog),
    "Restart", SHUTDOWN_MENU_RESPONSE_RESTART
  );
  GtkWidget *powerOffButton = gtk_dialog_add_button(
    GTK_DIALOG(smApp->dialog),
    "Power Off", SHUTDOWN_MENU_RESPONSE_POWEROFF
  );
  if (smApp->subcommandIsPowerOff)
    gtk_widget_grab_focus(powerOffButton);
  else
    gtk_widget_grab_focus(restartButton);

  g_signal_connect(
    GTK_DIALOG(smApp->dialog), "response",
    G_CALLBACK(dialog_response),
    smApp
  );


  gtk_widget_show((smApp->dialog));
  gtk_window_maximize(GTK_WINDOW(smApp->window));
  gtk_window_set_decorated(GTK_WINDOW(smApp->window), false);
  gtk_window_present(GTK_WINDOW(smApp->window));

  //! NOTE: https://discourse.gnome.org/t/how-to-hide-app-from-taskbar-in-gtk4/7084/2
#ifdef GDK_WINDOWING_X11
  GdkSurface *windowSurface;
  windowSurface = gtk_native_get_surface(GTK_NATIVE(smApp->window));
  if (windowSurface && GDK_IS_X11_SURFACE(windowSurface))
  {
    gdk_x11_surface_set_skip_taskbar_hint(windowSurface, true);
  }
#endif
  //! NOTE: does not seem to have an effect
  // windowSurface = gtk_native_get_surface(GTK_NATIVE(dialog));
  // if (windowSurface)
  // {
  //   gdk_x11_surface_set_skip_taskbar_hint(windowSurface, true);
  // }

  gtk_window_present(GTK_WINDOW(smApp->window));
}

gboolean shutdown_menu_get_dryrun(ShutdownMenuApplication *app)
{
  return app->dryRun;
}

static void dialog_response(GtkDialog *self, gint response_id, gpointer user_data)
{
  ShutdownMenuApplication *app = user_data;

  switch (response_id)
  {
  case SHUTDOWN_MENU_RESPONSE_RESTART:
    *(app->finalAction) = SHUTDOWN_MENU_FINAL_ACTION_RESTART;
    break;
  case SHUTDOWN_MENU_RESPONSE_POWEROFF:
    *(app->finalAction) = SHUTDOWN_MENU_FINAL_ACTION_POWER_OFF;
    break;
  case SHUTDOWN_MENU_RESPONSE_TIMEOUT:
  *(app->finalAction) = (
      app->subcommandIsPowerOff ?
      SHUTDOWN_MENU_FINAL_ACTION_POWER_OFF :
      SHUTDOWN_MENU_FINAL_ACTION_RESTART
      );
    break;
  default:
    *(app->finalAction) = SHUTDOWN_MENU_FINAL_ACTION_NOTHING;
  }

  gtk_window_destroy(GTK_WINDOW(app->window));
}

static void accelerator_escape_action(GSimpleAction *, GVariant *, gpointer userData)
{
  GtkDialog *dialog = userData;
  gtk_dialog_response(dialog, GTK_RESPONSE_CANCEL);
}

static void state_flags_changed(GtkWidget *window, GtkStateFlags, gpointer userData)
{
  GtkStateFlags flags = gtk_widget_get_state_flags(window);
  GtkWidget *dialog = userData;

  if (flags & GTK_STATE_FLAG_ACTIVE)
    gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
}


static gboolean dialog_countdown(gpointer data)
{
  ShutdownMenuApplication *app = data;

  app->countdownLength -= app->countdownUpdateInterval;
  if (app->countdownLength <= 0u)
    return false;

  gtk_message_dialog_format_secondary_text(
    GTK_MESSAGE_DIALOG(app->dialog),
    app->secondaryTextFormat, app->countdownLength
  );

  return true;
}

static gboolean dialog_timeout(gpointer data)
{
  ShutdownMenuApplication *app = data;

  gtk_dialog_response(
    GTK_DIALOG(app->dialog),
    SHUTDOWN_MENU_RESPONSE_TIMEOUT
  );

  return false;
}

#include "application.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include <linux/version.h>
#include <gdk/x11/gdkx.h>


#define RESPONSE_POWEROFF 1
#define RESPONSE_RESTART  2
#define RESPONSE_TIMEOUT  3

#define SYSTEMCTL_SUBCOMMAND_REBOOT   "reboot"
#define SYSTEMCTL_SUBCOMMAND_POWEROFF "poweroff"


#if LINUX_VERSION_MAJOR <= 3 && LINUX_VERSION_PATCHLEVEL < 1
#define HANDLE_SETPID(command)                \
  if ((command) == -1)                        \
    print_error("setpid");
#else
#define HANDLE_SETPID(command)                \
  int result;                                 \
  do                                          \
  {                                           \
    result = (command);                       \
  } while (result == -1 && errno == EAGAIN);  \
  if (result == -1)                           \
    print_error("setpid");
#endif

static void print_error(const char *fkt)
{
  fprintf(
    stderr, "Error %s in %s: %s\n",
    strerrorname_np(errno), fkt, strerrordesc_np(errno)
  );
}


struct _ShutdownMenuApplication
{
  GApplication parent_instance;

  GSettings *settings;
  GtkWidget *window,
            *dialog;

  gboolean      subcommandIsPowerOff;
  char         *secondaryTextFormat;
  unsigned int  countdownTimerTime,
                countdownTimerInterval;
  int64_t       gracePeriod;
};
G_DEFINE_TYPE(ShutdownMenuApplication, shutdown_menu_application, GTK_TYPE_APPLICATION);


ShutdownMenuApplication *shutdown_menu_application_new()
{
  return g_object_new(
    SHUTDOWN_MENU_TYPE_APPLICATION,
    "application-id", APPLICATION_ID,
    "flags", G_APPLICATION_FLAGS_NONE,
    NULL
  );
}

void shutdown_menu_add_cli_arguments(ShutdownMenuApplication *app)
{
  app->subcommandIsPowerOff = true;
  app->gracePeriod = 0l;
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
      .long_name    = "grace-period",
      .short_name   = 'g',
      .flags        = G_OPTION_FLAG_NONE,

      .arg          = G_OPTION_ARG_INT64,
      .arg_data     = &(app->gracePeriod),

      .description  = "The ammount of seconds the applications waits before enforcing the selected option."
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

  g_clear_object(&app->settings);
  //! TODO: if dialog: destroy
  //! TODO: if window: destroy

  G_OBJECT_CLASS(shutdown_menu_application_parent_class)->dispose(object);
}

static void shutdown_menu_application_activate(GApplication *app)
{
  g_assert(GTK_IS_APPLICATION(app));

  ShutdownMenuApplication *smApp = SHUTDOWN_MENU_APPLICATION(app);
  if (!smApp->window)
    smApp->window = gtk_application_window_new(GTK_APPLICATION(app));
  gtk_window_set_icon_name(GTK_WINDOW(smApp->window), "shutdown-menu");

  GtkCssProvider *cssProvider = gtk_css_provider_new();
  gtk_css_provider_load_from_path(
    cssProvider, STYLESHEET_DIR "stylesheet.css"
  );
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
  // g_signal_connect(
  //   smApp->dialog, "state-flags-changed",
  //   G_CALLBACK(state_flags_changed),
  //   NULL
  // );

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
    smApp->countdownTimerTime * 1000,
    dialog_timeout,
    smApp,
    NULL
  );
  gtk_message_dialog_format_secondary_text(
    GTK_MESSAGE_DIALOG(smApp->dialog),
    smApp->secondaryTextFormat, smApp->countdownTimerTime
  );
  g_timeout_add_full(
    G_PRIORITY_LOW,
    smApp->countdownTimerInterval * 1000,
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
    "Restart", RESPONSE_RESTART
  );
  GtkWidget *powerOffButton = gtk_dialog_add_button(
    GTK_DIALOG(smApp->dialog),
    "Power Off", RESPONSE_POWEROFF
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
  GdkSurface *windowSurface;
  windowSurface = gtk_native_get_surface(GTK_NATIVE(smApp->window));
  if (windowSurface)
  {
    gdk_x11_surface_set_skip_taskbar_hint(windowSurface, true);
  }
  //! NOTE: does not seem to have an effect
  // windowSurface = gtk_native_get_surface(GTK_NATIVE(dialog));
  // if (windowSurface)
  // {
  //   gdk_x11_surface_set_skip_taskbar_hint(windowSurface, true);
  // }

  gtk_window_present(GTK_WINDOW(smApp->window));
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

  // app->settings = g_settings_new(APPLICATION_ID);

  app->secondaryTextFormat = (
    app->subcommandIsPowerOff ?
    "The system will power off automatically in %d seconds." :
    "The system will restart automatically in %d seconds."
  );
  //! TODO: from settings
  app->countdownTimerTime = 60u;
  app->countdownTimerInterval = 10u;
}

static void dialog_response(GtkDialog *self, gint response_id, gpointer user_data)
{
  ShutdownMenuApplication *app = user_data;

  const char *subcommand = NULL;
  switch (response_id)
  {
  case RESPONSE_RESTART:
    subcommand = SYSTEMCTL_SUBCOMMAND_REBOOT;
    break;
  case RESPONSE_POWEROFF:
    subcommand = SYSTEMCTL_SUBCOMMAND_POWEROFF;
    break;
  case RESPONSE_TIMEOUT:
    subcommand = (
      app->subcommandIsPowerOff ?
      SYSTEMCTL_SUBCOMMAND_POWEROFF :
      SYSTEMCTL_SUBCOMMAND_REBOOT
      );
    break;
  default:
    // nothing
  }

  gtk_window_destroy(GTK_WINDOW(app->window));

  if (subcommand)
    run(subcommand);
}

static void accelerator_escape_action(GSimpleAction *, GVariant *, gpointer userData)
{
  GtkDialog *dialog = userData;
  gtk_dialog_response(dialog, GTK_RESPONSE_CANCEL);
}

static void state_flags_changed(GtkWidget *dialog, GtkStateFlags previousFlags, gpointer)
{
  GtkStateFlags flags = gtk_widget_get_state_flags(dialog);
  if (!(flags & GTK_STATE_FLAG_FOCUS_WITHIN) &&
      previousFlags & GTK_STATE_FLAG_FOCUS_WITHIN)
    gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
}


static void run(const char *subcommand)
{
  HANDLE_SETPID(setuid(0));
  if (setgid(0) == -1)
    print_error("setgid");

  const char *const argv[] = {"/usr/bin/systemctl", DRY_RUN subcommand, NULL};
  const char *const environ[] = {NULL};

  execve(argv[0], (char *const *)argv, (char *const *)environ);
  perror("execve");
}

static gboolean dialog_countdown(gpointer data)
{
  ShutdownMenuApplication *app = data;

  app->countdownTimerTime -= 10u;
  if (app->countdownTimerTime <= 0u)
    return false;

  gtk_message_dialog_format_secondary_text(
    GTK_MESSAGE_DIALOG(app->dialog),
    app->secondaryTextFormat, app->countdownTimerTime
  );

  return true;
}

static gboolean dialog_timeout(gpointer data)
{
  ShutdownMenuApplication *app = data;

  gtk_dialog_response(
    GTK_DIALOG(app->dialog),
    RESPONSE_TIMEOUT
  );

  return false;
}

#include "dialog.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include <linux/version.h>
#include <gdk/x11/gdkx.h>


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


void print_error(const char *fkt)
{
  fprintf(
    stderr, "Error %s in %s: %s\n",
    strerrorname_np(errno), fkt, strerrordesc_np(errno)
  );
}

void on_activate(GtkApplication *app, gpointer user_data) {
  gboolean isPowerOff = *(gboolean*)user_data;

  GtkWidget *window = gtk_application_window_new(app);
  gtk_window_set_icon_name(GTK_WINDOW(window), "shutdown-menu");

  GtkCssProvider *cssProvider = gtk_css_provider_new();
  gtk_css_provider_load_from_path(
    cssProvider, STYLESHEET_DIR "stylesheet.css"
  );
  GdkDisplay *display = gtk_widget_get_display(GTK_WIDGET(window));
  gtk_style_context_add_provider_for_display(
    display, GTK_STYLE_PROVIDER(cssProvider),
    GTK_STYLE_PROVIDER_PRIORITY_USER
  );

  GtkWidget *dialog = gtk_message_dialog_new(
    GTK_WINDOW(window), 0,
    GTK_MESSAGE_INFO, GTK_BUTTONS_NONE,
    "Power Off."
  );

  gtk_widget_set_name(window, "background-window");
  gtk_widget_set_name(dialog, "shutdown-dialog");

  dialog_countdown_data_t *countdown_data = create_dialog_countdown_data(dialog, isPowerOff);
  gtk_message_dialog_format_secondary_text(
    GTK_MESSAGE_DIALOG(dialog),
    countdown_data->secondary_text_format, 60
  );
  g_timeout_add_full(
    G_PRIORITY_LOW,
    10000, // 10s
    dialog_countdown,
    countdown_data,
    dialog_countdown_destroy
  );

  gtk_dialog_add_button(
    GTK_DIALOG(dialog),
    "Cancel", GTK_RESPONSE_CANCEL
  );
  GtkWidget *restartButton = gtk_dialog_add_button(
    GTK_DIALOG(dialog),
    "Restart", RESPONSE_RESTART
  );
  GtkWidget *powerOffButton = gtk_dialog_add_button(
    GTK_DIALOG(dialog),
    "Power Off", RESPONSE_POWEROFF
  );
  if (isPowerOff)
    gtk_widget_grab_focus(powerOffButton);
  else
    gtk_widget_grab_focus(restartButton);

  dialog_response_data_t *response_data = create_dialog_response_data(
    window, isPowerOff
  );
  g_signal_connect(
    dialog, "response",
    G_CALLBACK(dialog_response),
    response_data
  );


  gtk_widget_show(dialog);
  gtk_window_maximize(GTK_WINDOW(window));
  gtk_window_set_decorated(GTK_WINDOW(window), false);
  gtk_window_present(GTK_WINDOW(window));

  //! NOTE: https://discourse.gnome.org/t/how-to-hide-app-from-taskbar-in-gtk4/7084/2
  GdkSurface *windowSurface;
  windowSurface = gtk_native_get_surface(GTK_NATIVE(window));
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
}


dialog_response_data_t *create_dialog_response_data(GtkWidget *window, gboolean subcommandIsPowerOff)
{
  dialog_response_data_t *data = malloc(sizeof(dialog_response_data_t));
  assert(data);

  data->window = window;
  data->subcommandIsPowerOff = subcommandIsPowerOff;

  return data;
}

void dialog_response(GtkDialog* self, gint response_id, gpointer user_data)
{
  dialog_response_data_t *data = user_data;

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
        data->subcommandIsPowerOff ?
        SYSTEMCTL_SUBCOMMAND_POWEROFF :
        SYSTEMCTL_SUBCOMMAND_REBOOT
      );
      break;
    default:
      // nothing
  }

  gtk_window_destroy(GTK_WINDOW(data->window));

  if (subcommand)
    run(subcommand);
  free(data);
}


dialog_countdown_data_t *create_dialog_countdown_data(GtkWidget *dialog, gboolean isPowerOff)
{
  dialog_countdown_data_t *data = malloc(sizeof(dialog_countdown_data_t));
  assert(data);

  data->dialog  = dialog;
  data->time    = 50u;
  data->secondary_text_format = (
    isPowerOff ?
    "The system will power off automatically in %d seconds." :
    "The system will restart automatically in %d seconds."
  );

  return data;
}

void run(const char *subcommand)
{
  HANDLE_SETPID(setuid(0));
  if (setgid(0) == -1)
    print_error("setgid");

  const char *const argv[] = {"/usr/bin/systemctl", DRY_RUN subcommand, NULL};
  const char *const environ[] = { NULL };

  execve(argv[0], (char *const *)argv, (char *const *)environ);
  perror("execve");
}

gboolean dialog_countdown(gpointer data)
{
  dialog_countdown_data_t *countdown_data = data;

  if (countdown_data->time == 0u)
    return false;

  gtk_message_dialog_format_secondary_text(
    GTK_MESSAGE_DIALOG(countdown_data->dialog),
    countdown_data->secondary_text_format, countdown_data->time
  );
  countdown_data->time -= 10u;

  return true;
}

void dialog_countdown_destroy(gpointer data)
{
  dialog_countdown_data_t *countdown_data = data;

  gtk_dialog_response(
    GTK_DIALOG(countdown_data->dialog),
    RESPONSE_TIMEOUT
  );

  free(countdown_data);
}

#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <gdk/x11/gdkx.h>
#include <gdk/gdk.h>
// #include <adwaita.h>

#define RESPONSE_POWEROFF 1
#define RESPONSE_RESTART  2

/**
 * TODO:
 *  - make window transparent and black
 *  - fix dialog positioning (remove deco after dialog pos is determined)
 *  - systemctl seems to be missing a polkit session or something?
 *    -> https://www.reddit.com/r/systemd/comments/1esp7jq/i_hate_systemd_error_when_systemctl_suspend/
 *  - Pretty up main and secondary texts
*/


void on_activate(GtkApplication *app);
void dialog_response(GtkDialog* self, gint response_id, gpointer user_data);

int main(int argc, char **argv) {
  GtkApplication *app = gtk_application_new("org.example.shutdown-dialog", 0);
  g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
  return g_application_run(G_APPLICATION (app), argc, argv);
}

void on_activate(GtkApplication *app) {
  GtkWidget *window = gtk_application_window_new(app);
  gtk_window_maximize(GTK_WINDOW(window));
  gtk_window_set_decorated(GTK_WINDOW(window), false);
  gtk_window_present(GTK_WINDOW(window));
  // make transparent using: https://stackoverflow.com/a/3909283/7976097

  GtkStyleContext *styleContext = gtk_widget_get_style_context(window);
  GdkRGBA color;
  gtk_style_context_get_color(styleContext, &color);
  printf("Window color: %s\n", gdk_rgba_to_string(&color)); // OOPSIE DAISY, this is a memory leak.

  GdkSurface *windowSurface = gtk_native_get_surface(GTK_NATIVE(window));
  //! NOTE: https://discourse.gnome.org/t/how-to-hide-app-from-taskbar-in-gtk4/7084/2
  if (windowSurface)
  {
    gdk_x11_surface_set_skip_taskbar_hint(windowSurface, true);
  }

  GtkWidget *dialog = gtk_message_dialog_new(
    GTK_WINDOW(window), 0,
    GTK_MESSAGE_INFO, GTK_BUTTONS_NONE,
    "Power Off."
  );

  gtk_message_dialog_format_secondary_text(
    GTK_MESSAGE_DIALOG(dialog),
    "Restart or Power Off"
  );
  gtk_dialog_add_button(
    GTK_DIALOG(dialog),
    "Cancel", GTK_RESPONSE_CANCEL
  );
  gtk_dialog_add_button(
    GTK_DIALOG(dialog),
    "Restart", RESPONSE_RESTART
  );
  gtk_dialog_add_button(
    GTK_DIALOG(dialog),
    "Power Off", RESPONSE_POWEROFF
  );


  g_signal_connect(
    dialog, "response",
    G_CALLBACK(dialog_response),
    window
  );


  gtk_widget_show(dialog);
}

void dialog_response(GtkDialog* self, gint response_id, gpointer user_data)
{
  switch (response_id)
  {
    case RESPONSE_RESTART:
      system("systemctl --dry-run reboot");
      break;
    case RESPONSE_POWEROFF:
      system("systemctl --dry-run poweroff");
      break;
    default:
      // nothing
  }

  gtk_window_destroy(GTK_WINDOW(user_data));
}

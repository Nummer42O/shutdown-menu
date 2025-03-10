#ifndef DIALOG_H
#define DIALOG_H

#include <gtk/gtk.h>
#include <gdk/gdk.h>


#define RESPONSE_POWEROFF 1
#define RESPONSE_RESTART  2
#define RESPONSE_TIMEOUT  3


#define SYSTEMCTL_SUBCOMMAND_REBOOT   "reboot"
#define SYSTEMCTL_SUBCOMMAND_POWEROFF "poweroff"


void on_activate(
  GtkApplication *app,
  gpointer user_data
);


typedef struct {
  GtkWidget *window;
  gboolean subcommandIsPowerOff;
} dialog_response_data_t;

dialog_response_data_t *create_dialog_response_data(
  GtkWidget *window,
  gboolean subcommandIsPowerOff
);

void dialog_response(
  GtkDialog *self,
  gint response_id,
  gpointer user_data
);


void accelerator_escape_action(
  GSimpleAction *,
  GVariant *,
  gpointer userData
);

void state_flags_changed(
  GtkWidget *dialog,
  GtkStateFlags previousFlags,
  gpointer
);

typedef struct {
  GtkWidget *dialog;
  unsigned int time;
  const char *secondary_text_format;
} dialog_countdown_data_t;

dialog_countdown_data_t *create_dialog_countdown_data(
  GtkWidget *dialog,
  gboolean isPowerOff
);

void run(const char *subcommand);

gboolean dialog_countdown(
  gpointer data
);

void dialog_countdown_destroy(
  gpointer data
);


#endif //!defined(DIALOG_H)

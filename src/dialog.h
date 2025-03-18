#ifndef DIALOG_H
#define DIALOG_H

#include "application.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>


#define RESPONSE_POWEROFF 1
#define RESPONSE_RESTART  2
#define RESPONSE_TIMEOUT  3


#define SYSTEMCTL_SUBCOMMAND_REBOOT   "reboot"
#define SYSTEMCTL_SUBCOMMAND_POWEROFF "poweroff"

G_BEGIN_DECLS

GtkMessageDialog *shutdown_menu_messagedialog_new(
  ShutdownMenuApplication *app,
  gboolean defaultIsPowerOff
);

typedef struct {
  GtkWidget *window;
  gboolean subcommandIsPowerOff;
} dialog_response_data_t;

static dialog_response_data_t *create_dialog_response_data(
  GtkWidget *window,
  gboolean subcommandIsPowerOff
);

static void dialog_response(
  GtkDialog *self,
  gint response_id,
  gpointer user_data
);


static void accelerator_escape_action(
  GSimpleAction *,
  GVariant *,
  gpointer userData
);

static void state_flags_changed(
  GtkWidget *dialog,
  GtkStateFlags previousFlags,
  gpointer
);

typedef struct {
  GtkWidget *dialog;
  unsigned int time;
  const char *secondary_text_format;
} dialog_countdown_data_t;

static dialog_countdown_data_t *create_dialog_countdown_data(
  GtkWidget *dialog,
  gboolean isPowerOff
);

static void run(
  const char *subcommand
);

static gboolean dialog_countdown(
  gpointer data
);

static void dialog_countdown_destroy(
  gpointer data
);

G_END_DECLS
#endif //!defined(DIALOG_H)

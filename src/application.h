#ifndef APPLICATION_H
#define APPLICATION_H

#include <gtk/gtk.h>

#ifndef APPLICATION_ID
#error "Missing application id (APPLICATION_ID)."
#endif

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(ShutdownMenuApplication, shutdown_menu_application, SHUTDOWN_MENU, APPLICATION, GtkApplication)

#define SHUTDOWN_MENU_TYPE_APPLICATION (shutdown_menu_application_get_type())
#define SHUTDOWN_MENU_APPLICATION(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), SHUTDOWN_MENU_TYPE_APPLICATION, ShutdownMenuApplication))

ShutdownMenuApplication *shutdown_menu_application_new();
void shutdown_menu_add_cli_arguments(ShutdownMenuApplication *app);

G_END_DECLS


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

#endif //!defined(APPLICATION_H)
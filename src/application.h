#ifndef APPLICATION_H
#define APPLICATION_H

#include <gtk/gtk.h>

#ifndef APPLICATION_ID
#error "Missing application id (APPLICATION_ID)."
#endif

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(
  ShutdownMenuApplication,
  shutdown_menu_application,
  SHUTDOWN_MENU,
  APPLICATION,
  GtkApplication
)

#define SHUTDOWN_MENU_TYPE_APPLICATION  \
  (shutdown_menu_application_get_type())
#define SHUTDOWN_MENU_APPLICATION(obj)  \
  (G_TYPE_CHECK_INSTANCE_CAST(          \
    (obj),                              \
    SHUTDOWN_MENU_TYPE_APPLICATION,     \
    ShutdownMenuApplication             \
  ))

enum ShutdownMenuFinalAction
{
  SHUTDOWN_MENU_FINAL_ACTION_POWER_OFF,
  SHUTDOWN_MENU_FINAL_ACTION_RESTART,
  SHUTDOWN_MENU_FINAL_ACTION_NOTHING
};
ShutdownMenuApplication *shutdown_menu_application_new(
  enum ShutdownMenuFinalAction *finalAction
);
void shutdown_menu_add_cli_arguments(
  ShutdownMenuApplication *app
);

G_END_DECLS

static void dialog_response(
  GtkDialog *self,
  gint responseId,
  gpointer userData
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


static gboolean dialog_countdown(
  gpointer data
);

static gboolean dialog_timeout(
  gpointer data
);

#endif //!defined(APPLICATION_H)
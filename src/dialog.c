#include "dialog.h"

GtkMessageDialog *shutdown_menu_messagedialog_new(ShutdownMenuApplication *app, gboolean defaultIsPowerOff)
{
  GtkMessageDialog *dialog = gtk_message_dialog_new(
    GTK_WINDOW(app->window), 0,
    GTK_MESSAGE_INFO, GTK_BUTTONS_NONE,
    (
      defaultIsPowerOff ?
      "Power Off" :
      "Restart"
    )
  );


}


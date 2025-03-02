#include "dialog.h"


int main(int argc, char *argv[]) {
  GtkApplication *app = gtk_application_new(
    "github.Nummer42O.shutdown-dialog",
    G_APPLICATION_FLAGS_NONE
  );

  gboolean isPowerOff = true;
  GOptionEntry additional_options[] = {
    {
      .long_name    = "restart",
      .short_name   = 'r',
      .flags        = G_OPTION_FLAG_REVERSE,

      .arg          = G_OPTION_ARG_NONE,
      .arg_data     = &isPowerOff,

      .description  = "Restart instead of powering off."
    },
    NULL
  };
  g_application_add_main_option_entries(
    G_APPLICATION(app), additional_options
  );

  g_signal_connect(
    app, "activate",
    G_CALLBACK(on_activate),
    &isPowerOff
  );
  int status = g_application_run(G_APPLICATION(app), argc, argv);

  g_object_unref(app);
  return status;
}

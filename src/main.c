#include "dialog.h"


/**
 * TODO:
 *  - fix dialog positioning (remove deco after dialog pos is determined)
 *  - systemctl seems to be missing a polkit session or something?
 *    -> https://www.reddit.com/r/systemd/comments/1esp7jq/i_hate_systemd_error_when_systemctl_suspend/
 *  - Pretty up main and secondary texts
 *  - Add option for default
*/


int main(int argc, char **argv) {
  GtkApplication *app = gtk_application_new("org.example.shutdown-dialog", 0);
  g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
  return g_application_run(G_APPLICATION (app), argc, argv);
}

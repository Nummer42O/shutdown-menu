#include "application.h"
// #include "dialog.h"


int main(int argc, char *argv[])
{
  ShutdownMenuApplication *app = shutdown_menu_application_new();
  shutdown_menu_add_cli_arguments(app);
  return g_application_run(G_APPLICATION(app), argc, argv);
}

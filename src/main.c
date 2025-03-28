#include "application.h"
// #include "dialog.h"

#include <linux/version.h>

#define SYSTEMCTL_SUBCOMMAND_REBOOT   "reboot"
#define SYSTEMCTL_SUBCOMMAND_POWEROFF "poweroff"


static void print_error(const char *functionName)
{
  fprintf(
    stderr, "Error %s in %s: %s\n",
    strerrorname_np(errno), functionName, strerrordesc_np(errno)
  );
}

int main(int argc, char *argv[])
{
  enum ShutdownMenuFinalAction finalAction = SHUTDOWN_MENU_FINAL_ACTION_NOTHING;
  ShutdownMenuApplication *app = shutdown_menu_application_new(&finalAction);
  shutdown_menu_add_cli_arguments(app);

  int result = g_application_run(G_APPLICATION(app), argc, argv);
  if (result != 0)
    return result;

  if (finalAction == SHUTDOWN_MENU_FINAL_ACTION_NOTHING)
    return 0;

#if LINUX_VERSION_MAJOR <= 3 && LINUX_VERSION_PATCHLEVEL < 1
  if (setuid(0) == -1)
    print_error("setuid");
#else
  int setUidResult;
  do
  {
    setUidResult = setuid(0);
  } while (setUidResult == -1 && errno == EAGAIN);
  if (setUidResult == -1)
    print_error("setuid");
#endif
  if (setgid(0) == -1)
    print_error("setgid");

  const char *const subcommandArgv[] = {
    "/usr/bin/systemctl",
    DRY_RUN
    (
      finalAction == SHUTDOWN_MENU_FINAL_ACTION_POWER_OFF ?
      SYSTEMCTL_SUBCOMMAND_POWEROFF :
      SYSTEMCTL_SUBCOMMAND_REBOOT
    ),
    NULL
};
  const char *const environ[] = {NULL};

  execve(subcommandArgv[0], (char *const *)subcommandArgv, (char *const *)environ);
  perror("execve");
}

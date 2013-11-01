#include <unistd.h>

#include "src/utils/fdmi_err.h"
#include "src/utils/fdmi_param.h"
#include "src/config/fdmi_config.h"

void reexec()
{
  char **argv;
  int failed_rank;
  char new_fdmi_id_str[64];
  char new_fdmi_restart_str[64];

  argv = *fdmi_argv;
  sprintf(new_fdmi_id_str, "%d", fdmi_id + 1);
  fdmi_param_set("MPIRUN_ID", new_fdmi_id_str);
  sprintf(new_fdmi_restart_str, "%d", fdmi_restart + 1);
  fdmi_param_set("MPIRUN_RESTART", new_fdmi_restart_str);

  /* if (prank == 0) { */
  /*   int i; */
  /*   for(i=0 ; environ[i] != NULL; i++) { */
  /*     fdmi_dbg("%s", environ[i]); */
  /*   } */
  /* } */
  fdmi_dbgi(0, "exec");
  execvpe((*fdmi_argv)[0], *fdmi_argv, environ);
  fdmi_err("execvp failed (%s:%s:%d)", __FILE__, __func__, __LINE__);
}

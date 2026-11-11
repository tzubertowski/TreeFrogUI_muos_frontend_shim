#ifndef MUOS_HOST_SUPERVISOR_H
#define MUOS_HOST_SUPERVISOR_H

#include "libretro.h"

#ifndef MUOS_HOST_LAUNCH_FILE
#define MUOS_HOST_LAUNCH_FILE "/mnt/sdcard/cubegm/frogui_launch.txt"
#endif
#ifndef MUOS_HOST_SHUTDOWN_SCRIPT
#define MUOS_HOST_SHUTDOWN_SCRIPT "/mnt/sdcard/cubegm/shutdown.sh"
#endif
#ifndef MUOS_HOST_REBOOT_SCRIPT
#define MUOS_HOST_REBOOT_SCRIPT "/mnt/sdcard/cubegm/reboot.sh"
#endif
#ifndef MUOS_HOST_CORES_PATH
#define MUOS_HOST_CORES_PATH "/mnt/sdcard/cubegm/cores"
#endif

void muos_host_supervisor_step(retro_environment_t environment);
int muos_host_launch_game(retro_environment_t environment);

#endif

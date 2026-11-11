#ifndef MUOS_HOST_DIAG_H
#define MUOS_HOST_DIAG_H

void muos_diag_start(void);
void muos_diag_log(const char *event, const char *format, ...);

#endif

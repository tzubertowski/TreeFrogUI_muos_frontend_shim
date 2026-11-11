#include "muos_host_diag.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define DIAG_DEFAULT_PATH "/mnt/sdcard/cubegm/logs/muos_frontend_mvp.log"

static pthread_mutex_t diag_lock = PTHREAD_MUTEX_INITIALIZER;
static char diag_path[512] = DIAG_DEFAULT_PATH;

void muos_diag_log(const char *event, const char *format, ...) {
    pthread_mutex_lock(&diag_lock);
    FILE *file = fopen(diag_path, "a");
    if (file) {
        time_t now = time(NULL);
        struct tm tm_now;
        char timestamp[32] = "unknown-time";
        if (localtime_r(&now, &tm_now)) strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_now);
        fprintf(file, "%s %s", timestamp, event ? event : "event");
        if (format && *format) {
            fputc(' ', file);
            va_list args;
            va_start(args, format);
            vfprintf(file, format, args);
            va_end(args);
        }
        fputc('\n', file);
        fclose(file);
    }
    pthread_mutex_unlock(&diag_lock);
}

void muos_diag_start(void) {
    const char *override = getenv("MUOS_HOST_LOG_FILE");
    if (override && *override && strlen(override) < sizeof(diag_path))
        snprintf(diag_path, sizeof(diag_path), "%s", override);
    char previous[sizeof(diag_path) + 5];
    if (snprintf(previous, sizeof(previous), "%s.prev", diag_path) > 0) rename(diag_path, previous);
    FILE *file = fopen(diag_path, "w");
    if (file) fclose(file);
    muos_diag_log("start", "pid=%ld log=%s", (long)getpid(), diag_path);
}

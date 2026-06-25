#include "oware_ui.h"
#include "oware_store.h"
#include <stdio.h>
#include <string.h>

static bool stdio_read(oware_io_t *io, char *buf, size_t cap) {
    (void)io;
    if (fgets(buf, (int)cap, stdin) == NULL) {
        return false;
    }
    size_t len = strlen(buf);
    while ((len > 0u) && ((buf[len - 1u] == '\n') || (buf[len - 1u] == '\r'))) {
        buf[len - 1u] = '\0';
        len--;
    }
    return true;
}

static void stdio_write(oware_io_t *io, const char *s) {
    (void)io;
    (void)fputs(s, stdout);
    (void)fflush(stdout);
}

int main(void) {
    char path[512];
    (void)oware_store_default_path(path, sizeof(path));

    oware_store_t store;
    (void)oware_store_load(&store, path); /* false on first run is fine */

    oware_io_t io;
    io.read_line = stdio_read;
    io.write_str = stdio_write;
    io.ctx = NULL;

    oware_ui_run(&io, &store, path);
    (void)oware_store_save(&store, path);
    (void)fputs("Bye.\n", stdout);
    return 0;
}

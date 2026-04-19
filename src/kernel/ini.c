#include <kernel/ini.h>

// Trim leading whitespace, return pointer to first non-space char
static const char *skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

// Copy up to max-1 chars into dst, null-terminate, strip trailing whitespace
static void copy_trimmed(char *dst, const char *src, uint32 len, uint32 max) {
    if (len >= max) len = max - 1;
    for (uint32 i = 0; i < len; i++) dst[i] = src[i];
    dst[len] = '\0';
    // strip trailing whitespace
    while (len > 0 && (dst[len-1] == ' ' || dst[len-1] == '\t' || dst[len-1] == '\r')) {
        dst[--len] = '\0';
    }
}

int ini_parse(const char *buf, uint32 len, ini_handler handler, void *user) {
    char section[32] = {0};
    char key[32];
    char value[128];

    const char *p = buf;
    const char *end = buf + len;

    while (p < end) {
        // find end of line
        const char *eol = p;
        while (eol < end && *eol != '\n') eol++;

        const char *line = skip_ws(p);
        uint32 line_len = (uint32)(eol - line);

        // skip empty lines and comments
        if (line_len == 0 || *line == '#' || *line == ';' || *line == '\r') {
            p = (eol < end) ? eol + 1 : eol;
            continue;
        }

        if (*line == '[') {
            // section header
            const char *close = line + 1;
            while (close < eol && *close != ']') close++;
            if (*close == ']') {
                copy_trimmed(section, line + 1, (uint32)(close - line - 1), sizeof(section));
            }
        } else {
            // key=value
            const char *eq = line;
            while (eq < eol && *eq != '=') eq++;
            if (eq < eol) {
                copy_trimmed(key, line, (uint32)(eq - line), sizeof(key));
                const char *val_start = skip_ws(eq + 1);
                uint32 val_len = (uint32)(eol - val_start);
                copy_trimmed(value, val_start, val_len, sizeof(value));
                handler(section, key, value, user);
            }
        }

        p = (eol < end) ? eol + 1 : eol;
    }

    return 0;
}

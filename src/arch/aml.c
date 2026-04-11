#include <arch/aml.h>
#include <drivers/monitor.h>

// AML opcodes (subset)
#define AML_ZERO_OP        0x00
#define AML_ONE_OP         0x01
#define AML_ALIAS_OP       0x06
#define AML_NAME_OP        0x08
#define AML_BYTE_PREFIX    0x0A
#define AML_WORD_PREFIX    0x0B
#define AML_DWORD_PREFIX   0x0C
#define AML_STRING_PREFIX  0x0D
#define AML_QWORD_PREFIX   0x0E
#define AML_SCOPE_OP       0x10
#define AML_BUFFER_OP      0x11
#define AML_PACKAGE_OP     0x12
#define AML_VAR_PACKAGE_OP 0x13
#define AML_METHOD_OP      0x14
#define AML_EXTERNAL_OP    0x15
#define AML_EXT_OP_PREFIX  0x5B
#define AML_RETURN_OP      0xA4
#define AML_NOOP_OP        0xA3
#define AML_ONES_OP        0xFF

// Extended opcodes (after 0x5B)
#define AML_EXT_MUTEX_OP        0x01
#define AML_EXT_EVENT_OP        0x02
#define AML_EXT_OP_REGION_OP    0x80
#define AML_EXT_FIELD_OP        0x81
#define AML_EXT_DEVICE_OP       0x82
#define AML_EXT_PROCESSOR_OP    0x83
#define AML_EXT_POWER_RES_OP    0x84
#define AML_EXT_THERMAL_ZONE_OP 0x85
#define AML_EXT_INDEX_FIELD_OP  0x86

// Name characters
#define AML_ROOT_CHAR      0x5C  // '\'
#define AML_PARENT_PREFIX  0x5E  // '^'
#define AML_DUAL_PREFIX    0x2E  // '.'
#define AML_MULTI_PREFIX   0x2F  // '/'
#define AML_NULL_NAME      0x00

static AMLHostBridge host_bridges[MAX_AML_HOST_BRIDGES];
static uint32 host_bridge_count = 0;

// Cursor over AML byte stream. Returns 0 on overflow.
typedef struct {
    const uint8 *base;
    uint32 pos;
    uint32 end;
} AmlCursor;

static int cur_avail(AmlCursor *c, uint32 n) {
    return c->pos + n <= c->end;
}

static uint8 cur_byte(AmlCursor *c) {
    return c->base[c->pos];
}

static uint8 cur_read_byte(AmlCursor *c) {
    return c->base[c->pos++];
}

// Read a PkgLength field. PkgLength includes its own bytes.
// Returns the total length, or 0 on error.
static uint32 read_pkg_length(AmlCursor *c) {
    if (!cur_avail(c, 1)) return 0;
    uint8 first = cur_read_byte(c);
    uint32 extra = (first >> 6) & 0x3;
    if (extra == 0) {
        return first & 0x3F;
    }
    if (!cur_avail(c, extra)) return 0;
    uint32 result = first & 0x0F;
    for (uint32 i = 0; i < extra; i++) {
        uint8 b = cur_read_byte(c);
        result |= ((uint32)b) << (4 + 8 * i);
    }
    return result;
}

// Read a NameString. Stores up to 4 characters of the LAST NameSeg in
// last_seg (zero-padded). Returns 0 on overflow.
static int read_name_string(AmlCursor *c, char last_seg[4]) {
    last_seg[0] = last_seg[1] = last_seg[2] = last_seg[3] = 0;

    if (!cur_avail(c, 1)) return 0;

    // Skip RootChar and ParentPrefixes
    while (cur_avail(c, 1) &&
           (cur_byte(c) == AML_ROOT_CHAR || cur_byte(c) == AML_PARENT_PREFIX)) {
        cur_read_byte(c);
    }

    if (!cur_avail(c, 1)) return 0;
    uint8 first = cur_byte(c);
    uint32 segs = 1;

    if (first == AML_NULL_NAME) {
        cur_read_byte(c);
        return 1;
    } else if (first == AML_DUAL_PREFIX) {
        cur_read_byte(c);
        segs = 2;
    } else if (first == AML_MULTI_PREFIX) {
        cur_read_byte(c);
        if (!cur_avail(c, 1)) return 0;
        segs = cur_read_byte(c);
    }

    // Each segment is 4 bytes
    if (!cur_avail(c, segs * 4)) return 0;
    for (uint32 i = 0; i < segs; i++) {
        for (int j = 0; j < 4; j++) {
            char ch = (char)cur_read_byte(c);
            if (i == segs - 1) last_seg[j] = ch;
        }
    }
    return 1;
}

static int name_eq(const char *name, const char *target) {
    return name[0] == target[0] && name[1] == target[1] &&
           name[2] == target[2] && name[3] == target[3];
}

// Read a constant integer DataRefObject. Advances cursor past the value.
// Returns 1 if a constant integer was found, 0 otherwise.
// On failure, the cursor position may be advanced by 1 byte (the op).
static int read_const_integer(AmlCursor *c, uint64 *out) {
    if (!cur_avail(c, 1)) return 0;
    uint8 op = cur_read_byte(c);
    switch (op) {
    case AML_ZERO_OP:
        *out = 0;
        return 1;
    case AML_ONE_OP:
        *out = 1;
        return 1;
    case AML_ONES_OP:
        *out = (uint64)-1;
        return 1;
    case AML_BYTE_PREFIX:
        if (!cur_avail(c, 1)) return 0;
        *out = cur_read_byte(c);
        return 1;
    case AML_WORD_PREFIX:
        if (!cur_avail(c, 2)) return 0;
        *out = cur_read_byte(c);
        *out |= ((uint64)cur_read_byte(c)) << 8;
        return 1;
    case AML_DWORD_PREFIX:
        if (!cur_avail(c, 4)) return 0;
        *out = cur_read_byte(c);
        *out |= ((uint64)cur_read_byte(c)) << 8;
        *out |= ((uint64)cur_read_byte(c)) << 16;
        *out |= ((uint64)cur_read_byte(c)) << 24;
        return 1;
    case AML_QWORD_PREFIX:
        if (!cur_avail(c, 8)) return 0;
        *out = 0;
        for (int i = 0; i < 8; i++)
            *out |= ((uint64)cur_read_byte(c)) << (8 * i);
        return 1;
    default:
        return 0;
    }
}

// Skip a DataRefObject of unknown type. Best-effort. Returns 0 on
// unrecognized opcode (caller should abort the surrounding TermList).
static int skip_data_ref_object(AmlCursor *c) {
    if (!cur_avail(c, 1)) return 0;
    uint8 op = cur_read_byte(c);
    switch (op) {
    case AML_ZERO_OP:
    case AML_ONE_OP:
    case AML_ONES_OP:
        return 1;
    case AML_BYTE_PREFIX:
        if (!cur_avail(c, 1)) return 0;
        c->pos += 1;
        return 1;
    case AML_WORD_PREFIX:
        if (!cur_avail(c, 2)) return 0;
        c->pos += 2;
        return 1;
    case AML_DWORD_PREFIX:
        if (!cur_avail(c, 4)) return 0;
        c->pos += 4;
        return 1;
    case AML_QWORD_PREFIX:
        if (!cur_avail(c, 8)) return 0;
        c->pos += 8;
        return 1;
    case AML_STRING_PREFIX:
        // Null-terminated ASCII
        while (cur_avail(c, 1) && cur_read_byte(c) != 0) { }
        return 1;
    case AML_BUFFER_OP:
    case AML_PACKAGE_OP:
    case AML_VAR_PACKAGE_OP: {
        uint32 pkg_start = c->pos;
        uint32 pkg_len = read_pkg_length(c);
        if (pkg_len == 0) return 0;
        c->pos = pkg_start + pkg_len;
        if (c->pos > c->end) return 0;
        return 1;
    }
    default:
        return 0;
    }
}

// Forward declaration
static void walk_term_list(AmlCursor *c, AMLHostBridge *current_device);

// Inside a Method body, look for `Return (Integer)` and extract the constant.
// Returns 1 if found.
static int find_constant_return(const uint8 *body, uint32 length, uint64 *out) {
    AmlCursor c = { body, 0, length };
    while (c.pos < c.end) {
        uint8 op = cur_byte(&c);
        if (op == AML_RETURN_OP) {
            c.pos++;
            if (read_const_integer(&c, out)) return 1;
            return 0;
        }
        // Skip unknown bytes one at a time, looking for the first Return.
        // This is a simple heuristic; works for "Method (X, 0) { Return (N) }"
        c.pos++;
    }
    return 0;
}

static void walk_term_list(AmlCursor *c, AMLHostBridge *current_device) {
    while (c->pos < c->end) {
        if (!cur_avail(c, 1)) return;
        uint8 op = cur_read_byte(c);

        if (op == AML_SCOPE_OP) {
            uint32 pkg_start = c->pos;
            uint32 pkg_len = read_pkg_length(c);
            if (pkg_len == 0 || pkg_start + pkg_len > c->end) return;
            uint32 pkg_end = pkg_start + pkg_len;
            char name[4];
            if (!read_name_string(c, name)) { c->pos = pkg_end; continue; }
            AmlCursor inner = { c->base, c->pos, pkg_end };
            walk_term_list(&inner, current_device);
            c->pos = pkg_end;
        } else if (op == AML_METHOD_OP) {
            uint32 pkg_start = c->pos;
            uint32 pkg_len = read_pkg_length(c);
            if (pkg_len == 0 || pkg_start + pkg_len > c->end) return;
            uint32 pkg_end = pkg_start + pkg_len;
            char name[4];
            if (!read_name_string(c, name)) { c->pos = pkg_end; continue; }
            // MethodFlags byte
            if (!cur_avail(c, 1)) { c->pos = pkg_end; continue; }
            uint8 method_flags = cur_read_byte(c);
            // Body is from c->pos to pkg_end
            if (current_device && !current_device->has_pxm &&
                name_eq(name, "_PXM") && (method_flags & 7) == 0) {
                uint64 value = 0;
                if (find_constant_return(c->base + c->pos,
                                         pkg_end - c->pos, &value)) {
                    current_device->proximity = (uint32)value;
                    current_device->has_pxm = 1;
                }
            }
            c->pos = pkg_end;
        } else if (op == AML_NAME_OP) {
            char name[4];
            if (!read_name_string(c, name)) return;
            uint64 value = 0;
            uint32 saved_pos = c->pos;
            if (read_const_integer(c, &value)) {
                if (current_device) {
                    if (name_eq(name, "_BBN")) {
                        current_device->bus_base = (uint8)value;
                        current_device->has_bbn = 1;
                    } else if (name_eq(name, "_PXM")) {
                        current_device->proximity = (uint32)value;
                        current_device->has_pxm = 1;
                    }
                }
            } else {
                // Not an integer; rewind and try to skip it generically
                c->pos = saved_pos;
                if (!skip_data_ref_object(c)) return;
            }
        } else if (op == AML_ALIAS_OP) {
            char name[4];
            if (!read_name_string(c, name)) return;
            if (!read_name_string(c, name)) return;
        } else if (op == AML_EXTERNAL_OP) {
            char name[4];
            if (!read_name_string(c, name)) return;
            if (!cur_avail(c, 2)) return;
            c->pos += 2;  // ObjectType + ArgumentCount
        } else if (op == AML_NOOP_OP) {
            // nothing
        } else if (op == AML_EXT_OP_PREFIX) {
            if (!cur_avail(c, 1)) return;
            uint8 ext = cur_read_byte(c);
            if (ext == AML_EXT_DEVICE_OP) {
                uint32 pkg_start = c->pos;
                uint32 pkg_len = read_pkg_length(c);
                if (pkg_len == 0 || pkg_start + pkg_len > c->end) return;
                uint32 pkg_end = pkg_start + pkg_len;
                char name[4];
                if (!read_name_string(c, name)) { c->pos = pkg_end; continue; }

                AMLHostBridge dev = {0};
                AmlCursor inner = { c->base, c->pos, pkg_end };
                walk_term_list(&inner, &dev);
                if (dev.has_bbn && dev.has_pxm &&
                    host_bridge_count < MAX_AML_HOST_BRIDGES) {
                    host_bridges[host_bridge_count++] = dev;
                }
                c->pos = pkg_end;
            } else if (ext == AML_EXT_FIELD_OP ||
                       ext == AML_EXT_POWER_RES_OP ||
                       ext == AML_EXT_THERMAL_ZONE_OP ||
                       ext == AML_EXT_PROCESSOR_OP ||
                       ext == AML_EXT_INDEX_FIELD_OP) {
                uint32 pkg_start = c->pos;
                uint32 pkg_len = read_pkg_length(c);
                if (pkg_len == 0 || pkg_start + pkg_len > c->end) return;
                c->pos = pkg_start + pkg_len;
            } else if (ext == AML_EXT_OP_REGION_OP) {
                char name[4];
                if (!read_name_string(c, name)) return;
                if (!cur_avail(c, 1)) return;
                c->pos += 1;  // RegionSpace byte
                if (!skip_data_ref_object(c)) return;  // Offset
                if (!skip_data_ref_object(c)) return;  // Length
            } else if (ext == AML_EXT_MUTEX_OP) {
                char name[4];
                if (!read_name_string(c, name)) return;
                if (!cur_avail(c, 1)) return;
                c->pos += 1;  // SyncFlags
            } else if (ext == AML_EXT_EVENT_OP) {
                char name[4];
                if (!read_name_string(c, name)) return;
            } else {
                // Unknown extended op; abort this term list
                return;
            }
        } else {
            // Unknown opcode; abort this term list
            return;
        }
    }
}

void aml_parse(const uint8 *body, uint32 length) {
    AmlCursor c = { body, 0, length };
    walk_term_list(&c, NULL);
}

int aml_bus_to_node(uint8 bus, uint32 *node_out) {
    if (!node_out) return 0;
    for (uint32 i = 0; i < host_bridge_count; i++) {
        if (host_bridges[i].has_bbn && host_bridges[i].has_pxm &&
            host_bridges[i].bus_base == bus) {
            *node_out = host_bridges[i].proximity;
            return 1;
        }
    }
    return 0;
}

uint32 aml_host_bridge_count(void) {
    return host_bridge_count;
}

const AMLHostBridge *aml_host_bridge(uint32 idx) {
    if (idx >= host_bridge_count) return (const AMLHostBridge *)0;
    return &host_bridges[idx];
}

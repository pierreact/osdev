// PCI vendor and class name lookup.
// Loaded at runtime from /DATA/PCI.IDS on the boot ISO.
// If the file is missing, lookups return NULL (graceful degradation).

#include <drivers/pci.h>
#include <drivers/monitor.h>
#include <fs/vfs.h>
#include <kernel/mem.h>

#define MAX_VENDORS 64
#define MAX_CLASSES 256

typedef struct { uint16 id; const char *name; } PciVendorEntry;
typedef struct { uint8 cls; uint8 sub; const char *name; } PciClassEntry;

static PciVendorEntry vendors[MAX_VENDORS];
static PciClassEntry  classes[MAX_CLASSES];
static uint32 vendor_count;
static uint32 class_count;
static char *ids_buf;    // raw file data, modified in-place

// Parse a single hex digit, return 0-15 or -1
static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Parse a 2-digit hex value from s[0..1], return value or -1
static int parse_hex8(const char *s) {
    int hi = hex_digit(s[0]);
    int lo = hex_digit(s[1]);
    if (hi < 0 || lo < 0) return -1;
    return (hi << 4) | lo;
}

// Parse a 4-digit hex value from s[0..3], return value or -1
static int parse_hex16(const char *s) {
    int h = parse_hex8(s);
    int l = parse_hex8(s + 2);
    if (h < 0 || l < 0) return -1;
    return (h << 8) | l;
}

void pci_ids_init(void) {
    uint32 file_size = vfs_file_size("/DATA/PCI.IDS");
    if (file_size == 0) {
        kprint("PCI-IDS: /DATA/PCI.IDS not found, names disabled\n");
        return;
    }

    ids_buf = (char *)kmalloc(file_size + 1);
    if (!ids_buf) {
        kprint("PCI-IDS: alloc failed\n");
        return;
    }

    if (vfs_read_file("/DATA/PCI.IDS", ids_buf, file_size) < 0) {
        kprint("PCI-IDS: read failed\n");
        kfree(ids_buf);
        ids_buf = 0;
        return;
    }
    ids_buf[file_size] = '\0';

    // Parse line by line. Newlines are replaced with NUL so name
    // pointers can point directly into ids_buf.
    vendor_count = 0;
    class_count = 0;

    char *p = ids_buf;
    char *end = ids_buf + file_size;
    while (p < end && *p) {
        // Find end of line
        char *eol = p;
        while (eol < end && *eol != '\n') eol++;

        // Terminate line (replace newline or mark end)
        if (eol < end) *eol = '\0';

        if (p[0] == 'V' && p[1] == ' ' && (eol - p) > 7) {
            // V XXXX Name
            int id = parse_hex16(p + 2);
            if (id >= 0 && vendor_count < MAX_VENDORS) {
                vendors[vendor_count].id = (uint16)id;
                vendors[vendor_count].name = p + 7;
                vendor_count++;
            }
        } else if (p[0] == 'C' && p[1] == ' ' && (eol - p) > 8) {
            // C XX XX Name
            int cls = parse_hex8(p + 2);
            int sub = parse_hex8(p + 5);
            if (cls >= 0 && sub >= 0 && class_count < MAX_CLASSES) {
                classes[class_count].cls = (uint8)cls;
                classes[class_count].sub = (uint8)sub;
                classes[class_count].name = p + 8;
                class_count++;
            }
        }

        p = eol + 1;
    }

    kprint("PCI-IDS: ");
    kprint_dec(vendor_count);
    kprint(" vendors, ");
    kprint_dec(class_count);
    kprint(" classes loaded\n");
}

const char *pci_vendor_name(uint16 vendor_id) {
    for (uint32 i = 0; i < vendor_count; i++)
        if (vendors[i].id == vendor_id) return vendors[i].name;
    return 0;
}

const char *pci_class_name(uint8 cls, uint8 sub) {
    // First try exact (cls, sub) match
    for (uint32 i = 0; i < class_count; i++)
        if (classes[i].cls == cls && classes[i].sub == sub)
            return classes[i].name;
    // Fall back to class-only match (sub == 0xFF)
    for (uint32 i = 0; i < class_count; i++)
        if (classes[i].cls == cls && classes[i].sub == 0xFF)
            return classes[i].name;
    return 0;
}

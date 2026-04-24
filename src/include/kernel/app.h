#ifndef KERNEL_APP_H
#define KERNEL_APP_H

#include <types.h>
#include <net/nic.h>

#define MAX_APPS         4
#define APP_NAME_LEN    32
#define APP_PATH_LEN    64
#define APP_LOAD_BASE   0x2000000    // first app at 32MB
#define APP_REGION_SIZE 0x1000000    // 16MB per app slot
#define MAX_APP_CORES   15           // max cores per app (APs only)

// Per-app network configuration from INI (L3). Zero = unset.
// ip / mask / gw are stored in network byte order.
typedef struct {
    uint32  ip;
    uint32  mask;
    uint32  gw;
    uint16  mtu;            // 0 = default (1500 applied at consumer)
    uint8   forward;        // 0 = drop non-local, 1 = forward
    uint8   reserved;
} AppNetConfig;

// Parsed manifest
typedef struct {
    char    name[APP_NAME_LEN];
    char    binary[APP_PATH_LEN];
    uint32  cores[MAX_APP_CORES];
    uint32  core_count;
    AppNetConfig net;
} AppManifest;

// Running application slot
typedef struct {
    uint8   active;
    char    name[APP_NAME_LEN];
    uint64  load_addr;
    uint32  file_size;
    uint32  cores[MAX_APP_CORES];
    uint8   core_count;
    uint8   cores_done;
    uint8   nic_locked[MAX_NICS];
    AppNetConfig net;
} AppSlot;

extern AppSlot app_table[MAX_APPS];
extern uint8   core_to_app[16];       // CPU index -> app slot (0xFF = free)

void app_init(void);
int  app_launch(const char *manifest_path);
int  app_check_completion(void);       // poll, clean up finished, return count
void app_list(void);                   // print running apps

#endif

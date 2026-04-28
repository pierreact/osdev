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
//
// IP assignment mirrors the system NIC assignment mode
// (sys.nic.mode, see src/include/net/nic.h):
//   APP_IP_MODE_PER_CORE: one IP per core, ip_count == core_count.
//                         ips[i] is for cores[i].
//   APP_IP_MODE_PER_NUMA: one IP per distinct NUMA node the app
//                         covers, ip_count == distinct node count.
//                         ips[i] is for the NUMA node at
//                         ip_numa_nodes[i]; cores on the same node
//                         share that IP (they share a NIC).
#define APP_IP_MODE_UNSET     0
#define APP_IP_MODE_PER_CORE  1
#define APP_IP_MODE_PER_NUMA  2

typedef struct {
    uint8   ip_mode;        // APP_IP_MODE_PER_CORE or APP_IP_MODE_PER_NUMA
    uint8   ip_count;       // entries used in ips[] / ip_numa_nodes[]
    uint8   reserved[2];
    uint32  ips[MAX_APP_CORES];
    uint32  ip_numa_nodes[MAX_APP_CORES];   // per-NUMA only; 0 in per-core
    uint32  mask;
    uint32  gw;
    uint16  mtu;            // 0 = default (1500 applied at consumer)
    uint8   forward;        // 0 = drop non-local, 1 = forward
    uint8   reserved2;
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

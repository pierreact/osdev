#ifndef ISURUS_NET_L2_KERN_H
#define ISURUS_NET_L2_KERN_H

#include <net/l2.h>

// Initialize BSP L2 contexts (management + inter-node).
// Called from kmain.s after nic_init.
void l2_kern_init(void);

// Access BSP L2 contexts (for shell commands, kernel consumers)
L2Context *l2_kern_mgmt(void);
L2Context *l2_kern_inter(void);

#endif

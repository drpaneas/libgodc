#include <kos.h>
#include <kos/fs_romdisk.h>

// Weak reference to romdisk symbol - only present if romdisk.o is linked
// bin2o creates this symbol pointing to the romdisk data
extern const uint8 romdisk[] __attribute__((weak));

// KOS looks for __kos_romdisk to know where the romdisk data is
// We initialize it directly to the romdisk symbol.
// If romdisk.o is not linked, the weak symbol resolves to NULL,
// and KOS will simply not mount anything at /rd.
const void *__kos_romdisk = romdisk;

// Override the weak function pointer to enable automatic romdisk mounting
// This tells KOS to call fs_romdisk_mount_builtin_legacy during init
extern void fs_romdisk_mount_builtin_legacy(void);
void (*fs_romdisk_mount_builtin_legacy_weak)(void) = fs_romdisk_mount_builtin_legacy;

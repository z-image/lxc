#ifndef __LXC_BDEV_H
#define __LXC_BDEV_H
/* blockdev operations for:
 * dir, raw, btrfs, overlayfs, aufs, lvm, loop, zfs, btrfs
 * someday: qemu-nbd, qcow2, qed
 */

#include "config.h"
#include "lxccontainer.h"

struct bdev;

struct bdev_ops {
	/* detect whether path is of this bdev type */
	int (*detect)(const char *path);
	// mount requires src and dest to be set.
	int (*mount)(struct bdev *bdev);
	int (*umount)(struct bdev *bdev);
	/* given original mount, rename the paths for cloned container */
	int (*clone_paths)(struct bdev *orig, struct bdev *new, const char *oldname,
			const char *cname, const char *oldpath, const char *lxcpath,
			int snap, unsigned long newsize);
};

struct bdev {
	struct bdev_ops *ops;
	char *type;
	char *src;
	char *dest;
	char *data;
};

/*
 * Instantiate a bdev object.  The src is used to determine which blockdev
 * type this should be.  The dst and data are optional, and will be used
 * in case of mount/umount.
 *
 * Optionally, src can be 'dir:/var/lib/lxc/c1' or 'lvm:/dev/lxc/c1'.  For
 * other backing stores, this will allow additional options.  In particular,
 * "overlayfs:/var/lib/lxc/canonical/rootfs:/var/lib/lxc/c1/delta" will mean
 * use /var/lib/lxc/canonical/rootfs as lower dir, and /var/lib/lxc/c1/delta
 * as the upper, writeable layer.
 */
struct bdev *bdev_init(const char *src, const char *dst, const char *data);

struct bdev *bdev_copy(const char *src, const char *oldname, const char *cname,
			const char *oldpath, const char *lxcpath, const char *bdevtype,
			int snap, const char *bdevdata, unsigned long newsize);
void bdev_put(struct bdev *bdev);
#endif

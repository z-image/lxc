/*
 *
 * Copyright © 2013 Serge Hallyn <serge.hallyn@ubuntu.com>.
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "../lxc/lxccontainer.h"

#include <stdio.h>
#include <libgen.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>

#include <lxc/lxc.h>
#include <lxc/log.h>
#include <lxc/bdev.h>

#include "arguments.h"
#include "utils.h"

lxc_log_define(lxc_create, lxc);

/* we pass fssize in bytes */
static unsigned long get_fssize(char *s)
{
	unsigned long ret;
	char *end;

	ret = strtoul(s, &end, 0);
	if (end == s)
		return 0;
	while (isblank(*end))
		end++;
	if (!(*end))
		return ret;
	if (*end == 'g' || *end == 'G')
		ret *= 1000000000;
	else if (*end == 'm' || *end == 'M')
		ret *= 1000000;
	else if (*end == 'k' || *end == 'K')
		ret *= 1000;
	return ret;
}

static int my_parser(struct lxc_arguments* args, int c, char* arg)
{
	switch (c) {
	case 'B': args->bdevtype = arg; break;
	case 'f': args->configfile = arg; break;
	case 't': args->template = arg; break;
	case '0': args->lvname = arg; break;
	case '1': args->vgname = arg; break;
	case '2': args->fstype = arg; break;
	case '3': args->fssize = get_fssize(arg); break;
	case '4': args->zfsroot = arg; break;
	case '5': args->dir = arg; break;
	}
	return 0;
}

static const struct option my_longopts[] = {
	{"bdev", required_argument, 0, 'B'},
	{"config", required_argument, 0, 'f'},
	{"template", required_argument, 0, 't'},
	{"lvname", required_argument, 0, '0'},
	{"vgname", required_argument, 0, '1'},
	{"fstype", required_argument, 0, '2'},
	{"fssize", required_argument, 0, '3'},
	{"zfsroot", required_argument, 0, '4'},
	{"dir", required_argument, 0, '5'},
	LXC_COMMON_OPTIONS
};

static struct lxc_arguments my_args = {
	.progname = "lxc-create",
	.help     = "\
--name=NAME [-w] [-r] [-t timeout] [-P lxcpath]\n\
\n\
lxc-creae creates a container\n\
\n\
Options :\n\
  -n, --name=NAME   NAME for name of the container\n\
  -f, --config=file initial configuration file\n\
  -t, --template=t  template to use to setup container\n\
  -B, --bdev=BDEV   backing store type to use\n\
  --lxcpath=PATH    place container under PATH\n\
  --lvname=LVNAME   Use LVM lv name LVNAME\n\
                    (Default: container name)\n\
  --vgname=VG       Use LVM vg called VG\n\
                    (Default: lxc))\n\
  --fstype=TYPE     Create fstype TYPE\n\
                    (Default: ext3))\n\
  --fssize=SIZE     Create filesystem of size SIZE\n\
                    (Default: 1G))\n\
  --dir=DIR         Place rootfs directory under DIR\n\
  --zfsroot=PATH    Create zfs under given zfsroot\n\
                    (Default: tank/lxc))\n",
	.options  = my_longopts,
	.parser   = my_parser,
	.checker  = NULL,
};

bool validate_bdev_args(struct lxc_arguments *a)
{
	if (strcmp(a->bdevtype, "lvm") != 0) {
		if (a->fstype || a->fssize) {
			fprintf(stderr, "filesystem type and size are only valid with block devices\n");
			return false;
		}
		if (a->lvname || a->vgname) {
			fprintf(stderr, "--lvname and --vgname are only valid with -B lvm\n");
			return false;
		}
	}
	if (strcmp(a->bdevtype, "zfs") != 0) {
		if (a->zfsroot) {
			fprintf(stderr, "zfsroot is only valid with -B zfs\n");
			return false;
		}
	}
	return true;
}

/* grab this through autoconf from @config-path@ ? */
#define DEFAULT_CONFIG "/etc/lxc/default.conf"
int main(int argc, char *argv[])
{
	struct lxc_container *c;
	struct bdev_specs spec;

	/* this is a short term test.  We'll probably want to check for
	 * write access to lxcpath instead */
	if (geteuid()) {
		fprintf(stderr, "%s must be run as root\n", argv[0]);
		exit(1);
	}

	if (lxc_arguments_parse(&my_args, argc, argv))
		exit(1);

	if (lxc_log_init(my_args.name, my_args.log_file, my_args.log_priority,
			 my_args.progname, my_args.quiet, my_args.lxcpath[0]))
		exit(1);

	memset(&spec, 0, sizeof(spec));
	if (!my_args.bdevtype)
		my_args.bdevtype = "_unset";
	if (!validate_bdev_args(&my_args))
		exit(1);

	c = lxc_container_new(my_args.name, my_args.lxcpath[0]);
	if (!c) {
		fprintf(stderr, "System error loading container\n");
		exit(1);
	}
	if (c->is_defined(c)) {
		fprintf(stderr, "Container already exists\n");
		exit(1);
	}
	if (my_args.configfile)
		c->load_config(c, my_args.configfile);
	else
		c->load_config(c, DEFAULT_CONFIG);

	if (strcmp(my_args.bdevtype, "zfs") == 0) {
		if (my_args.zfsroot)
			spec.u.zfs.zfsroot = my_args.zfsroot;
	} else if (strcmp(my_args.bdevtype, "lvm") == 0) {
		if (my_args.lvname)
			spec.u.lvm.lv = my_args.lvname;
		if (my_args.vgname)
			spec.u.lvm.vg = my_args.vgname;
		if (my_args.fstype)
			spec.u.lvm.fstype = my_args.fstype;
		if (my_args.fssize)
			spec.u.lvm.fssize = my_args.fssize;
	} else if (my_args.dir) {
		ERROR("--dir is not yet supported");
		exit(1);
	}

	if (strcmp(my_args.bdevtype, "_unset") == 0)
		my_args.bdevtype = NULL;
	if (!c->create(c, my_args.template, my_args.bdevtype, &spec, &argv[optind])) {
		ERROR("Error creating container %s", c->name);
		lxc_container_put(c);
		exit(1);
	}
	INFO("container %s created", c->name);
	exit(0);
}

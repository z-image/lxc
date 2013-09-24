/*
 * lxc: linux Container library
 *
 * Copyright © 2013 Oracle.
 *
 * Authors:
 * Dwight Engen <dwight.engen@oracle.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __lxc_lsm_h
#define __lxc_lsm_h

struct lxc_conf;

#include <sys/types.h>

struct lsm_drv {
	const char *name;

	char *(*process_label_get)(pid_t pid);
	int   (*process_label_set)(const char *label, int use_default);
};

#if HAVE_APPARMOR || HAVE_SELINUX
void  lsm_init(void);
char *lsm_process_label_get(pid_t pid);
int   lsm_process_label_set(const char *label, int use_default);
int   lsm_proc_mount(struct lxc_conf *lxc_conf);
void  lsm_proc_unmount(struct lxc_conf *lxc_conf);
#else
static inline void  lsm_init(void) { }
static inline char *lsm_process_label_get(pid_t pid) { return NULL; }
static inline int   lsm_process_label_set(char *label, int use_default) { return 0; }
static inline int   lsm_proc_mount(struct lxc_conf *lxc_conf) { return 0; }
static inline void  lsm_proc_unmount(struct lxc_conf *lxc_conf) { }
#endif

#endif

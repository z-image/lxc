/* liblxcapi
 *
 * Copyright © 2012 Serge Hallyn <serge.hallyn@ubuntu.com>.
 * Copyright © 2012 Canonical Ltd.
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

#include <pthread.h>
#include "lxclock.h"
#include <malloc.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <lxc/utils.h>
#include <lxc/log.h>

#define OFLAG (O_CREAT | O_RDWR)
#define SEMMODE 0660
#define SEMVALUE 1
#define SEMVALUE_LOCKED 0

lxc_log_define(lxc_lock, lxc);

pthread_mutex_t thread_mutex = PTHREAD_MUTEX_INITIALIZER;

static char *lxclock_name(const char *p, const char *n)
{
	int ret;
	// $lxcpath/locks/$lxcname + '\0'
	int len = strlen(p) + strlen(n) + strlen("/locks/") + 1;
	char *dest = malloc(len);
	if (!dest)
		return NULL;
	ret = snprintf(dest, len, "%s/locks", p);
	if (ret < 0 || ret >= len) {
		free(dest);
		return NULL;
	}
	if (mkdir_p(dest, 0755) < 0) {
		free(dest);
		return NULL;
	}

	ret = snprintf(dest, len, "%s/locks/%s", p, n);
	if (ret < 0 || ret >= len) {
		free(dest);
		return NULL;
	}
	return dest;
}

static sem_t *lxc_new_unnamed_sem(void)
{
	sem_t *s;
	int ret;

	s = malloc(sizeof(*s));
	if (!s)
		return NULL;
	ret = sem_init(s, 0, 1);
	if (ret) {
		free(s);
		return NULL;
	}
	return s;
}

struct lxc_lock *lxc_newlock(const char *lxcpath, const char *name)
{
	struct lxc_lock *l;
	int ret = pthread_mutex_lock(&thread_mutex);
	if (ret != 0) {
		ERROR("pthread_mutex_lock returned:%d %s", ret, strerror(ret));
		return NULL;
	}

	l = malloc(sizeof(*l));
	if (!l)
		goto out;

	if (!name) {
		l->type = LXC_LOCK_ANON_SEM;
		l->u.sem = lxc_new_unnamed_sem();
		goto out;
	}

	l->type = LXC_LOCK_FLOCK;
	l->u.f.fname = lxclock_name(lxcpath, name);
	if (!l->u.f.fname) {
		free(l);
		l = NULL;
		goto out;
	}
	l->u.f.fd = -1;

out:
	pthread_mutex_unlock(&thread_mutex);
	return l;
}

int lxclock(struct lxc_lock *l, int timeout)
{
	int saved_errno = errno;
	int ret = pthread_mutex_lock(&thread_mutex);
	if (ret != 0) {
		ERROR("pthread_mutex_lock returned:%d %s", ret, strerror(ret));
		return ret;
	}

	switch(l->type) {
	case LXC_LOCK_ANON_SEM:
		if (!timeout) {
			ret = sem_wait(l->u.sem);
			if (ret == -1)
				saved_errno = errno;
		} else {
			struct timespec ts;
			if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
				ret = -2;
				goto out;
			}
			ts.tv_sec += timeout;
			ret = sem_timedwait(l->u.sem, &ts);
			if (ret == -1)
				saved_errno = errno;
		}
		break;
	case LXC_LOCK_FLOCK:
		ret = -2;
		if (timeout) {
			ERROR("Error: timeout not supported with flock");
			ret = -2;
			goto out;
		}
		if (!l->u.f.fname) {
			ERROR("Error: filename not set for flock");
			ret = -2;
			goto out;
		}
		if (l->u.f.fd == -1) {
			l->u.f.fd = open(l->u.f.fname, O_RDWR|O_CREAT,
					S_IWUSR | S_IRUSR);
			if (l->u.f.fd == -1) {
				ERROR("Error opening %s", l->u.f.fname);
				goto out;
			}
		}
		ret = flock(l->u.f.fd, LOCK_EX);
		if (ret == -1)
			saved_errno = errno;
		break;
	}

out:
	pthread_mutex_unlock(&thread_mutex);
	errno = saved_errno;
	return ret;
}

int lxcunlock(struct lxc_lock *l)
{
	int saved_errno = errno;
	int ret = pthread_mutex_lock(&thread_mutex);

	if (ret != 0) {
		ERROR("pthread_mutex_lock returned:%d %s", ret, strerror(ret));
		return ret;
	}

	switch(l->type) {
	case LXC_LOCK_ANON_SEM:
		if (!l->u.sem)
			ret = -2;
		else
			ret = sem_post(l->u.sem);
			saved_errno = errno;
		break;
	case LXC_LOCK_FLOCK:
		if (l->u.f.fd != -1) {
			if ((ret = flock(l->u.f.fd, LOCK_UN)) < 0)
				saved_errno = errno;
			close(l->u.f.fd);
			l->u.f.fd = -1;
		} else
			ret = -2;
		break;
	}

	pthread_mutex_unlock(&thread_mutex);
	errno = saved_errno;
	return ret;
}

void lxc_putlock(struct lxc_lock *l)
{
	int ret = pthread_mutex_lock(&thread_mutex);
	if (ret != 0) {
		ERROR("pthread_mutex_lock returned:%d %s", ret, strerror(ret));
		return;
	}

	if (!l)
		goto out;
	switch(l->type) {
	case LXC_LOCK_ANON_SEM:
		if (l->u.sem)
			sem_close(l->u.sem);
		break;
	case LXC_LOCK_FLOCK:
		if (l->u.f.fd != -1) {
			close(l->u.f.fd);
			l->u.f.fd = -1;
		}
		if (l->u.f.fname) {
			free(l->u.f.fname);
			l->u.f.fname = NULL;
		}
		break;
	}
out:
	pthread_mutex_unlock(&thread_mutex);
}

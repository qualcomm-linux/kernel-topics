// SPDX-License-Identifier: GPL-2.0
#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/loop.h>
#include <linux/magic.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/vfs.h>

#include "kselftest_harness.h"
#include "wrappers.h"

/*
 * Put @fd behind a free loop device. The device is set up with
 * LO_FLAGS_AUTOCLEAR and goes away again when it is closed here.
 */
static int loop_attach(int fd, bool rw, char *dev, size_t len)
{
	struct loop_config cfg = {};
	int ctl, nr, lfd, ret = 0;

	ctl = open("/dev/loop-control", O_RDWR | O_CLOEXEC);
	if (ctl < 0)
		return -errno;
	nr = ioctl(ctl, LOOP_CTL_GET_FREE);
	if (nr < 0)
		ret = -errno;
	close(ctl);
	if (ret)
		return ret;

	snprintf(dev, len, "/dev/loop%d", nr);
	lfd = open(dev, (rw ? O_RDWR : O_RDONLY) | O_CLOEXEC);
	if (lfd < 0)
		return -errno;

	cfg.fd = fd;
	cfg.info.lo_flags = LO_FLAGS_AUTOCLEAR;
	if (!rw)
		cfg.info.lo_flags |= LO_FLAGS_READ_ONLY;
	if (ioctl(lfd, LOOP_CONFIGURE, &cfg)) {
		ret = -errno;
		close(lfd);
	} else {
		ret = lfd;
	}
	return ret;
}

static int attach_path(const char *path, bool rw)
{
	char dev[PATH_MAX];
	int fd, ret;

	fd = open(path, (rw ? O_RDWR : O_RDONLY) | O_CLOEXEC);
	if (fd < 0)
		return -errno;
	ret = loop_attach(fd, rw, dev, sizeof(dev));
	close(fd);
	if (ret >= 0) {
		close(ret);
		ret = 0;
	}
	return ret;
}

static bool can_use_loop(void)
{
	return geteuid() == 0 && access("/dev/loop-control", R_OK | W_OK) == 0;
}

TEST(memfd_backing_file)
{
	char dev[PATH_MAX];
	int fd, lfd;

	if (!can_use_loop())
		SKIP(return, "loop devices unavailable");

	fd = memfd_create("pseudo_backing_test", MFD_CLOEXEC);
	ASSERT_GE(fd, 0);
	ASSERT_EQ(ftruncate(fd, 1 << 20), 0);

	lfd = loop_attach(fd, true, dev, sizeof(dev));
	ASSERT_GE(lfd, 0);
	EXPECT_EQ(close(lfd), 0);
	EXPECT_EQ(close(fd), 0);
}

TEST(loop_device_backing_file)
{
	char dev[PATH_MAX], dev2[PATH_MAX];
	int fd, lfd, bfd, lfd2;

	if (!can_use_loop())
		SKIP(return, "loop devices unavailable");

	fd = memfd_create("pseudo_backing_test", MFD_CLOEXEC);
	ASSERT_GE(fd, 0);
	ASSERT_EQ(ftruncate(fd, 1 << 20), 0);
	lfd = loop_attach(fd, true, dev, sizeof(dev));
	ASSERT_GE(lfd, 0);

	/* A block device stays fine as a backing file. */
	bfd = open(dev, O_RDWR | O_CLOEXEC);
	ASSERT_GE(bfd, 0);
	lfd2 = loop_attach(bfd, true, dev2, sizeof(dev2));
	ASSERT_GE(lfd2, 0);

	EXPECT_EQ(close(lfd2), 0);
	EXPECT_EQ(close(bfd), 0);
	EXPECT_EQ(close(lfd), 0);
	EXPECT_EQ(close(fd), 0);
}

TEST(write_only_backing_file)
{
	char dev[PATH_MAX], path[PATH_MAX];
	int fd, wfd, lfd;

	if (!can_use_loop())
		SKIP(return, "loop devices unavailable");

	fd = memfd_create("pseudo_backing_test", MFD_CLOEXEC);
	ASSERT_GE(fd, 0);
	ASSERT_EQ(ftruncate(fd, 1 << 20), 0);

	/* Loop reads from its backing file so it must be open for reading. */
	snprintf(path, sizeof(path), "/proc/self/fd/%d", fd);
	wfd = open(path, O_WRONLY | O_CLOEXEC);
	ASSERT_GE(wfd, 0);

	lfd = loop_attach(wfd, true, dev, sizeof(dev));
	if (lfd >= 0)
		close(lfd);
	EXPECT_EQ(lfd, -EBADF);
	EXPECT_EQ(close(wfd), 0);
	EXPECT_EQ(close(fd), 0);
}

TEST(procfs_backing_file)
{
	if (!can_use_loop())
		SKIP(return, "loop devices unavailable");

	ASSERT_EQ(attach_path("/proc/cpuinfo", false), -EINVAL);
}

TEST(sysfs_backing_file)
{
	if (!can_use_loop())
		SKIP(return, "loop devices unavailable");

	ASSERT_EQ(attach_path("/sys/kernel/warn_count", false), -EINVAL);

	if (access("/sys/kernel/reboot/mode", F_OK))
		SKIP(return, "no /sys/kernel/reboot/mode");
	ASSERT_EQ(attach_path("/sys/kernel/reboot/mode", true), -EINVAL);
}

TEST(overlayfs_on_sysfs_backing_file)
{
	if (!can_use_loop())
		SKIP(return, "loop devices unavailable");

	ASSERT_EQ(unshare(CLONE_NEWNS), 0);
	ASSERT_EQ(mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL), 0);
	ASSERT_EQ(mount("tmpfs", "/tmp", "tmpfs", 0, NULL), 0);
	ASSERT_EQ(mkdir("/tmp/empty", 0755), 0);
	ASSERT_EQ(mkdir("/tmp/merged", 0755), 0);

	/* A lower-only overlayfs serves reads from sysfs. */
	if (mount("overlay", "/tmp/merged", "overlay", MS_RDONLY,
		  "lowerdir=/sys/kernel:/tmp/empty"))
		SKIP(return, "no overlayfs on top of sysfs: %s", strerror(errno));

	ASSERT_EQ(attach_path("/tmp/merged/warn_count", false), -EINVAL);
}

#define CONFIGFS_DIR "/sys/kernel/config"
#define GADGET_DIR CONFIGFS_DIR "/usb_gadget/pseudo_backing_test"
#define MASS_STORAGE_DIR GADGET_DIR "/functions/mass_storage.0"

TEST(usb_mass_storage_backing_file)
{
	const char *path = "/sys/kernel/warn_count";
	struct statfs sfs;
	ssize_t ret;
	int fd, err;

	if (geteuid())
		SKIP(return, "needs root");

	/* Mounting configfs on top of itself fails with EBUSY. */
	if (statfs(CONFIGFS_DIR, &sfs) || sfs.f_type != CONFIGFS_MAGIC) {
		ASSERT_EQ(unshare(CLONE_NEWNS), 0);
		ASSERT_EQ(mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL), 0);
		if (mount("configfs", CONFIGFS_DIR, "configfs", 0, NULL))
			SKIP(return, "no configfs: %s", strerror(errno));
	}

	if (mkdir(GADGET_DIR, 0755))
		SKIP(return, "no usb gadget configfs: %s", strerror(errno));
	if (mkdir(MASS_STORAGE_DIR, 0755)) {
		err = errno;
		rmdir(GADGET_DIR);
		SKIP(return, "no usb mass storage function: %s", strerror(err));
	}

	/* Writing the path opens the LUN's backing file right away. */
	fd = open(MASS_STORAGE_DIR "/lun.0/file", O_WRONLY | O_CLOEXEC);
	EXPECT_GE(fd, 0);
	if (fd >= 0) {
		ret = write(fd, path, strlen(path));
		err = errno;
		EXPECT_EQ(ret, -1);
		EXPECT_EQ(err, EINVAL);
		EXPECT_EQ(close(fd), 0);
	}

	EXPECT_EQ(rmdir(MASS_STORAGE_DIR), 0);
	EXPECT_EQ(rmdir(GADGET_DIR), 0);
}

TEST(erofs_backing_file)
{
	bool refused = false;
	char buf[256];
	int fsfd, ret, err;
	ssize_t n;

	if (geteuid())
		SKIP(return, "needs root");

	fsfd = sys_fsopen("erofs", FSOPEN_CLOEXEC);
	if (fsfd < 0)
		SKIP(return, "no erofs: %s", strerror(errno));

	ASSERT_EQ(sys_fsconfig(fsfd, FSCONFIG_SET_STRING, "source",
			       "/sys/kernel/warn_count", 0), 0);
	ret = sys_fsconfig(fsfd, FSCONFIG_CMD_CREATE, NULL, NULL, 0);
	err = errno;
	if (ret && err == ENOTBLK) {
		close(fsfd);
		SKIP(return, "no file-backed erofs mounts");
	}
	ASSERT_EQ(ret, -1);
	ASSERT_EQ(err, EINVAL);

	/*
	 * Something that isn't an erofs image fails with EINVAL as well so
	 * check that the source itself was refused.
	 */
	while ((n = read(fsfd, buf, sizeof(buf) - 1)) > 0) {
		buf[n] = '\0';
		if (strstr(buf, "pseudo filesystem"))
			refused = true;
	}
	EXPECT_TRUE(refused);
	EXPECT_EQ(close(fsfd), 0);
}

TEST_HARNESS_MAIN

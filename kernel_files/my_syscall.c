#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>
#include <linux/uaccess.h>

SYSCALL_DEFINE2(my_gettime, long long int*, sec, long int*, nsec) {
    struct timespec64 ts;
    ts = ktime_to_timespec64(ktime_get_real());
    copy_to_user(sec, &ts.tv_sec, sizeof(long long int));
    copy_to_user(nsec, &ts.tv_nsec, sizeof(long int));
    return 0;
}

SYSCALL_DEFINE5(my_print, pid_t, pid, int, start_sec, int, start_nsec, int, stop_sec, int, stop_nsec) {
    printk(KERN_INFO "[Project1] %d %d.%09d %d.%09d\n", pid, start_sec, start_nsec, stop_sec, stop_nsec);
    return 0;
}

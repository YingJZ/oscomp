/*copyright: 2023 farmos*/
#include <kernel/syscall_ids.h>
#include <kernel/syscall_file.h>
#include <kernel/syscall.h>


// 按字母顺序排列
struct syscall_function sys_table[] = {
    [1023] = {0, 0},
    // [SYS_accept] = {sys_accept, "accept"},
    // [SYS_bind] = {sys_bind, "bind"},
    // [SYS_brk] = {sys_brk, "brk"},
    // [SYS_chdir] = {sys_chdir, "chdir"},
    // [SYS_clock_gettime] = {sys_clock_gettime, "clock_gettime"},
    // [SYS_clock_nanosleep] = {sys_clock_nanosleep, "clock_nanosleep"},
    // [SYS_clone] = {sys_clone, "clone"},
    // [SYS_close] = {sys_close, "close"},
    // [SYS_connect] = {sys_connect, "connect"},
    // [SYS_copy_file_range] = {sys_copy_file_range, "copy_file_range"},
    // [SYS_dup] = {sys_dup, "dup"},
    // [SYS_dup3] = {sys_dup3, "dup3"},
    // [SYS_execve] = {sys_exec, "execve"},
    // [SYS_exit] = {sys_exit, "exit"},
    // [SYS_faccessat] = {sys_faccessat, "faccessat"},
    // [SYS_fchmod] = {sys_fchmod, "fchmod"},
    // [SYS_fcntl] = {sys_fcntl, "fcntl"},
    // [SYS_fstat] = {sys_fstat, "fstat"},
    // [SYS_fstatat] = {sys_fstatat, "fstatat"},
    // [SYS_fsync] = {sys_fsync, "fsync"},
    // [SYS_ftruncate] = {sys_ftruncate, "ftruncate"},
    // [SYS_futex] = {sys_futex, "futex"},
    // [SYS_getcwd] = {sys_getcwd, "getcwd"},
    // [SYS_getdents64] = {sys_getdents64, "getdents64"},
    // [SYS_getegid] = {sys_getegid, "getegid"},
    // [SYS_geteuid] = {sys_geteuid, "geteuid"},
    // [SYS_getgid] = {sys_getgid, "getgid"},
    // [SYS_getitimer] = {sys_getitimer, "getitimer"},
    // [SYS_getpeername] = {sys_getpeername, "getpeername"},
    // [SYS_getpgid] = {sys_getpgid, "getpgid"},
    // [SYS_getpid] = {sys_getpid, "getpid"},
    // [SYS_getppid] = {sys_getppid, "getppid"},
    // [SYS_getrandom] = {sys_getrandom, "getrandom"},
    // [SYS_getrusage] = {sys_getrusage, "getrusage"},
    // [SYS_getsid] = {sys_getsid, "getsid"},
    // [SYS_getsockname] = {sys_getsocketname, "getsockname"},
    // [SYS_getsockopt] = {sys_getsockopt, "getsockopt"},
    // [SYS_gettid] = {sys_gettid, "gettid"},
    // [SYS_gettimeofday] = {sys_gettimeofday, "gettimeofday"},
    // [SYS_getuid] = {sys_getuid, "getuid"},
    // [SYS_ioctl] = {sys_ioctl, "ioctl"},
    // [SYS_kill] = {sys_kill, "kill"},
    // [SYS_linkat] = {sys_linkat, "linkat"},
    // [SYS_listen] = {sys_listen, "listen"},
    // [SYS_lseek] = {sys_lseek, "lseek"},
    // [SYS_madvise] = {sys_madvise, "sys_madvise"},
    // [SYS_membarrier] = {sys_membarrier, "membarrier"},
    // [SYS_mkdirat] = {sys_mkdirat, "mkdirat"},

		// [SYS_mmap] = {sys_mmap, "mkdirat"},
    // [SYS_mount] = {sys_mount, "mount"},
    //[SYS_mprotect] = {sys_mprotect, "mprotect"},
    // [SYS_msync] = {sys_msync, "msync"},
    //[SYS_munmap] = {sys_unmap, "munmap"},
    // [SYS_nanosleep] = {sys_nanosleep, "nanosleep"},
    // [SYS_openat] = {sys_openat, "openat"},
    // [SYS_pipe2] = {sys_pipe2, "pipe2"},
    // [SYS_pread64] = {sys_pread64, "pread64"},
    // [SYS_pselect6] = {sys_pselect6, "pselect6"},
    // [SYS_prlimit64] = {sys_prlimit64, "prlimit64"},
    // [SYS_pwrite64] = {sys_pwrite64, "pwrite64"},
    // [SYS_read] = {sys_read, "read"},
    // [SYS_readlinkat] = {sys_readlinkat, "readlinkat"},
    // [SYS_readv] = {sys_readv, "readv"},
    // [SYS_reboot] = {sys_reboot, "reboot"},
    // [SYS_recvfrom] = {sys_recvfrom, "recvfrom"},
    // [SYS_renameat2] = {sys_renameat2, "renameat2"},
    // [SYS_rt_sigaction] = {sys_sigaction, "sigaction"},
    // [SYS_rt_sigprocmask] = {sys_sigprocmask, "sigprocmask"},
    // [SYS_rt_sigsuspend] = {sys_sigsuspend, "sigsuspend"},
    // [SYS_rt_sigreturn] = {sys_sigreturn, "sigreturn"},
    // [SYS_rt_sigtimedwait] = {sys_sigtimedwait, "sigtimedwait"},
    // [SYS_sched_getaffinity] = {sys_sched_getaffinity, "sched_getaffinity"},
    // [SYS_sched_getparam] = {sys_sched_getparam, "sched_getparam"},
    // [SYS_sched_getscheduler] = {sys_sched_getscheduler, "sched_getscheduler"},
    // [SYS_sched_setaffinity] = {sys_sched_setaffinity, "sched_setaffinity"},
    // [SYS_sched_setscheduler] = {sys_sched_setscheduler, "sched_setscheduler"},
    // [SYS_sched_yield] = {sys_sched_yield, "sched_yield"},
    // [SYS_sendto] = {sys_sendto, "sendto"},
    // [SYS_setgroups] = {sys_setgroups, "setgroups"},
    // [SYS_setitimer] = {sys_setitimer, "setitimer"},
    // [SYS_setpgid] = {sys_setpgid, "setpgid"},
    // [SYS_setsid] = {sys_setsid, "setsid"},
    // [SYS_set_tid_address] = {sys_set_tid_address, "set_tid_address"},
};

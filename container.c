#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <linux/sched.h>
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>

#define HOSTNAME "container"
#define MAX_PIDS "5"
#define CGROUPS_PATH "/sys/fs/cgroup/pids/mycontainer/"
#define STACK_SIZE 1024*1024

#define concat(a,b) (a"" b)



char* stack_memory() {
    void *stack = malloc(STACK_SIZE);

    if (stack == NULL) {
        printf("Error: failed to allocate memory");
        return 0;
    }

    // Moving the pointer to the end of the stack because it grows backward
    return ((char*) stack) + STACK_SIZE;
}

void write_rule_cgroups(const char* path, const char* value) {
    int fp = open(path, O_WRONLY | O_APPEND);
    write(fp, value, strlen(value));
    close(fp);
}

char* int_to_str(int x) {
    int length = snprintf( NULL, 0, "%d", x);
    char* str = (char*) malloc(length + 1);
    snprintf(str, length + 1, "%d", x);
    return str;
}

void cgroups_limit_process() {
    mkdir(CGROUPS_PATH, S_IRUSR | S_IWUSR);
    char* pid_str = int_to_str(getpid());

    write_rule_cgroups(concat(CGROUPS_PATH, "cgroup.procs"), pid_str);
    write_rule_cgroups(concat(CGROUPS_PATH, "pids.max"), MAX_PIDS);

    // Clean resources
    write_rule_cgroups(concat(CGROUPS_PATH, "notify_on_release"), "1");
    free(pid_str);
}

void change_root(const char* folder) {
    chroot(folder);
    chdir("/");
}

int run(const char *name) {
    char *_args[] = {(char *) name, (char *) 0};
    return execvp(name, _args);
}

void setup_env() {
    clearenv(); // Clear all env vars
    setenv("TERM", "xterm-256color", 0);
    setenv("PATH", "/bin/:/sbin/:usr/bin:/usr/sbin", 0);
}

int run_shell(void* args) {
    return run("/bin/sh");
}

int container(void* args) {
    printf("Hello from child! Child's PID: %d\n", getpid());

    // Putting container filesystem in a file by using loop
    system("dd if=/dev/zero of=loopbackfile.img bs=100M count=10"); // Created zeroed
    system("losetup -fP loopbackfile.img"); // Making block device
    system("mkfs.ext4 loopbackfile.img"); // Creating fs on block device
    system("mkdir tmp_mnt"); // Creating tmp folder for future mount
    system("mount -o loop /dev/loop0 tmp_mnt"); // Mounting created block device to this directory

    // Setup network
    sethostname(HOSTNAME, strlen(HOSTNAME));
    system("ifconfig veth1 10.0.0.4");

    cgroups_limit_process();
    setup_env();
    change_root("./root");

    mount("proc", "/proc", "proc", 0, 0);
    mount("dev", "/dev", "tmpfs", MS_NOSUID | MS_STRICTATIME, NULL);

    int pid = clone(run_shell, stack_memory(), SIGCHLD, 0);
    printf("Shell PID: %d\n", pid);

    wait(NULL); // Wait for shell until exit

    // Clean resources
    umount("proc");
    umount("/dev/loop0");
    umount("dev");
    return 0;
}

int main(int argc, char** argv) {
    printf("Program started!\n");

    int pid = clone(container, stack_memory(), CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNET | CLONE_NEWNS | SIGCHLD, 0);
    printf("Child PID: %d\n", pid);

    char buf[255];
    // Creating virtual Ethernet connection(bridge) between parent and child namespaces
    sprintf(buf,"ip link add name veth0 type veth peer name veth1 netns %d",pid);
    system(buf);
    system("ifconfig veth0 10.0.0.3");

    // Clean resources
    memset(buf, 0, sizeof buf);
    char cwd[255];
    wait(NULL); // Wait for container until it finishes its job
    getcwd(cwd, sizeof(cwd)); // Getting current working dir
    sprintf(buf, "umount %s/tmp_mnt", cwd);
    system(buf); // Unmounting block device
    system("rm -r tmp_mnt"); // Removing tmp folder for mount

    printf("Program finished!\n");

    return 0;
}

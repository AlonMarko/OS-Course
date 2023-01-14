#include <iostream>
#include <sys/mount.h>
#include "cstdlib"
#include "cstring"
#include "fstream"
#include <stdio.h>
#include <unistd.h>
#include "sys/types.h"
#include <sys/wait.h>
#include <sys/stat.h>
#include <sched.h>
#include "cerrno"
#include "signal.h"

#define STACK 8192
#define ZERO 0
#define CGROUP_SYS "/sys"
#define CGROUP_FS "/sys/fs"
#define CGROUP_CGROUP "/sys/fs/cgroup"
#define CGROUP_PIDS "/sys/fs/cgroup/pids"
#define CGROUP_PIDSMAX "/sys/fs/cgroup/pids/pids.max"
#define CGROUP_PROCS "/sys/fs/cgroup/pids/cgroup.procs"
#define CGROUP_NOTIFY "/sys/fs/cgroup/pids/notify_on_release"


void make_directory_pids() {
    struct stat sb;
    if(!(stat(CGROUP_SYS, &sb) == 0 && S_ISDIR(sb.st_mode))) {
        if (mkdir(CGROUP_SYS, 0755) < ZERO) {
            printf("System Error: Error on creating /sys");
            exit(EXIT_FAILURE);
        }
    }
    if (mkdir(CGROUP_FS, 0755) < ZERO) {
        printf("System Error: Error on creating /sys/fs");
        exit(EXIT_FAILURE);
    }
    if (mkdir(CGROUP_CGROUP, 0755) < ZERO) {
        printf("System Error: Error on creating /sys/fs/cgroup");
        exit(EXIT_FAILURE);
    }
    if (mkdir(CGROUP_PIDS, 0755) < ZERO) {
        printf("System Error: Error on creating /sys/fs/cgroup/pids");
        exit(EXIT_FAILURE);
    }
}

void create_and_write_files(char *max_procs) {
    FILE *file_pointer;
    file_pointer = fopen(CGROUP_PROCS, "w");
    if (file_pointer == nullptr) {
        printf("System Error: Error on creating cgroup_procs.");
        exit(EXIT_FAILURE);
    }
    pid_t pid = getpid();
    fprintf(file_pointer, "%d", pid);
    fclose(file_pointer);
    file_pointer = fopen(CGROUP_PIDSMAX, "w");
    if (file_pointer == nullptr) {
        printf("System Error: Error on creating a pids.max.");
        exit(EXIT_FAILURE);
    }
    fprintf(file_pointer, "%s", max_procs);
    fclose(file_pointer);
    file_pointer = fopen(CGROUP_NOTIFY, "w");
    if (file_pointer == nullptr) {
        printf("System Error: Error on creating notify_on_release.");
        exit(EXIT_FAILURE);
    }
    fprintf(file_pointer, "%d", 1);
    fclose(file_pointer);
}

int container_root(void *arguments) {
    char **argv = (char **) arguments;
    if (sethostname(argv[1], strlen(argv[1])) < ZERO) { //sets the host name.
        printf("System Error: Error on setting host name");
        exit(EXIT_FAILURE);
    }
    if (chroot(argv[2]) < ZERO) {
        printf("System Error: Error on changing the root");
        exit(EXIT_FAILURE);
    }
    if (chdir("/") < ZERO) {
        printf("System Error: Error on changing the  working directory");
        exit(EXIT_FAILURE);
    }
    make_directory_pids();
    create_and_write_files(argv[3]);
    if (mount("proc", "/proc", "proc", 0, 0) < ZERO) {
        printf("System Error: Error using mount");
        exit(EXIT_FAILURE);
    }
    if (execvp(argv[4], argv + 4) < ZERO) {
        printf("System Error: Error on execvp");
        exit(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}

// allocated memory for the stack!
void *stack_mem() {
    auto *stack = malloc(STACK);
    if (stack == nullptr) {
        printf("System Error: Error on allocating the stack for the container");
        exit(EXIT_FAILURE);
    }
    return stack;
}


int main(int argc, char *argv[]) {
    void *stack = stack_mem();
    if (clone(container_root, (char *) stack + STACK, CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS | SIGCHLD,
              (void *) argv) <
        ZERO) {
        printf("System Error: Error on cloning");
        exit(EXIT_FAILURE);
    }
    wait(nullptr);
    std::string remove_all("rm -rf ");
    remove_all.append(argv[2]);
    remove_all.append("/sys/*");
    if (system(remove_all.c_str()) < ZERO) {
        printf("System Error: Error upon removing /sys/*");
        exit(EXIT_FAILURE);
    }
    std::string u_mount(argv[2]);
    u_mount.append("/proc");
    if (umount(u_mount.c_str()) < ZERO) {
        printf("System Error: Error upon umount");
        exit(EXIT_FAILURE);
    }
    free(stack);
    return EXIT_SUCCESS;
}

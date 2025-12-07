#include "cpu.h"
#include "timer.h"
#include "sched.h"
#include "loader.h"
#include "mm.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

static int time_slot;
static int num_cpus;
static int done = 0;
static struct krnl_t os;

#ifdef MM_PAGING
static int memramsz;
static int memswpsz[PAGING_MAX_MMSWP];

struct mmpaging_ld_args {
    /* A dispatched argument struct to compact many-fields passing to loader */
    int vmemsz;
    struct memphy_struct *mram;
    struct memphy_struct **mswp;
    struct memphy_struct *active_mswp;
    int active_mswp_id;
    struct timer_id_t  *timer_id;
};
#endif

static struct ld_args {
    char ** path;
    unsigned long * start_time;
#ifdef MLQ_SCHED
    unsigned long * prio;
#endif
} ld_processes;

int num_processes;

struct cpu_args {
    struct timer_id_t * timer_id;
    int id;
};

/*----------------------------------------------------------
 * CPU routine
 *---------------------------------------------------------*/
static void * cpu_routine(void * args) {
    struct timer_id_t * timer_id = ((struct cpu_args*)args)->timer_id;
    int hw_id = ((struct cpu_args*)args)->id;
    int id = hw_id;
    
    if (num_cpus == 2) {
        id = 1 - hw_id;   // 0 -> 1, 1 -> 0
    }

    int time_left = 0;
    struct pcb_t * proc = NULL;

    while (1) {
        next_slot(timer_id);

        if (proc && proc->pc == proc->code->size) {
            printf("\tCPU %d: Processed %2d has finished\n", id, proc->pid);
            free(proc);
            proc = NULL;
            time_left = 0;
        }

        if (proc && time_left == 0) {
            printf("\tCPU %d: Put process %2d to run queue\n", id, proc->pid);
            put_proc(proc);
            proc = NULL;
        }

        if (!proc) {
            proc = get_proc();
            if (proc) {
                printf("\tCPU %d: Dispatched process %2d\n", id, proc->pid);
                time_left = time_slot;
            }
        }

        if (!proc && done) {
            printf("\tCPU %d stopped\n", id);
            break;
        }

        if (proc) {
            run(proc);
            time_left--;
        }
    }

    detach_event(timer_id);
    pthread_exit(NULL);
}

/*----------------------------------------------------------
 * Loader routine
 *---------------------------------------------------------*/
static void * ld_routine(void * args) {
#ifdef MM_PAGING
    struct mmpaging_ld_args *pargs = (struct mmpaging_ld_args *)args;
    struct timer_id_t      *timer_id     = pargs->timer_id;
    struct memphy_struct   *mram         = pargs->mram;
    struct memphy_struct  **mswp         = pargs->mswp;
    struct memphy_struct   *active_mswp  = pargs->active_mswp;
#else
    struct timer_id_t * timer_id = (struct timer_id_t*)args;
#endif

    int i = 0;
    printf("ld_routine\n");

    while (i < num_processes) {
        /* Chờ đến đúng start_time của process thứ i */
        while (current_time() < ld_processes.start_time[i]) {
            next_slot(timer_id);
        }

        /* Load process từ file mô tả */
        struct pcb_t * proc = load(ld_processes.path[i]);
        struct krnl_t * krnl = proc->krnl = &os;

#ifdef MLQ_SCHED
        proc->prio = ld_processes.prio[i];
#endif

#ifdef MM_PAGING
        krnl->mm = malloc(sizeof(struct mm_struct));
        init_mm(krnl->mm, proc);

        krnl->mram           = mram;
        krnl->mswp           = mswp;
        krnl->active_mswp    = active_mswp;
        krnl->active_mswp_id = 0;
#endif

#ifdef MLQ_SCHED
        printf("\tLoaded a process at %s, PID: %d PRIO: %lu\n",
               ld_processes.path[i], proc->pid, ld_processes.prio[i]);
#else
        printf("\tLoaded a process at %s, PID: %d\n",
               ld_processes.path[i], proc->pid);
#endif

        add_proc(proc);
        free(ld_processes.path[i]);
        i++;
    }

    free(ld_processes.path);
    free(ld_processes.start_time);
#ifdef MLQ_SCHED
    free(ld_processes.prio);
#endif

    done = 1;
    detach_event(timer_id);
    pthread_exit(NULL);
}

/*----------------------------------------------------------
 * Read configuration
 *---------------------------------------------------------*/
static void read_config(const char * path) {
    FILE * file;
    if ((file = fopen(path, "r")) == NULL) {
        printf("Cannot find configure file at %s\n", path);
        exit(1);
    }


    if (fscanf(file, "%d %d %d", &time_slot, &num_cpus, &num_processes) != 3) {
        fprintf(stderr, "Cannot read header line in %s\n", path);
        exit(1);
    }

#ifdef MM_PAGING

    if (fscanf(file, "%d %d %d %d %d",
               &memramsz,
               &memswpsz[0],
               &memswpsz[1],
               &memswpsz[2],
               &memswpsz[3]) != 5) {
        fprintf(stderr, "Cannot read memory config line in %s\n", path);
        exit(1);
    }
#endif

    ld_processes.path = (char**)malloc(sizeof(char*) * num_processes);
    ld_processes.start_time = (unsigned long*)
        malloc(sizeof(unsigned long) * num_processes);
#ifdef MLQ_SCHED
    ld_processes.prio = (unsigned long*)
        malloc(sizeof(unsigned long) * num_processes);
#endif

    int i;
    for (i = 0; i < num_processes; i++) {
        char proc[100] = {0};

#ifdef MLQ_SCHED
        int nread = fscanf(file, "%lu %99s %lu",
                           &ld_processes.start_time[i],
                           proc,
                           &ld_processes.prio[i]);
        if (nread != 3) {
            fprintf(stderr,
                    "Config error at process %d (dong %d trong file %s), nread=%d\n",
                    i, i + 2, path, nread);
            exit(1);
        }
#else
        int nread = fscanf(file, "%lu %99s",
                           &ld_processes.start_time[i],
                           proc);
        if (nread != 2) {
            fprintf(stderr,
                    "Config error at process %d (dong %d trong file %s), nread=%d\n",
                    i, i + 2, path, nread);
            exit(1);
        }
#endif

        size_t len = strlen("input/proc/") + strlen(proc) + 1;
        ld_processes.path[i] = (char*)malloc(len);
        snprintf(ld_processes.path[i], len, "input/proc/%s", proc);
    }

    fclose(file);
}

/*----------------------------------------------------------
 * main
 *---------------------------------------------------------*/
int main(int argc, char * argv[]) {
    /* Read config */
    if (argc != 2) {
        printf("Usage: os [path to configure file]\n");
        return 1;
    }

    char path[100];
    path[0] = '\0';
    strcat(path, "input/");
    strcat(path, argv[1]);
    read_config(path);

    pthread_t * cpu = (pthread_t*)malloc(num_cpus * sizeof(pthread_t));
    struct cpu_args * args =
        (struct cpu_args*)malloc(sizeof(struct cpu_args) * num_cpus);
    pthread_t ld;

    /* Init timer */
    int i;
    for (i = 0; i < num_cpus; i++) {
        args[i].timer_id = attach_event();
        args[i].id = i;
    }
    struct timer_id_t * ld_event = attach_event();
    start_timer();

#ifdef MM_PAGING
    /* Init all MEMPHY include 1 MEMRAM and n of MEMSWP */
    int rdmflag = 1; /* By default memphy is RANDOM ACCESS MEMORY */

    struct memphy_struct mram;
    struct memphy_struct mswp[PAGING_MAX_MMSWP];

    /* Create MEM RAM */
    init_memphy(&mram, memramsz, rdmflag);

    /* Create all MEM SWAP */
    int sit;
    for (sit = 0; sit < PAGING_MAX_MMSWP; sit++)
        init_memphy(&mswp[sit], memswpsz[sit], rdmflag);

    /* In Paging mode, it needs passing the system mem to each PCB through loader*/
    struct mmpaging_ld_args *mm_ld_args =
        malloc(sizeof(struct mmpaging_ld_args));

    mm_ld_args->timer_id       = ld_event;
    mm_ld_args->mram           = (struct memphy_struct *) &mram;
    mm_ld_args->mswp           = (struct memphy_struct**) &mswp;
    mm_ld_args->active_mswp    = (struct memphy_struct *) &mswp[0];
    mm_ld_args->active_mswp_id = 0;
#endif

    /* Init scheduler */
    init_scheduler();

    /* Run CPU and loader */
#ifdef MM_PAGING
    pthread_create(&ld, NULL, ld_routine, (void*)mm_ld_args);
#else
    pthread_create(&ld, NULL, ld_routine, (void*)ld_event);
#endif

    for (i = 0; i < num_cpus; i++) {
        pthread_create(&cpu[i], NULL,
                       cpu_routine, (void*)&args[i]);
    }

    /* Wait for CPU and loader finishing */
    for (i = 0; i < num_cpus; i++) {
        pthread_join(cpu[i], NULL);
    }
    pthread_join(ld, NULL);

    /* Stop timer */
    stop_timer();

    return 0;
}


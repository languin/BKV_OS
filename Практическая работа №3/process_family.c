#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_CHILDREN 10000

/*
 * У каждого процесса есть свой параметр N (current_n).
 * По нему строится дерево: процесс с N создает N детей,
 * а каждому ребенку передает N-1.
 */
static int current_n = 0;

/*
 * В этом массиве хранятся PID непосредственных детей текущего процесса.
 * Массив нужен для рассылки сигналов детям.
 */
static pid_t children[MAX_CHILDREN];
static int children_count = 0;

/* Флаги запросов от обработчиков сигналов. */
static volatile sig_atomic_t need_alrm = 0;
static volatile sig_atomic_t need_usr1 = 0;
static volatile sig_atomic_t need_usr2 = 0;

static void add_child(pid_t pid) {
    if (children_count < MAX_CHILDREN) {
        children[children_count++] = pid;
    }
}

static void send_signal_to_children(int sig) {
    int i;
    for (i = 0; i < children_count; i++) {
        if (children[i] > 0) {
            kill(children[i], sig);
        }
    }
}

static void print_self_and_children(void) {
    int i;

    printf("pid=%d children:", (int)getpid());
    for (i = 0; i < children_count; i++) {
        printf(" %d", (int)children[i]);
    }
    printf("\n");
    fflush(stdout);
}

static void create_initial_tree(void);

/*
 * Создание одного нового потомка с кодом child.
 * Новый потомок получает параметр N-1.
 */
static void create_one_child(void) {
    pid_t pid;

    if (current_n <= 0) {
        return;
    }

    pid = fork();
    if (pid < 0) {
        perror("fork");
        return;
    }

    if (pid == 0) {
        /* Код нового child-процесса. */
        children_count = 0;
        current_n = current_n - 1;
        create_initial_tree();

        for (;;) {
            sleep(1);
            if (need_alrm) {
                need_alrm = 0;
                print_self_and_children();
                send_signal_to_children(SIGALRM);
            }
            if (need_usr2) {
                need_usr2 = 0;
                create_one_child();
            }
            if (need_usr1) {
                need_usr1 = 0;
                send_signal_to_children(SIGUSR1);
                _exit(0);
            }
        }
    }

    add_child(pid);
}

/*
 * Рекурсивное построение дерева процессов:
 * процесс с current_n создает current_n детей,
 * а дети работают с current_n-1.
 */
static void create_initial_tree(void) {
    int i;
    int to_create = current_n;

    if (to_create <= 0) {
        return;
    }

    for (i = 0; i < to_create; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            continue;
        }

        if (pid == 0) {
            children_count = 0;
            current_n = current_n - 1;
            create_initial_tree();

            for (;;) {
                sleep(1);
                if (need_alrm) {
                    need_alrm = 0;
                    print_self_and_children();
                    send_signal_to_children(SIGALRM);
                }
                if (need_usr2) {
                    need_usr2 = 0;
                    create_one_child();
                }
                if (need_usr1) {
                    need_usr1 = 0;
                    send_signal_to_children(SIGUSR1);
                    _exit(0);
                }
            }
        }

        add_child(pid);
    }
}

static void signal_handler(int sig) {
    if (sig == SIGALRM) {
        need_alrm = 1;
    } else if (sig == SIGUSR1) {
        need_usr1 = 1;
    } else if (sig == SIGUSR2) {
        need_usr2 = 1;
    }
}

static int parse_n(const char *s, int *out) {
    char *end = NULL;
    long value;

    errno = 0;
    value = strtol(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0') {
        return -1;
    }
    if (value < 0 || value > 1000) {
        return -1;
    }

    *out = (int)value;
    return 0;
}

int main(int argc, char *argv[]) {
    struct sigaction sa;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s N\n", argv[0]);
        return 1;
    }

    if (parse_n(argv[1], &current_n) != 0) {
        fprintf(stderr, "Invalid N. Use integer from 0 to 1000.\n");
        return 1;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sigaction(SIGALRM, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);

    /* Чтобы не накапливать зомби после завершения потомков. */
    signal(SIGCHLD, SIG_IGN);

    create_initial_tree();

    /*
     * Родитель остается живым и тоже принимает сигналы.
     * По SIGALRM может вывести себя и своих детей.
     */
    for (;;) {
        pause();

        if (need_alrm) {
            need_alrm = 0;
            print_self_and_children();
            send_signal_to_children(SIGALRM);
        }

        if (need_usr2) {
            need_usr2 = 0;
            create_one_child();
        }

        if (need_usr1) {
            need_usr1 = 0;
            send_signal_to_children(SIGUSR1);
            _exit(0);
        }
    }
}

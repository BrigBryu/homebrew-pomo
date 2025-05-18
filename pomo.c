// pomo.c — Pomodoro timer with ANSI true-color & XDG config persistence

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>
#include <sys/stat.h>
#include <signal.h>

#define BAR_LEN 40

static void usage(const char *prog) {
    printf("Mostly just use pomo start and pomo break\n");
    printf("Then if needed do pomo end\n");
    printf("You can also run with these flags\n");
    printf("pomo -p INT         Pomodoro duration in minutes (override)\n");
    printf("pomo -b INT         Break duration in minutes (override)\n");
    printf("pomo -setp INT      Set & persist default Pomodoro duration\n");
    printf("pomo -setb INT      Set & persist default Break duration\n");
    printf("pomo -c1 #RRGGBB    Color for letters/words (default #FFFFFF)\n");
    printf("pomo -c2 #RRGGBB    Color for digits/punctuation (default #00CCFF)\n");
    printf("pomo -h, --help     Show this help and exit\n");
    exit(0);
}

static int parse_hex(const char *hex) {
    char buf[3] = { hex[0], hex[1], '\0' };
    return strtol(buf, NULL, 16);
}

static void rgb_from_hex(const char *hex, int *r, int *g, int *b) {
    if (*hex == '#') ++hex;
    *r = parse_hex(hex + 0);
    *g = parse_hex(hex + 2);
    *b = parse_hex(hex + 4);
}

static void set_fg(int r, int g, int b) {
    printf("\033[38;2;%d;%d;%dm", r, g, b);
}
static void clear_line()  { printf("\033[2K"); }
static void hide_cursor() { printf("\033[?25l"); }
static void show_cursor() { printf("\033[?25h"); }

int main(int argc, char *argv[]) {
    int pomo_min  = 25;
    int break_min = 5;
    char color1[8] = "#FFFFFF";
    char color2[8] = "#00CCFF";
    int c1_set = 0, c2_set = 0, p_set = 0, b_set = 0;
    int p_flag = 0, b_flag = 0;
    const char *cmd = NULL;

    const char *xdg = getenv("XDG_CONFIG_HOME");
    char cfg_file[PATH_MAX];
    if (xdg && *xdg)
        snprintf(cfg_file, PATH_MAX, "%s/pomo/config", xdg);
    else
        snprintf(cfg_file, PATH_MAX, "%s/.config/pomo/config", getenv("HOME"));

    char cfg_dir[PATH_MAX];
    strncpy(cfg_dir, cfg_file, PATH_MAX);
    char *slash = strrchr(cfg_dir, '/');
    if (slash) *slash = '\0';

    char pid_file[PATH_MAX];
    snprintf(pid_file, PATH_MAX, "%s/pid", cfg_dir);

    FILE *f = fopen(cfg_file, "r");
    if (f) {
        char line[64];
        while (fgets(line, sizeof(line), f)) {
            if (!strncmp(line, "COLOR1=", 7)) {
                strncpy(color1, line + 7, 7);
                color1[7] = '\0';
            } else if (!strncmp(line, "COLOR2=", 7)) {
                strncpy(color2, line + 7, 7);
                color2[7] = '\0';
            } else if (!strncmp(line, "POMO_MIN=", 9)) {
                pomo_min = atoi(line + 9);
            } else if (!strncmp(line, "BREAK_MIN=", 10)) {
                break_min = atoi(line + 10);
            }
        }
        fclose(f);
    }

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage(argv[0]);
        } else if (!strcmp(argv[i], "-p") && i+1 < argc) {
            pomo_min = atoi(argv[++i]);
            p_flag = 1;
        } else if (!strcmp(argv[i], "-b") && i+1 < argc) {
            break_min = atoi(argv[++i]);
            b_flag = 1;
        } else if (!strcmp(argv[i], "-setp") && i+1 < argc) {
            pomo_min = atoi(argv[++i]);
            p_set = 1;
        } else if (!strcmp(argv[i], "-setb") && i+1 < argc) {
            break_min = atoi(argv[++i]);
            b_set = 1;
        } else if (!strcmp(argv[i], "-c1") && i+1 < argc) {
            strncpy(color1, argv[++i], 7);
            color1[7] = '\0';
            c1_set = 1;
        } else if (!strcmp(argv[i], "-c2") && i+1 < argc) {
            strncpy(color2, argv[++i], 7);
            color2[7] = '\0';
            c2_set = 1;
        } else if (!strcmp(argv[i], "start") ||
                   !strcmp(argv[i], "break") ||
                   !strcmp(argv[i], "end")) {
            cmd = argv[i];
        } else {
            fprintf(stderr, "Unknown arg: %s\nTry '%s --help'\n",
                    argv[i], argv[0]);
            return 1;
        }
    }

    if (c1_set || c2_set || p_set || b_set) {
        mkdir(cfg_dir, 0755);
        FILE *wf = fopen(cfg_file, "w");
        if (wf) {
            fprintf(wf, "COLOR1=%s\n", color1);
            fprintf(wf, "COLOR2=%s\n", color2);
            fprintf(wf, "POMO_MIN=%d\n", pomo_min);
            fprintf(wf, "BREAK_MIN=%d\n", break_min);
            fclose(wf);
        }
        if (c1_set)
            printf("Color1 set to %s\n", color1);
        if (c2_set)
            printf("Color2 set to %s\n", color2);
        if (p_set)
            printf("Pomodoro default time set to %d\n", pomo_min);
        if (b_set)
            printf("Break default time set to %d\n", break_min);
        return 0;
    }

    if (p_flag && !cmd) cmd = "start";
    if (b_flag && !cmd) cmd = "break";

    if (cmd && !strcmp(cmd, "end")) {
        FILE *pf = fopen(pid_file, "r");
        if (!pf) {
            fprintf(stderr, "No running session found\n");
            return 1;
        }
        pid_t pid;
        if (fscanf(pf, "%d", &pid) == 1)
            kill(pid, SIGTERM);
        fclose(pf);
        remove(pid_file);
        printf("Stopped pomo (%d)\n", pid);
        return 0;
    }

    if (!cmd)
        usage(argv[0]);

    mkdir(cfg_dir, 0755);
    FILE *pf = fopen(pid_file, "w");
    if (pf) {
        fprintf(pf, "%d\n", getpid());
        fclose(pf);
    }

    int duration = (!strcmp(cmd, "break")) ? break_min : pomo_min;
    int total_s = duration * 60;

    int r1, g1, b1; 
    int r2, g2, b2;
    rgb_from_hex(color1, &r1, &g1, &b1);
    rgb_from_hex(color2, &r2, &g2, &b2);

    time_t now = time(NULL);
    char start_buf[6], end_buf[6];
    strftime(start_buf, sizeof(start_buf), "%H:%M", localtime(&now));
    strftime(end_buf, sizeof(end_buf), "%H:%M",
             localtime(&(time_t){ now + total_s }));

    hide_cursor();
    printf("\033c");
    set_fg(r1, g1, b1);
    printf("%s: ", strcmp(cmd, "break") == 0 ? "Break" : "Pomodoro");
    set_fg(r2, g2, b2);
    printf("%s - %s\n", start_buf, end_buf);
    fflush(stdout);

    for (int rem = total_s; rem >= 0; --rem) {
        int mins = rem / 60, secs = rem % 60;
        char line2[64];
        snprintf(line2, sizeof(line2), "%d minute(s) - %dm%ds",
                 duration, mins, secs);

        clear_line();
        for (char *p = line2; *p; ++p) {
            if (isdigit((unsigned char)*p)) set_fg(r2, g2, b2);
            else                             set_fg(r1, g1, b1);
            putchar(*p);
        }
        putchar('\n');

        clear_line();
        int filled = ((total_s - rem) * BAR_LEN) / total_s;
        int empty  = BAR_LEN - filled;
        set_fg(r2, g2, b2);
        for (int i = 0; i < filled; ++i) fputs("█", stdout);
        set_fg(r1, g1, b1);
        for (int i = 0; i < empty;  ++i) fputs("░", stdout);
        putchar('\n');

        fflush(stdout);
        if (rem > 0) {
            printf("\033[2A");
            sleep(1);
        }
    }

    show_cursor();
    remove(pid_file);
    system("terminal-notifier -title 'Pomodoro' -message 'Done!' -sound default &");
    system("afplay /System/Library/Sounds/Ping.aiff &");
    return 0;
}


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

#define BAR_LEN 40

// Parse a two-hex-digit string into an int
static int parse_hex(const char *hex) {
    char buf[3] = { hex[0], hex[1], '\0' };
    return (int)strtol(buf, NULL, 16);
}

// Convert "#RRGGBB" or "RRGGBB" → r,g,b
static void rgb_from_hex(const char *hex, int *r, int *g, int *b) {
    if (*hex == '#') ++hex;
    *r = parse_hex(hex + 0);
    *g = parse_hex(hex + 2);
    *b = parse_hex(hex + 4);
}

// Emit 24-bit ANSI foreground
static void set_fg(int r, int g, int b) {
    printf("\033[38;2;%d;%d;%dm", r, g, b);
}
static void clear_line()  { printf("\033[2K"); }
static void hide_cursor() { printf("\033[?25l"); }
static void show_cursor() { printf("\033[?25h"); }

int main(int argc, char *argv[]) {
    // ─── Defaults ──────────────────────────────────────────
    int pomo_min = 25, break_min = 5;
    char color1[8] = "#FFFFFF";   // letters & words
    char color2[8] = "#00CCFF";   // digits & punctuation
    const char *cmd = "start";
    int c1_set = 0, c2_set = 0;

    // ─── Figure out config path ────────────────────────────
    const char *xdg = getenv("XDG_CONFIG_HOME");
    char cfg_file[PATH_MAX];
    if (xdg && *xdg) {
        snprintf(cfg_file, PATH_MAX, "%s/pomo/config", xdg);
    } else {
        snprintf(cfg_file, PATH_MAX, "%s/.config/pomo/config", getenv("HOME"));
    }

    // ─── Load existing config ──────────────────────────────
    FILE *f = fopen(cfg_file, "r");
    if (f) {
        char line[64];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "COLOR1=", 7) == 0) {
                strncpy(color1, line + 7, 7); color1[7] = '\0';
            } else if (strncmp(line, "COLOR2=", 7) == 0) {
                strncpy(color2, line + 7, 7); color2[7] = '\0';
            }
        }
        fclose(f);
    }

    // ─── Parse CLI ──────────────────────────────────────────
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-p") && i+1<argc) {
            pomo_min = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "-b") && i+1<argc) {
            break_min = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "-c1") && i+1<argc) {
            strncpy(color1, argv[++i], 7); color1[7]='\0'; c1_set=1;
        } else if (!strcmp(argv[i], "-c2") && i+1<argc) {
            strncpy(color2, argv[++i], 7); color2[7]='\0'; c2_set=1;
        } else if (!strcmp(argv[i], "start") || !strcmp(argv[i], "break")) {
            cmd = argv[i];
        } else {
            fprintf(stderr, "Unknown arg: %s\n", argv[i]);
            return 1;
        }
    }

    // ─── Persist new colors if changed ────────────────────
    if (c1_set || c2_set) {
        char cfg_dir[PATH_MAX];
        strncpy(cfg_dir, cfg_file, PATH_MAX);
        char *slash = strrchr(cfg_dir, '/');
        if (slash) *slash = '\0';
        mkdir(cfg_dir, 0755);
        FILE *wf = fopen(cfg_file, "w");
        if (wf) {
            fprintf(wf, "COLOR1=%s\n", color1);
            fprintf(wf, "COLOR2=%s\n", color2);
            fclose(wf);
        }
    }

    // ─── Compute session params ────────────────────────────
    int duration = (!strcmp(cmd, "break")) ? break_min : pomo_min;
    int total_s  = duration * 60;

    int r1,g1,b1, r2,g2,b2;
    rgb_from_hex(color1, &r1, &g1, &b1);
    rgb_from_hex(color2, &r2, &g2, &b2);

    time_t now = time(NULL);
    char start_buf[6], end_buf[6];
    strftime(start_buf, sizeof(start_buf), "%H:%M", localtime(&now));
    strftime(end_buf,   sizeof(end_buf),   "%H:%M", localtime(&(time_t){now+total_s}));

    // ─── Render ────────────────────────────────────────────
    hide_cursor();
    printf("\033c");  // clear screen

    // Line 1: header
    set_fg(r1,g1,b1);
    printf("%s: ", strcmp(cmd,"break")==0 ? "Break" : "Pomodoro");
    set_fg(r2,g2,b2);
    printf("%s - %s\n", start_buf, end_buf);
    fflush(stdout);

    // Lines 2+3: loop
    for (int rem = total_s; rem >= 0; --rem) {
        int mins = rem / 60, secs = rem % 60;
        char line2[64];
        snprintf(line2, sizeof(line2), "%d minute(s) - %dm%ds", duration, mins, secs);

        // Line 2
        clear_line();
        for (char *p = line2; *p; ++p) {
            if (isdigit((unsigned char)*p)) set_fg(r2,g2,b2);
            else                             set_fg(r1,g1,b1);
            putchar(*p);
        }
        putchar('\n');

        // Line 3: progress bar
        clear_line();
        int filled = ((total_s - rem) * BAR_LEN) / total_s;
        int empty  = BAR_LEN - filled;
        set_fg(r2,g2,b2);
        for (int i = 0; i < filled; ++i) fputs("█", stdout);
        set_fg(r1,g1,b1);
        for (int i = 0; i < empty;  ++i) fputs("░", stdout);
        putchar('\n');

        fflush(stdout);
        if (rem > 0) {
            printf("\033[2A"); // move up 2 lines
            sleep(1);
        }
    }

    show_cursor();
    // macOS notification & ding
    system("terminal-notifier -title 'Pomodoro' -message 'Done!' -sound default >/dev/null 2>&1 &");
    system("afplay /System/Library/Sounds/Ping.aiff >/dev/null 2>&1 &");
    return 0;
}

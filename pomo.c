// pomo.c — Pomodoro timer with ANSI true-color & XDG config persistence
//             + save/load/delete/list custom color sets
//             + shared status tracking across windows (full display)

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
#include <dirent.h>

#define BAR_LEN 40

static void usage(const char *prog) {
    printf("Mostly just use pomo start and pomo break\n");
    printf("Then if needed do pomo end\n");
    printf("You can also run with these flags\n");
    printf("  pomo -p INT         Pomodoro duration override\n");
    printf("  pomo -b INT         Break duration override\n");
    printf("  pomo -setp INT      Set & persist default Pomodoro duration\n");
    printf("  pomo -setb INT      Set & persist default Break duration\n");
    printf("  pomo -c1 #RRGGBB    Color for letters/words\n");
    printf("  pomo -c2 #RRGGBB    Color for digits/punctuation\n");
    printf("  pomo -savec NAME    Save current colors under NAME\n");
    printf("  pomo -loadc NAME    Load colors from NAME and persist\n");
    printf("  pomo -deletec NAME  Delete saved colors NAME\n");
    printf("  pomo -listc         List all saved color sets\n");
    printf("  pomo -track         Track and share full display status\n");
    printf("  pomo -status        Print shared status display\n");
    printf("  pomo end            Stop a running session early\n");
    printf("  pomo -h, --help     Show this help and exit\n");
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

int main(int argc, char *argv[]) {
    // Defaults
    int pomo_min = 25, break_min = 5;
    char color1[8] = "#FFFFFF", color2[8] = "#00CCFF";
    int c1_set=0, c2_set=0, p_set=0, b_set=0;
    int p_flag=0, b_flag=0;
    int track_mode=0, status_mode=0;
    char *cmd=NULL, *arg=NULL;

    // Config paths
    const char *xdg = getenv("XDG_CONFIG_HOME");
    char cfg_file[PATH_MAX];
    if (xdg && *xdg)
        snprintf(cfg_file, sizeof cfg_file, "%s/pomo/config", xdg);
    else
        snprintf(cfg_file, sizeof cfg_file, "%s/.config/pomo/config", getenv("HOME"));

    char cfg_dir[PATH_MAX];
    strncpy(cfg_dir, cfg_file, sizeof(cfg_dir));
    char *slash = strrchr(cfg_dir, '/');
    if (slash) *slash = '\0';

    char pid_file[PATH_MAX];
    snprintf(pid_file, sizeof pid_file, "%s/pid", cfg_dir);

    char status_file[PATH_MAX];
    snprintf(status_file, sizeof status_file, "%s/status", cfg_dir);

    char colors_dir[PATH_MAX];
    snprintf(colors_dir, sizeof colors_dir, "%s/colors", cfg_dir);

    // Load existing config
    FILE *f = fopen(cfg_file, "r");
    if (f) {
        char line[64];
        while (fgets(line, sizeof line, f)) {
            if (!strncmp(line, "COLOR1=", 7))    { strncpy(color1, line+7, 7);  color1[7]='\0'; }
            else if (!strncmp(line, "COLOR2=", 7)) { strncpy(color2, line+7, 7);  color2[7]='\0'; }
            else if (!strncmp(line, "POMO_MIN=", 9))    pomo_min  = atoi(line+9);
            else if (!strncmp(line, "BREAK_MIN=",10))    break_min = atoi(line+10);
        }
        fclose(f);
    }

    // Parse CLI
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage(argv[0]);
        } else if (!strcmp(argv[i], "-p") && i+1 < argc) {
            pomo_min = atoi(argv[++i]); p_flag = 1;
        } else if (!strcmp(argv[i], "-b") && i+1 < argc) {
            break_min = atoi(argv[++i]); b_flag = 1;
        } else if (!strcmp(argv[i], "-setp") && i+1 < argc) {
            pomo_min = atoi(argv[++i]); p_set = 1;
        } else if (!strcmp(argv[i], "-setb") && i+1 < argc) {
            break_min = atoi(argv[++i]); b_set = 1;
        } else if (!strcmp(argv[i], "-c1") && i+1 < argc) {
            strncpy(color1, argv[++i], 7); color1[7]='\0'; c1_set = 1;
        } else if (!strcmp(argv[i], "-c2") && i+1 < argc) {
            strncpy(color2, argv[++i], 7); color2[7]='\0'; c2_set = 1;
        } else if (!strcmp(argv[i], "-savec") && i+1 < argc) {
            cmd = "savec"; arg = argv[++i];
        } else if (!strcmp(argv[i], "-loadc") && i+1 < argc) {
            cmd = "loadc"; arg = argv[++i];
        } else if (!strcmp(argv[i], "-deletec") && i+1 < argc) {
            cmd = "deletec"; arg = argv[++i];
        } else if (!strcmp(argv[i], "-listc")) {
            cmd = "listc";
        } else if (!strcmp(argv[i], "-track")) {
            track_mode = 1;
        } else if (!strcmp(argv[i], "-status")) {
            status_mode = 1;
        } else if (!strcmp(argv[i], "start") ||
                   !strcmp(argv[i], "break") ||
                   !strcmp(argv[i], "end")) {
            cmd = argv[i];
        } else {
            fprintf(stderr, "Unknown arg: %s\nTry '%s --help'\n", argv[i], argv[0]);
            return 1;
        }
    }

    // Status mode: just cat status file (which has full ANSI-colored display)
    if (status_mode) {
        FILE *sf = fopen(status_file, "r");
        if (!sf) {
            fprintf(stderr, "No active timer\n");
            return 1;
        }
        int ch;
        while ((ch = fgetc(sf)) != EOF) putchar(ch);
        fclose(sf);
        return 0;
    }

    // Save/load/delete/list color sets
    if (cmd && (!strcmp(cmd, "savec") || !strcmp(cmd, "loadc")
             || !strcmp(cmd, "deletec") || !strcmp(cmd, "listc"))) {
        mkdir(colors_dir, 0755);
        if (!strcmp(cmd, "savec")) {
            char path[PATH_MAX];
            snprintf(path, sizeof path, "%s/%s", colors_dir, arg);
            FILE *cf = fopen(path, "w");
            if (cf) { fprintf(cf, "%s\n%s\n", color1, color2); fclose(cf);
                printf("Saved %s: color1=%s color2=%s\n", arg, color1, color2);
            }
        } else if (!strcmp(cmd, "loadc")) {
            char path[PATH_MAX], c1[8], c2[8];
            snprintf(path, sizeof path, "%s/%s", colors_dir, arg);
            FILE *cf = fopen(path, "r");
            if (cf && fscanf(cf, "%7s %7s", c1, c2)==2) {
                strncpy(color1, c1,7); color1[7]='\0';
                strncpy(color2, c2,7); color2[7]='\0';
                fclose(cf);
                printf("Loaded %s: color1=%s color2=%s\n", arg, color1, color2);
                mkdir(cfg_dir,0755);
                FILE *wf = fopen(cfg_file, "w");
                if (wf) {
                    fprintf(wf, "COLOR1=%s\nCOLOR2=%s\nPOMO_MIN=%d\nBREAK_MIN=%d\n",
                            color1, color2, pomo_min, break_min);
                    fclose(wf);
                }
            }
        } else if (!strcmp(cmd, "deletec")) {
            char path[PATH_MAX];
            snprintf(path, sizeof path, "%s/%s", colors_dir, arg);
            if (!remove(path)) printf("Deleted '%s'\n", arg);
        } else { // listc
            DIR *d = opendir(colors_dir);
            struct dirent *e;
            while ((e = readdir(d))) {
                if (e->d_name[0]=='.') continue;
                char path[PATH_MAX], c1[8], c2[8];
                snprintf(path, sizeof path, "%s/%s", colors_dir, e->d_name);
                FILE *cf = fopen(path, "r");
                if (cf && fscanf(cf, "%7s %7s", c1, c2)==2) {
                    printf("%s: color1=%s color2=%s\n", e->d_name, c1, c2);
                    fclose(cf);
                }
            }
            closedir(d);
        }
        return 0;
    }

    // Persist config changes
    if (c1_set||c2_set||p_set||b_set) {
        mkdir(cfg_dir,0755);
        FILE *wf = fopen(cfg_file, "w");
        if (wf) {
            fprintf(wf, "COLOR1=%s\nCOLOR2=%s\nPOMO_MIN=%d\nBREAK_MIN=%d\n",
                    color1, color2, pomo_min, break_min);
            fclose(wf);
        }
        if (c1_set) printf("Color1 set to %s\n", color1);
        if (c2_set) printf("Color2 set to %s\n", color2);
        if (p_set)  printf("Pomodoro default set to %d\n", pomo_min);
        if (b_set)  printf("Break default set to %d\n", break_min);
        return 0;
    }

    // Flags map
    if (p_flag && !cmd) cmd = "start";
    if (b_flag && !cmd) cmd = "break";

    // Handle early end
    if (cmd && !strcmp(cmd, "end")) {
        FILE *pf = fopen(pid_file, "r");
        if (!pf) { fprintf(stderr, "No running session\n"); return 1; }
        pid_t pid; if (fscanf(pf, "%d", &pid)==1) kill(pid, SIGTERM);
        fclose(pf); remove(pid_file); remove(status_file);
        printf("Stopped pomo (%d)\n", pid);
        return 0;
    }

    // No command → help
    if (!cmd) usage(argv[0]);

    // Record our pid
    mkdir(cfg_dir,0755);
    FILE *pf = fopen(pid_file, "w");
    if (pf) { fprintf(pf, "%d\n", getpid()); fclose(pf); }

    // Prepare timer
    int duration = (!strcmp(cmd, "break")) ? break_min : pomo_min;
    int total_s  = duration * 60;

    // Colors
    int r1,g1,b1, r2,g2,b2;
    rgb_from_hex(color1, &r1, &g1, &b1);
    rgb_from_hex(color2, &r2, &g2, &b2);

    // Compute start/end times
    time_t now = time(NULL);
    struct tm end_tm = *localtime(&now);
    end_tm.tm_sec += total_s;
    mktime(&end_tm);

    char start_buf[6], end_buf[6];
    strftime(start_buf, sizeof start_buf, "%H:%M", localtime(&now));
    strftime(end_buf,   sizeof end_buf,   "%H:%M", &end_tm);

    // Header
    printf("\033c");                      // clear
    printf("\033[38;2;%d;%d;%dm",r1,g1,b1);
    printf("%s: ", strcmp(cmd,"break")==0?"Break":"Pomodoro");
    printf("\033[38;2;%d;%d;%dm",r2,g2,b2);
    printf("%s - %s\n", start_buf, end_buf);

    // Countdown loop
    for (int rem = total_s; rem >= 0; --rem) {
        int m = rem/60, s = rem%60;
        char line2[64];
        snprintf(line2, sizeof line2, "%d minute(s) - %dm%02ds", duration, m, s);

        // Print remaining
        printf("\033[2K");  // clear
        for (char *p = line2; *p; ++p) {
            if (isdigit((unsigned char)*p))
                printf("\033[38;2;%d;%d;%dm", r2, g2, b2);
            else
                printf("\033[38;2;%d;%d;%dm", r1, g1, b1);
            putchar(*p);
        }
        putchar('\n');

        // Progress bar
        printf("\033[2K");
        int filled = ((total_s - rem)*BAR_LEN)/total_s;
        printf("\033[38;2;%d;%d;%dm",r2,g2,b2);
        for (int i=0; i<filled; ++i) fputs("█", stdout);
        printf("\033[38;2;%d;%d;%dm",r1,g1,b1);
        for (int i=filled; i<BAR_LEN; ++i) fputs("░", stdout);
        putchar('\n');

        // Track full display to file
        if (track_mode && rem>0) {
            mkdir(cfg_dir,0755);
            FILE *sf = fopen(status_file,"w");
            if (sf) {
                // header
                fprintf(sf,"\033[38;2;%d;%d;%dm%s: \033[38;2;%d;%d;%dm%s - %s\n",
                        r1,g1,b1,
                        strcmp(cmd,"break")==0?"Break":"Pomodoro",
                        r2,g2,b2, start_buf, end_buf);
                // line2
                for (char *p=line2; *p; ++p) {
                    if (isdigit((unsigned char)*p))
                        fprintf(sf,"\033[38;2;%d:%d:%dm",r2,g2,b2);
                    else
                        fprintf(sf,"\033[38;2;%d:%d:%dm",r1,g1,b1);
                    fputc(*p, sf);
                }
                fputc('\n', sf);
                // bar
                fprintf(sf,"\033[38;2;%d;%d;%dm",r2,g2,b2);
                for (int i=0; i<filled; ++i) fputs("█", sf);
                fprintf(sf,"\033[38;2;%d;%d;%dm",r1,g1,b1);
                for (int i=filled; i<BAR_LEN; ++i) fputs("░", sf);
                fputc('\n', sf);
                fclose(sf);
            }
        }

        if (rem>0) {
            printf("\033[2A");  // move cursor up 2 lines
            sleep(1);
        }
    }

    // Cleanup
    remove(pid_file);
    if (track_mode) remove(status_file);
    system("terminal-notifier -title 'Pomodoro' -message 'Done!' -sound default &");
    system("afplay /System/Library/Sounds/Ping.aiff &");
    return 0;
}

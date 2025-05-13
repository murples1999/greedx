#include <ncurses.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>    // for fabs

#define DEFAULT_ROWS    22
#define DEFAULT_COLS    79
#define KEYBIND_FILE    "greed_keys.txt"
#define CONFIG_FILE     "greed_config.txt"
#define MAX_SCORES      10

typedef struct { int y, x; } Vec2;
typedef struct { int score; float percent; } HighScore;

// defaults
static const short DEFAULT_COLORS_MAP[10] = {
    0,
    COLOR_WHITE, COLOR_YELLOW, COLOR_MAGENTA,
    COLOR_YELLOW, COLOR_GREEN, COLOR_RED,
    COLOR_GREEN, COLOR_RED, COLOR_CYAN
};
static const int DEFAULT_KEYS_MAP[8] = {
    'w','s','a','d','q','e','z','c'
};
static const int DEFAULT_KEY_HINT = 'p';
static const int DEFAULT_KEY_QUIT = 'l';

// globals
int ROWS = DEFAULT_ROWS;
int COLS = DEFAULT_COLS;
short colors[10] = {
    0,
    COLOR_WHITE, COLOR_YELLOW, COLOR_MAGENTA,
    COLOR_YELLOW, COLOR_GREEN, COLOR_RED,
    COLOR_GREEN, COLOR_RED, COLOR_CYAN
};
int **grid;
Vec2 player;
int score = 0, has_moved = 0, show_hint = 0, bad_move_timer = 0;
bool show_help = false;
int key_hint    = DEFAULT_KEY_HINT;
int key_quit    = DEFAULT_KEY_QUIT;
int key_help    = '?';
int key_keybind = '=';
int key_resize  = '-';
int key_color   = '0';
int key_reset   = '|';

Vec2 directions[8] = {
    {-1,0},{1,0},{0,-1},{0,1},
    {-1,-1},{-1,1},{1,-1},{1,1}
};
int keys[8] = { 'w','s','a','d','q','e','z','c' };

// build high-score filename for this board size
static void build_score_filename(char *buf, size_t len) {
    snprintf(buf, len, "greed_scores_%dx%d.txt", ROWS, COLS);
}

// prototypes
void init_colors();
void allocate_grid();
void free_grid();
void draw_grid();
bool has_moves();
void move_player(int ch);

void load_config();
void save_config();
void load_keybinds();
void save_keybinds();

void prompt_resize();
void prompt_color_picker();
void prompt_keybinds();
void prompt_reset_settings();

void load_high_scores(HighScore hs[], int *n);
void save_high_scores(HighScore hs[], int n);
bool update_high_scores(int final_score, float final_percent);

//─── main ─────────────────────────────────────────────────────────────────────
int main(void) {
    srand(time(NULL));
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    curs_set(0);

    load_config();
    load_keybinds();
    init_colors();
    allocate_grid();

    // initial player position
    do {
        player.y = rand() % ROWS;
        player.x = rand() % COLS;
    } while (grid[player.y][player.x] == 0);

    while (1) {
        if (show_help) {
            WINDOW *h = newwin(17, 52, (LINES-17)/2, (COLS-52)/2);
            box(h, 0, 0);
            wattron(h, A_BOLD);
            mvwprintw(h, 1, 2, "GREED - HOW TO PLAY");
            wattroff(h, A_BOLD);
            mvwprintw(h, 3, 2, "Move using:       %c/%c/%c/%c/%c/%c/%c/%c",
                      keys[0],keys[1],keys[2],keys[3],
                      keys[4],keys[5],keys[6],keys[7]);
            mvwprintw(h, 4, 2, "Show hint:        %c", key_hint);
            mvwprintw(h, 5, 2, "Quit game:        %c", key_quit);
            mvwprintw(h, 6, 2, "Resize grid:      %c", key_resize);
            mvwprintw(h, 7, 2, "Color picker:     %c", key_color);
            mvwprintw(h, 8, 2, "Set keybinds:     %c", key_keybind);
            mvwprintw(h, 9, 2, "Reset settings:   %c", key_reset);
            mvwprintw(h,10,2, "Restart run:      <space>");
            mvwprintw(h,11,2, "");
            mvwprintw(h,12,2, "Your goal: clear as many tiles as possible");
            mvwprintw(h,13,2, "Step on a number to move exactly that many tiles.");
            mvwprintw(h,14,2, "Tiles vanish as you go.");
            mvwprintw(h,15,2, "Press any key to return.");
            wrefresh(h);
            getch();
            delwin(h);
            show_help = false;
            continue;
        }

        draw_grid();

        if (bad_move_timer > 0) bad_move_timer--;

        if (!has_moves()) {
            mvprintw(ROWS/2, COLS/2 - 5, "GAME OVER");
            refresh();
            getch();
            break;
        }

        int ch = getch();
        if      (ch == key_hint)   show_hint = !show_hint;
        else if (ch == key_quit)   break;
        else if (ch == key_help)   show_help = true;
        else if (ch == key_keybind)prompt_keybinds();
        else if (ch == key_resize) prompt_resize();
        else if (ch == key_color)  prompt_color_picker();
        else if (ch == key_reset)  prompt_reset_settings();
        else if (ch == ' ') {
            // restart current run
            free_grid();
            allocate_grid();
            score = has_moved = show_hint = bad_move_timer = 0;
            do {
                player.y = rand() % ROWS;
                player.x = rand() % COLS;
            } while (grid[player.y][player.x] == 0);
        }
        else move_player(ch);
    }

    endwin();
    free_grid();

    float pct = 100.0f * score / (ROWS * COLS);
    update_high_scores(score, pct);

    return 0;
}

//─── drawing & movement ─────────────────────────────────────────────────────────
void init_colors() {
    start_color();
    for (int i = 1; i <= 9; i++)
        init_pair(i, colors[i], COLOR_BLACK);
}

void allocate_grid() {
    grid = malloc(ROWS * sizeof(int*));
    for (int y = 0; y < ROWS; y++) {
        grid[y] = malloc(COLS * sizeof(int));
        for (int x = 0; x < COLS; x++)
            grid[y][x] = (rand() % 9) + 1;
    }
}

void free_grid() {
    for (int y = 0; y < ROWS; y++)
        free(grid[y]);
    free(grid);
}

void draw_grid() {
    erase();
    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
            int v = grid[y][x];
            if (y==player.y && x==player.x) {
                attron(A_BOLD | (has_moved?0:A_BLINK));
                mvaddch(y, x, '@');
                attroff(A_BOLD | A_BLINK);
            } else if (v > 0) {
                if (v >= 6) attron(A_BOLD);
                attron(COLOR_PAIR(v));
                mvaddch(y, x, '0' + v);
                attroff(COLOR_PAIR(v));
                if (v >= 6) attroff(A_BOLD);
            }
        }
    }

    if (show_hint) {
        for (int d = 0; d < 8; d++) {
            Vec2 dir = directions[d];
            int fy = player.y + dir.y, fx = player.x + dir.x;
            if (fy<0||fy>=ROWS||fx<0||fx>=COLS||grid[fy][fx]==0) continue;
            int step = grid[fy][fx], ok = 1;
            for (int i = 1; i <= step; i++) {
                int ny = player.y + dir.y*i, nx = player.x + dir.x*i;
                if (ny<0||ny>=ROWS||nx<0||nx>=COLS||grid[ny][nx]==0) {
                    ok=0; break;
                }
            }
            if (ok)
                for (int i = 1; i <= step; i++)
                    mvchgat(player.y+dir.y*i,
                            player.x+dir.x*i,
                            1, A_REVERSE, 0, NULL);
        }
    }

    move(ROWS, 0);
    clrtoeol();
    float pct = 100.0f * score / (ROWS * COLS);
    printw("Score: %d  %.2f%%  (%c=help)", score, pct, key_help);
    if (bad_move_timer>0) printw("  **Bad Move**");
    refresh();
}

bool has_moves() {
    for (int d = 0; d < 8; d++) {
        Vec2 dir = directions[d];
        int fy = player.y + dir.y, fx = player.x + dir.x;
        if (fy<0||fy>=ROWS||fx<0||fx>=COLS||grid[fy][fx]==0) continue;
        int step = grid[fy][fx], ok = 1;
        for (int i = 1; i <= step; i++) {
            int ny = player.y + dir.y*i, nx = player.x + dir.x*i;
            if (ny<0||ny>=ROWS||nx<0||nx>=COLS||grid[ny][nx]==0) {
                ok=0; break;
            }
        }
        if (ok) return true;
    }
    return false;
}

void move_player(int ch) {
    for (int d = 0; d < 8; d++) {
        if (ch == keys[d]) {
            Vec2 dir = directions[d];
            int fy = player.y + dir.y, fx = player.x + dir.x;
            if (fy<0||fy>=ROWS||fx<0||fx>=COLS||grid[fy][fx]==0) {
                bad_move_timer = 1;
                return;
            }
            int step = grid[fy][fx], ok = 1;
            for (int i = 1; i <= step; i++) {
                int ny = player.y + dir.y*i, nx = player.x + dir.x*i;
                if (ny<0||ny>=ROWS||nx<0||nx>=COLS||grid[ny][nx]==0) {
                    ok = 0; break;
                }
            }
            if (!ok) { bad_move_timer=1; return; }
            for (int i = 1; i <= step; i++) {
                int ny = player.y + dir.y*i, nx = player.x + dir.x*i;
                grid[ny][nx] = 0;
                score++;
            }
            player.y += dir.y * step;
            player.x += dir.x * step;
            has_moved = 1;
            return;
        }
    }
    bad_move_timer = 1;
}

//─── config & keybind persistence ───────────────────────────────────────────────
void load_config() {
    FILE *f = fopen(CONFIG_FILE, "r");
    if (!f) return;
    fscanf(f, "%d %d", &ROWS, &COLS);
    for (int i = 1; i <= 9; i++) {
        int t;
        if (fscanf(f, "%d", &t) == 1 && t >= 0 && t <= 7)
            colors[i] = (short)t;
    }
    fclose(f);
}

void save_config() {
    FILE *f = fopen(CONFIG_FILE, "w");
    if (!f) return;
    fprintf(f, "%d %d\n", ROWS, COLS);
    for (int i = 1; i <= 9; i++) fprintf(f, "%d ", colors[i]);
    fprintf(f, "\n");
    fclose(f);
}

void load_keybinds() {
    FILE *f = fopen(KEYBIND_FILE, "r");
    if (!f) return;
    for (int i = 0; i < 8; i++) fscanf(f, "%d", &keys[i]);
    fscanf(f, "%d %d", &key_hint, &key_quit);
    fclose(f);
}

void save_keybinds() {
    FILE *f = fopen(KEYBIND_FILE, "w");
    if (!f) return;
    for (int i = 0; i < 8; i++) fprintf(f, "%d ", keys[i]);
    fprintf(f, "%d %d\n", key_hint, key_quit);
    fclose(f);
}

//─── interactive prompts ───────────────────────────────────────────────────────
void prompt_resize() {
    echo(); curs_set(1);
    int r, c;
    mvprintw(ROWS, 0,
      "Enter new grid size (rows cols) [Default: %d %d]: ",
      DEFAULT_ROWS, DEFAULT_COLS);
    scanw("%d %d", &r, &c);
    if (r >= 10 && c >= 10) {
        free_grid();
        ROWS = r; COLS = c;
        allocate_grid();
        do {
            player.y = rand() % ROWS;
            player.x = rand() % COLS;
        } while (grid[player.y][player.x] == 0);
        save_config();
        // reset run
        score = has_moved = show_hint = bad_move_timer = 0;
    }
    noecho(); curs_set(0);
}

void prompt_color_picker() {
    echo(); curs_set(1);
    mvprintw(ROWS, 0,
      "Color Indexes: 0=BLACK 1=RED 2=GREEN 3=YELLOW 4=BLUE 5=MAGENTA 6=CYAN 7=WHITE");
    for (int i = 1; i <= 9; i++) {
        mvprintw(ROWS + i, 0,
                 "Select color (0-7) for number %d: ", i);
        int c; scanw("%d", &c);
        if (c >= 0 && c <= 7) colors[i] = (short)c;
    }
    init_colors();
    save_config();
    noecho(); curs_set(0);
}

void prompt_keybinds() {
    WINDOW *k = newwin(15, 40, (LINES-15)/2, (COLS-40)/2);
    box(k, 0, 0);
    wattron(k, A_BOLD);
    mvwprintw(k, 1, 2, "SET KEYBINDS");
    wattroff(k, A_BOLD);

    char *labels[10] = {
      "Up:","Down:","Left:","Right:",
      "Up-Left:","Up-Right:","Down-Left:","Down-Right:",
      "Hint:","Quit:"
    };
    int *vars[10] = {
      &keys[0],&keys[1],&keys[2],&keys[3],
      &keys[4],&keys[5],&keys[6],&keys[7],
      &key_hint,&key_quit
    };

    for (int i = 0; i < 10; i++) {
        mvwprintw(k, 3+i, 2, "%s ", labels[i]);
        wrefresh(k);
        int nc = wgetch(k);
        *vars[i] = nc;
        mvwprintw(k, 3+i, 15, "%c", nc);
    }

    mvwprintw(k, 13, 2, "Press any key to return.");
    wrefresh(k);
    wgetch(k);
    delwin(k);
    save_keybinds();
}

void prompt_reset_settings() {
    echo(); curs_set(1);
    mvprintw(ROWS, 0, "Reset all settings to default? (y/n): ");
    int c = getch();
    noecho(); curs_set(0);
    if (c=='y' || c=='Y') {
        // reset size & colors
        ROWS = DEFAULT_ROWS;
        COLS = DEFAULT_COLS;
        memcpy(colors, DEFAULT_COLORS_MAP, sizeof(colors));
        save_config();

        // reset keybinds
        memcpy(keys, DEFAULT_KEYS_MAP, sizeof(keys));
        key_hint = DEFAULT_KEY_HINT;
        key_quit = DEFAULT_KEY_QUIT;
        save_keybinds();

        // re-init board
        init_colors();
        free_grid();
        allocate_grid();
        do {
            player.y = rand() % ROWS;
            player.x = rand() % COLS;
        } while (grid[player.y][player.x] == 0);

        // reset run
        score = has_moved = show_hint = bad_move_timer = 0;
    }
}

//─── high-score management ─────────────────────────────────────────────────────
void load_high_scores(HighScore hs[], int *n) {
    *n = 0;
    char fname[64];
    build_score_filename(fname, sizeof(fname));
    FILE *f = fopen(fname, "r");
    if (!f) return;
    while (*n < MAX_SCORES &&
           fscanf(f, "%d %f", &hs[*n].score, &hs[*n].percent) == 2)
        (*n)++;
    fclose(f);
}

void save_high_scores(HighScore hs[], int n) {
    char fname[64];
    build_score_filename(fname, sizeof(fname));
    FILE *f = fopen(fname, "w");
    if (!f) return;
    for (int i = 0; i < n; i++)
        fprintf(f, "%d %.2f\n", hs[i].score, hs[i].percent);
    fclose(f);
}

bool update_high_scores(int final_score, float final_percent) {
    HighScore hs[MAX_SCORES+1];
    int n;
    load_high_scores(hs, &n);
    hs[n].score   = final_score;
    hs[n].percent = final_percent;
    n++;
    // sort descending
    for (int i=0;i<n-1;i++)
      for (int j=i+1;j<n;j++)
        if (hs[j].score > hs[i].score) {
          HighScore t = hs[i];
          hs[i] = hs[j];
          hs[j] = t;
        }
    if (n>MAX_SCORES) n = MAX_SCORES;

    printf("=== HIGH SCORES for %dx%d ===\n", ROWS, COLS);
    int arrow = -1;
    for (int i=0;i<n;i++)
      if (hs[i].score==final_score &&
          fabs(hs[i].percent-final_percent)<0.001f)
        arrow = i;
    for (int i=0;i<n;i++) {
      if (i==arrow)
        printf("%d. %d - %.2f%%  <--\n",
               i+1, hs[i].score, hs[i].percent);
      else
        printf("%d. %d - %.2f%%\n",
               i+1, hs[i].score, hs[i].percent);
    }
    save_high_scores(hs, n);
    return (arrow == 0);
}

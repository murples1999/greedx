#include <ncurses.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define ROWS 22
#define COLS 79
#define HIGHSCORE_FILE "greed_scores.txt"
#define MAX_SCORES 10
#define KEYBIND_FILE "greed_keys.txt"

typedef struct { int y, x; } Vec2;
typedef struct { int score; float percent; } HighScore;

int grid[ROWS][COLS];
Vec2 player;
int show_hint = 0, score = 0, has_moved = 0;
int bad_move_timer = 0;
bool show_help = false;
bool show_keybinds = false;
int key_hint = 'p';
int key_quit = 'l';
int key_help = '?';
int key_keybind = '=';


Vec2 directions[8] = {
    {-1,  0}, { 1,  0}, { 0, -1}, { 0,  1},
    {-1, -1}, {-1,  1}, { 1, -1}, { 1,  1}
};
int keys[8] = { 'w','s','a','d','q','e','z','c' };

void init_colors() {
    start_color();
    init_pair(1, COLOR_WHITE,   COLOR_BLACK);   // 1
    init_pair(2, COLOR_YELLOW,  COLOR_BLACK);   // 2
    init_pair(3, COLOR_MAGENTA, COLOR_BLACK);   // 3
    init_pair(4, COLOR_YELLOW,  COLOR_BLACK);   // 4
    init_pair(5, COLOR_GREEN,   COLOR_BLACK);   // 5
    init_pair(6, COLOR_RED,     COLOR_BLACK);   // 6
    init_pair(7, COLOR_GREEN,   COLOR_BLACK);   // 7
    init_pair(8, COLOR_RED,     COLOR_BLACK);   // 8
    init_pair(9, COLOR_CYAN,    COLOR_BLACK);   // 9
}

void draw_grid() {
    erase();
    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
            int val = grid[y][x];
            if (y == player.y && x == player.x) {
                attron(A_BOLD | (has_moved ? 0 : A_BLINK));
                mvaddch(y, x, '@');
                attroff(A_BOLD | A_BLINK);
            } else if (val > 0) {
                if (val >= 6) attron(A_BOLD);
                attron(COLOR_PAIR(val));
                mvaddch(y, x, '0' + val);
                attroff(COLOR_PAIR(val));
                if (val >= 6) attroff(A_BOLD);
            }
        }
    }

    if (show_hint) {
        for (int d = 0; d < 8; d++) {
            Vec2 dir = directions[d];
            int fy = player.y + dir.y, fx = player.x + dir.x;
            if (fy < 0 || fy >= ROWS || fx < 0 || fx >= COLS || grid[fy][fx] == 0) continue;
            int step = grid[fy][fx], ok = 1;
            for (int i = 1; i <= step; i++) {
                int ny = player.y + dir.y * i, nx = player.x + dir.x * i;
                if (ny < 0 || ny >= ROWS || nx < 0 || nx >= COLS || grid[ny][nx] == 0) {
                    ok = 0; break;
                }
            }
            if (ok) {
                for (int i = 1; i <= step; i++)
                    mvchgat(player.y + dir.y * i, player.x + dir.x * i, 1, A_REVERSE, 0, NULL);
            }
        }
    }

    float pct = 100.0f * score / (ROWS * COLS);
    move(ROWS, 0);
    clrtoeol();
    printw("Score: %d  %.2f%%  (%c=hint, %c=quit, %c=help, %c=keybinds)", score, pct, key_hint, key_quit, key_help, key_keybind, score, pct);
    if (bad_move_timer > 0)
        printw("  **Bad Move**");
    refresh();
}

int has_moves() {
    for (int d = 0; d < 8; d++) {
        Vec2 dir = directions[d];
        int fy = player.y + dir.y, fx = player.x + dir.x;
        if (fy<0||fy>=ROWS||fx<0||fx>=COLS||grid[fy][fx]==0) continue;
        int step = grid[fy][fx], ok = 1;
        for (int i = 1; i <= step; i++) {
            int ny = player.y + dir.y*i, nx = player.x + dir.x*i;
            if (ny<0||ny>=ROWS||nx<0||nx>=COLS||grid[ny][nx]==0) {
                ok = 0; break;
            }
        }
        if (ok) return 1;
    }
    return 0;
}

void move_player(int ch) {
    for (int d = 0; d < 8; d++) {
        if (ch == keys[d]) {
            Vec2 dir = directions[d];
            int fy = player.y + dir.y, fx = player.x + dir.x;
            if (fy<0 || fy>=ROWS || fx<0 || fx>=COLS || grid[fy][fx]==0) {
                bad_move_timer = 1;
                return;
            }
            int step = grid[fy][fx], ok = 1;
            for (int i = 1; i <= step; i++) {
                int ny = player.y + dir.y*i, nx = player.x + dir.x*i;
                if (ny<0||ny>=ROWS||nx<0||nx>=COLS||grid[ny][nx]==0) {
                    bad_move_timer = 1;
                    return;
                }
            }
            for (int i = 1; i <= step; i++) {
                int ny = player.y + dir.y*i, nx = player.x + dir.x*i;
                grid[ny][nx] = 0;
                score++;
            }
            player.y += dir.y * step;
            player.x += dir.x * step;
            has_moved = 1;
        }
    }
}

void load_high_scores(HighScore hs[], int *n) {
    *n = 0;
    FILE *f = fopen(HIGHSCORE_FILE, "r");
    if (!f) return;
    while (*n < MAX_SCORES &&
           fscanf(f, "%d %f", &hs[*n].score, &hs[*n].percent) == 2)
        (*n)++;
    fclose(f);
}

void save_high_scores(HighScore hs[], int n) {
    FILE *f = fopen(HIGHSCORE_FILE, "w");
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

    for (int i = 0; i < n-1; i++)
        for (int j = i+1; j < n; j++)
            if (hs[j].score > hs[i].score) {
                HighScore t = hs[i];
                hs[i] = hs[j];
                hs[j] = t;
            }
    if (n > MAX_SCORES) n = MAX_SCORES;

    bool made = false;
    int first_match_index = -1;
    for (int i = 0; i < n; i++) {
        if (hs[i].score == final_score &&
            hs[i].percent == final_percent) {
            made = true;
            if (first_match_index == -1)
                first_match_index = i;
        }
    }
    if (!made) return false;

    save_high_scores(hs, n);

    printf("=== HIGH SCORES ===\n");
    bool arrow = false;
    for (int i = 0; i < n; i++) {
        if (!arrow &&
            hs[i].score == final_score &&
            hs[i].percent == final_percent) {
            printf("%d. %d - %.2f%%  <--\n",
                   i+1, hs[i].score, hs[i].percent);
            arrow = true;
        } else {
            printf("%d. %d - %.2f%%\n",
                   i+1, hs[i].score, hs[i].percent);
        }
    }

    return (first_match_index == 0);
}

void load_keybinds() {
    FILE *f = fopen(KEYBIND_FILE, "r");
    if (!f) return;
    for (int i = 0; i < 8; i++) fscanf(f, "%d", &keys[i]);
    fscanf(f, "%d %d %d %d", &key_hint, &key_quit, &key_help, &key_keybind);
    fclose(f);
}

void save_keybinds() {
    FILE *f = fopen(KEYBIND_FILE, "w");
    if (!f) return;
    for (int i = 0; i < 8; i++) fprintf(f, "%d ", keys[i]);
    fprintf(f, "%d %d %d %d\n", key_hint, key_quit, key_help, key_keybind);
    fclose(f);
}

int main(void) {
    srand(time(NULL));

    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    curs_set(0);
    init_colors();
    load_keybinds();

    for (int y = 0; y < ROWS; y++)
        for (int x = 0; x < COLS; x++)
            grid[y][x] = (rand() % 9) + 1;

    do {
        player.y = rand() % ROWS;
        player.x = rand() % COLS;
    } while (grid[player.y][player.x] == 0);

    while (1) {
        if (show_help) {
            WINDOW *helpwin = newwin(12, 52, (LINES-12)/2, (COLS-52)/2);
            box(helpwin, 0, 0);
            wattron(helpwin, A_BOLD);
            mvwprintw(helpwin, 1, 2, "GREED - HOW TO PLAY");
            wattroff(helpwin, A_BOLD);
            mvwprintw(helpwin, 3, 2, "Move using: %c/%c/%c/%c/%c/%c/%c/%c",
                       keys[0], keys[1], keys[2], keys[3],
                       keys[4], keys[5], keys[6], keys[7]);
            mvwprintw(helpwin, 4, 2, "Show hint: %c", key_hint);
            mvwprintw(helpwin, 5, 2, "Quit game: %c", key_quit);
            mvwprintw(helpwin, 6, 2, "Open keybinds: %c", key_keybind);
            mvwprintw(helpwin, 7, 2, "Your goal is to clear as many tiles as possible.");
            mvwprintw(helpwin, 8, 2, "Step on a number to move exactly that many tiles");
            mvwprintw(helpwin, 9, 2, "in one direction. Tiles are removed as you go.");
            mvwprintw(helpwin, 10, 2, "Press any key to return.");
            wrefresh(helpwin);
            getch();
            delwin(helpwin);
            show_help = false;
            continue;
        }
        draw_grid();
        if (show_keybinds) {
            WINDOW *keywin = newwin(18, 40, (LINES-18)/2, (COLS-40)/2);
            box(keywin, 0, 0);
            wattron(keywin, A_BOLD);
            mvwprintw(keywin, 1, 2, "SET KEYBINDS");
            wattroff(keywin, A_BOLD);

            char prompts[12][20] = {
                "Up:", "Down:", "Left:", "Right:",
                "Up-Left:", "Up-Right:", "Down-Left:", "Down-Right:",
                "Hint:", "Quit:", "Help:", "Keybinds:" };
            int* binds[12] = {
                &keys[0], &keys[1], &keys[2], &keys[3],
                &keys[4], &keys[5], &keys[6], &keys[7],
                (int*)&key_hint, (int*)&key_quit, (int*)&key_help, (int*)&key_keybind };

            for (int i = 0; i < 12; i++) {
                mvwprintw(keywin, 3+i, 2, "%s ", prompts[i]);
                wrefresh(keywin);
                int newch = wgetch(keywin);
                *binds[i] = newch;
                mvwprintw(keywin, 3+i, 18, "%c", newch);
            }

            mvwprintw(keywin, 15, 2, "Press any key to return.");
            wrefresh(keywin);
            getch();
            delwin(keywin);
            save_keybinds();
            show_keybinds = false;
            continue;
        }
        if (bad_move_timer > 0) bad_move_timer--;
        if (!has_moves()) {
            mvprintw(ROWS/2, COLS/2 - 5, "GAME OVER");
            refresh();
            getch();
            break;
        }
        int ch = getch();
        if (ch == key_help) {
            show_help = true;
            continue;
        }
        if (ch == key_keybind) {
            show_keybinds = true;
            continue;
        }
        if (ch == key_quit) break;
        if (ch == key_hint) show_hint = !show_hint;
        else move_player(ch);
    }

    endwin();

    float pct = 100.0f * score / (ROWS * COLS);
    update_high_scores(score, pct);

    return 0;
}

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ncurses.h>


#define COLOR_PAIR_TEXT         1
#define COLOR_PAIR_TEXT_INV     2


typedef struct {
    int x;
    int y;
} Point;

typedef struct {
    int height;
    int width;
} Size;


void init(void);
void quit(void);
void draw(void);
void draw_devices(void);


Size main_windows_size = {0, 0};
int use_color = 0;

WINDOW *wdevices;


int main(int argc, char *argv[])
{
    init();

    draw();
    sleep(5);


    quit();

    return 0;
}

void init(void)
{
    srand((unsigned int)time(NULL));
    initscr();
    nodelay(stdscr, TRUE);
    cbreak();
    curs_set(FALSE);
    keypad(stdscr, TRUE);
    noecho();

    getmaxyx(stdscr, main_windows_size.height, main_windows_size.width);

    use_color = has_colors() == TRUE;

    if (use_color) {
        start_color();
        init_pair(COLOR_PAIR_TEXT, COLOR_YELLOW, COLOR_BLACK);
        init_pair(COLOR_PAIR_TEXT_INV, COLOR_BLACK, COLOR_WHITE);
    }

    wdevices = newwin(main_windows_size.height, main_windows_size.width, 0, 0);
}

void draw(void)
{
    draw_devices();
}

void draw_devices(void)
{
    wclear(wdevices);
    box(wdevices, ACS_VLINE, ACS_HLINE);
    if (use_color) {
        wattron(wdevices, COLOR_PAIR(COLOR_PAIR_TEXT_INV));
    }
    mvwprintw(wdevices, main_windows_size.height/2, main_windows_size.width/2-6, "Hallo, Welt!");
    mvwprintw(wdevices, main_windows_size.height/2+1, main_windows_size.width/2-4, "(%d, %d)", main_windows_size.height, main_windows_size.width);
    if (use_color) {
        wattroff(wdevices, COLOR_PAIR(COLOR_PAIR_TEXT_INV));
    }
    touchwin(wdevices);
    wrefresh(wdevices);
}

void quit(void)
{
    endwin();
    exit(0);
}

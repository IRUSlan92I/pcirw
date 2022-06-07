#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <hw/pci.h>
#include <string.h>
#include <ncurses.h>
#include <inttypes.h>
#include <hw/pci-full.h>


#define MAX_BUS     0xff
#define MAX_DEV     0x1f
#define MAX_FUNC    0x07

#define MAX_KEY_COUNT 10

#define CBUTTON_EXIT        KEY_F(10)
#define CBUTTON_SELECT_UP   KEY_UP
#define CBUTTON_SELECT_DOWN KEY_DOWN

#define COLOR_PAIR_TEXT         1
#define COLOR_PAIR_TEXT_INV     2

#define TO_DEVFUNC(dev, func)   (((dev)<<3)+(func))


typedef struct {
    int x;
    int y;
} Point;

typedef struct {
    int height;
    int width;
} Size;

typedef struct device_t {
    struct device_t *previous;
    struct device_t *next;

    uint8_t bus;
    uint8_t device;
    uint8_t function;

    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command;
    uint16_t status;
    uint8_t revision;
    uint8_t interface;
    uint8_t subclass;
    uint8_t class;
} Device;


void init(void);
void quit(void);

void update(void);
void draw(void);

void draw_devices(void);
void draw_device(Device *d, int y, int x);

void fill_devices(void);
int devices_between(Device *device_1, Device *device_2);
int devices_before(Device *device);
int devices_after(Device *device);

void select_up(void);
void select_down(void);



Size main_windows_size = {0, 0};
int use_color = 0;


WINDOW *wdevices = NULL;

Device *first_device = NULL;
Device *last_device = NULL;

Device *selected_device = NULL;


int main(int argc, char *argv[])
{
    fill_devices();

    init();

    while (1) {
        draw();
        update();
    }

    quit();

    return 0;
}

void init(void)
{
    srand((unsigned int)time(NULL));
    initscr();
    cbreak();
    curs_set(FALSE);
    keypad(stdscr, TRUE);
    noecho();

    getmaxyx(stdscr, main_windows_size.height, main_windows_size.width);

    use_color = has_colors() == TRUE;

    if (use_color) {
        start_color();
        init_pair(COLOR_PAIR_TEXT, COLOR_WHITE, COLOR_BLACK);
        init_pair(COLOR_PAIR_TEXT_INV, COLOR_BLACK, COLOR_WHITE);
    }

    wdevices = newwin(main_windows_size.height, main_windows_size.width, 0, 0);
}

void update(void)
{
    switch (getch()) {
        case CBUTTON_EXIT:
            quit();
            break;
        case CBUTTON_SELECT_UP:
            select_up();
            break;
        case CBUTTON_SELECT_DOWN:
            select_down();
            break;
    }
}

void draw(void)
{
    draw_devices();
}

void draw_devices(void)
{
    wclear(wdevices);
    box(wdevices, ACS_VLINE, ACS_HLINE);

    int x = 2;
    int y = 0;

    mvwprintw(wdevices, y++, x, " b  d  f  vid  did comm stat re in sc  c");


    Device *first_device_for_printing = first_device;
    while (
        devices_between(first_device_for_printing, selected_device) >
                                                    main_windows_size.height-6
    ) {
        first_device_for_printing = first_device_for_printing->next;
    }

    if (first_device_for_printing != first_device) {
        int xx;
        for (xx = x; xx < main_windows_size.width-2; xx+=2) {
            mvwprintw(wdevices, y, xx, "/\\");
        }
        y++;
    }

    Device *d;
    for (d = first_device_for_printing; d != NULL; d = d->next) {
        draw_device(d, y, x);
        y++;

        if (y == main_windows_size.height-1) {
            if (d != last_device) {
                int xx;
                for (xx = x; xx < main_windows_size.width-2; xx+=2) {
                    mvwprintw(wdevices, y-1, xx, "\\/");
                }
            }
            break;
        }
    }



    touchwin(wdevices);
    wrefresh(wdevices);
}

void draw_device(Device *d, int y, int x)
{
    if (use_color && d == selected_device) {
        wattron(wdevices, COLOR_PAIR(COLOR_PAIR_TEXT_INV));
    }

    mvwprintw(wdevices, y, x,
        "%2d %2d %2d %04X %04X %04X %04X %02X %02X %02X %02X",
        d->bus, d->device, d->function, d->vendor_id, d->device_id,
        d->command, d->status, d->revision,
        d->interface, d->subclass, d->class);

    if (use_color && d == selected_device) {
        wattroff(wdevices, COLOR_PAIR(COLOR_PAIR_TEXT_INV));
    }
}

void quit(void)
{
    endwin();
    exit(0);
}

void fill_devices(void)
{
    if (pci_attach(0) == -1) {
        fputs("Unable to attach PCI!", stderr);
        exit(1);
    }

    Device *previous_device = NULL;
    Device **next_device = &first_device;

    unsigned short bus, dev, func;
    for(bus = 0; bus <= MAX_BUS; bus++) {
        for(dev = 0; dev <= MAX_DEV; dev++) {
            for(func = 0; func <= MAX_FUNC; func++) {
                struct pci_dev_info dev_info;
                memset(&dev_info, 0, sizeof(dev_info));

                dev_info.BusNumber = bus;
                dev_info.DevFunc = TO_DEVFUNC(dev, func);

                void *handleDevice = pci_attach_device(NULL, PCI_SEARCH_BUSDEV,
                                                        0, &dev_info);

                if (handleDevice != NULL) {
                    *next_device = malloc(sizeof(Device));
                    (*next_device)->previous = previous_device;
                    (*next_device)->next = NULL;

                    (*next_device)->bus = bus;
                    (*next_device)->device = dev;
                    (*next_device)->function = func;

                    (*next_device)->vendor_id = 0x8086;
                    (*next_device)->device_id = 0xA000;


                    if (pci_read_config(
                        handleDevice, 0x00, 1, 2,&(*next_device)->vendor_id
                    ) != PCI_SUCCESS) {
                        perror("Unable to get vendor_id");
                        exit(1);
                    }

                    if (pci_read_config(
                        handleDevice, 0x02, 1, 2, &(*next_device)->device_id
                    ) != PCI_SUCCESS) {
                        perror("Unable to get device_id");
                        exit(1);
                    }

                    if (pci_read_config(
                        handleDevice, 0x04, 1, 2, &(*next_device)->command
                    ) != PCI_SUCCESS) {
                        perror("Unable to get command");
                        exit(1);
                    }

                    if (pci_read_config(
                        handleDevice, 0x06, 1, 2, &(*next_device)->status
                    ) != PCI_SUCCESS) {
                        perror("Unable to get status");
                        exit(1);
                    }

                    if (pci_read_config(
                        handleDevice, 0x08, 1, 1, &(*next_device)->revision
                    ) != PCI_SUCCESS) {
                        perror("Unable to get revision");
                        exit(1);
                    }

                    if (pci_read_config(
                        handleDevice, 0x09, 1, 1, &(*next_device)->interface
                    ) != PCI_SUCCESS) {
                        perror("Unable to get interface");
                        exit(1);
                    }

                    if (pci_read_config(
                        handleDevice, 0x0a, 1, 1, &(*next_device)->subclass
                    ) != PCI_SUCCESS) {
                        perror("Unable to get subclasscode");
                        exit(1);
                    }

                    if (pci_read_config(
                        handleDevice, 0x0b, 1, 1, &(*next_device)->class
                    ) != PCI_SUCCESS) {
                        perror("Unable to get classcode");
                        exit(1);
                    }

                    pci_detach_device(handleDevice);

                    previous_device = *next_device;
                    last_device = previous_device;
                    next_device = &(*next_device)->next;
                }
            }
        }
    }

    selected_device = first_device;
}

void select_up(void)
{

    if (selected_device != NULL && selected_device->previous != NULL) {
        selected_device = selected_device->previous;
    }
}

void select_down(void)
{
    if (selected_device != NULL && selected_device->next != NULL) {
        selected_device = selected_device->next;
    }
}

int devices_between(Device *device_1, Device *device_2)
{
    if (device_1 == NULL || device_2 == NULL || device_1 == device_2) {
        return 0;
    }

    int i = 0;
    Device *d = device_1->next;
    while (d != NULL) {
        if (d == device_2) {
            return i;
        }
        i++;
        d = d->next;
    }

    return devices_between(device_2, device_1);
}

int devices_before(Device *device)
{
    int i = 0;
    if (device != NULL) {
        Device *d;
        for (d = device->previous; d != NULL; d = d->previous) {
            i++;
        }
    }

    return i;
}

int devices_after(Device *device)
{
    int i = 0;
    if (device != NULL) {
        Device *d;
        for (d = device->next; d != NULL; d = d->next) {
            i++;
        }
    }

    return i;
}

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

#define CBUTTON_EXIT            KEY_F(10)
#define CBUTTON_SELECT_UP       KEY_UP
#define CBUTTON_SELECT_DOWN     KEY_DOWN
#define CBUTTON_ABOUT_DEVICE    '/'
#define CBUTTON_SHOW_MODE       't'
#define CBUTTON_ESCAPE          27

#define COLOR_PAIR_TEXT         1
#define COLOR_PAIR_TEXT_INV     2

#define CLASS_BRIDGE            6
#define SUBCLASS_BRIDGE_PCI     4

#define DEVICE_BAR_COUNT        6
#define BRIDGE_BAR_COUNT        2

#define TO_DEVFUNC(dev, func)   (((dev)<<3)+(func))


typedef struct {
    int x;
    int y;
} Point;

typedef struct {
    int height;
    int width;
} Size;

typedef enum {
    Device,
    TypeDevice,
    TypeBridge,
} Type;

typedef enum {
    ShowModeTable,
    ShowModeTree,
} ShowMode;

typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;

    uint16_t command;
    uint16_t status;

    uint8_t revision;
    uint8_t interface;
    uint8_t subclass;
    uint8_t class;

    uint8_t cache_line_size;
    uint8_t latency_timer;
    uint8_t header_type;
    uint8_t bist;

    uint32_t bars[DEVICE_BAR_COUNT];

    uint32_t cis_pointer;

    uint16_t ss_vendor_id;
    uint16_t ss_device_id;

    uint32_t exp_bar;

    uint8_t capabilities;

    uint8_t reserved[7];

    uint8_t int_line;
    uint8_t int_pin;
    uint8_t min_gnt;
    uint8_t max_lat;
} DeviceConfig;

typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;

    uint16_t command;
    uint16_t status;

    uint8_t revision;
    uint8_t interface;
    uint8_t subclass;
    uint8_t class;

    uint8_t cache_line_size;
    uint8_t latency_timer;
    uint8_t header_type;
    uint8_t bist;

    uint32_t bars[BRIDGE_BAR_COUNT];

    uint8_t primary_bus;
    uint8_t secondary_bus;
    uint8_t subordinate_bus;
    uint8_t secondary_latency_timer;

    uint8_t io_base;
    uint8_t io_limit;
    uint16_t secondary_status;

    uint16_t memory_base;
    uint16_t memory_limit;

    uint16_t prefetch_memory_base;
    uint16_t prefetch_memory_limit;

    uint32_t prefetch_base_upper;

    uint32_t prefetch_limit_upper;

    uint16_t io_base_upper;
    uint16_t io_limit_upper;

    uint8_t capabilities;

    uint8_t reserved[3];

    uint32_t exp_bar;

    uint8_t int_line;
    uint8_t int_pin;
    uint16_t bridge_control;
} BridgeConfig;

typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;

    uint16_t command;
    uint16_t status;

    uint8_t revision;
    uint8_t interface;
    uint8_t subclass;
    uint8_t class;

    uint8_t cache_line_size;
    uint8_t latency_timer;
    uint8_t header_type;
    uint8_t bist;

    uint32_t reserved[12];
} UnificatedConfig;

typedef union {
    DeviceConfig device;
    BridgeConfig bridge;
    UnificatedConfig uni;
} PciConfig;

typedef struct _BridgeDevice {
    struct _BridgeDevice *previous;
    struct _BridgeDevice *next;

    struct _BridgeDevice *first_connected_device;
    struct _BridgeDevice *last_connected_device;
    struct _BridgeDevice *previous_connected_device;
    struct _BridgeDevice *next_connected_device;

    int nesting;

    uint8_t bus;
    uint8_t device;
    uint8_t function;

    uint8_t pci_index;

    Type type;

    PciConfig pci_config;

    uint32_t bar_sizes[max(DEVICE_BAR_COUNT, BRIDGE_BAR_COUNT)];

    char *desc_vendor;
    char *desc_device;
} BridgeDevice;

typedef struct {
    uint8_t wdevices:1;
    uint8_t waboutd:1;
} WindowFlags;


void init(void);
void quit(void);

void update(void);
void draw(void);

void draw_wdevices(void);
void draw_waboutd(void);
void draw_device(BridgeDevice *d, int y, int x);

void fill_devices(void);
void update_device_data(BridgeDevice *device);
uint8_t get_pci_index(const struct pci_dev_info *dev_info);
char *get_vendor_description(struct pci_dev_info *dev_info);
char *get_device_description(struct pci_dev_info *dev_info);
int devices_between(BridgeDevice *device_1, BridgeDevice *device_2);
int devices_before(BridgeDevice *device);
int devices_after(BridgeDevice *device);
void fill_tree(void);
void fill_bridge(BridgeDevice *bridge, int nesting);

void select_up(void);
void select_down(void);
void switch_waboutd(void);
void switch_show_mode(void);
void hide_all_subwindows(void);


Size stdscr_size = {0, 0};
Size wdevice_size = {0, 0};
Size waboutd_size = {0, 0};
int use_color = 0;


WINDOW *wdevices = NULL;
WINDOW *waboutd = NULL;

WindowFlags visible_windows;
ShowMode show_mode;

BridgeDevice *first_device = NULL;
BridgeDevice *last_device = NULL;

BridgeDevice *root_device = NULL;

BridgeDevice *selected_device = NULL;


int main(int argc, char *argv[])
{
    if (sizeof(DeviceConfig) != 64 ||
        sizeof(BridgeConfig) != 64 ||
        sizeof(UnificatedConfig) != 64
    ) {
        fprintf(stderr, "Wrong size of config structs!\n");
        return 1;
    }

    fill_devices();

    init();

    refresh();
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

    hide_all_subwindows();
    show_mode = ShowModeTable;

    use_color = has_colors() == TRUE;

    if (use_color) {
        start_color();
        init_pair(COLOR_PAIR_TEXT, COLOR_WHITE, COLOR_BLACK);
        init_pair(COLOR_PAIR_TEXT_INV, COLOR_BLACK, COLOR_WHITE);
    }

    getmaxyx(stdscr, stdscr_size.height, stdscr_size.width);

    wdevice_size.height = min(stdscr_size.height, 26);
    wdevice_size.width = min(stdscr_size.width, 62);

    Point wdevice_pos;
    wdevice_pos.y = (stdscr_size.height-wdevice_size.height)/2;
    wdevice_pos.x = (stdscr_size.width-wdevice_size.width)/2;

    waboutd_size.height = min(stdscr_size.height, 16);
    waboutd_size.width = min(stdscr_size.width, 70);

    Point waboutd_pos;
    waboutd_pos.y = (stdscr_size.height-waboutd_size.height)/2;
    waboutd_pos.x = (stdscr_size.width-waboutd_size.width)/2;

    wdevices = newwin(wdevice_size.height, wdevice_size.width,
                wdevice_pos.y, wdevice_pos.x);
    waboutd = newwin(waboutd_size.height, waboutd_size.width,
                waboutd_pos.y, waboutd_pos.x);
}

void update(void)
{
    static int is_first = 1;
    if (is_first) {
        is_first = 0;
        return;
    }

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
        case CBUTTON_ABOUT_DEVICE:
            switch_waboutd();
            break;
        case CBUTTON_SHOW_MODE:
            switch_show_mode();
            break;
        case CBUTTON_ESCAPE:
            hide_all_subwindows();
            break;
    }
}

void draw(void)
{
    if (visible_windows.wdevices) {
        draw_wdevices();
    }
    if (visible_windows.waboutd) {
        draw_waboutd();
    }

    refresh();
}

void draw_wdevices(void)
{
    wclear(wdevices);
    box(wdevices, ACS_VLINE, ACS_HLINE);

    int x = 2;
    int y = 0;

    if (show_mode == ShowModeTable) {
        mvwprintw(wdevices, y, x, "typ  b  d f  vid  did di  cmd stat rv  c sc if cs lt ht bi");
    }

    y++;

    BridgeDevice *first_device_for_printing = first_device;
    while (
        devices_between(first_device_for_printing, selected_device) >
                                                    wdevice_size.height-6
    ) {
        first_device_for_printing = first_device_for_printing->next;
    }

    if (first_device_for_printing != first_device) {
        int xx;
        for (xx = x; xx < wdevice_size.width-2; xx+=2) {
            mvwprintw(wdevices, y, xx, "/\\");
        }
        y++;
    }

    BridgeDevice *d;
    for (d = first_device_for_printing; d != NULL; d = d->next) {
        draw_device(d, y, x);
        y++;

        if (y == wdevice_size.height-1) {
            if (d != last_device) {
                int xx;
                for (xx = x; xx < wdevice_size.width-2; xx+=2) {
                    mvwprintw(wdevices, y-1, xx, "\\/");
                }
            }
            break;
        }
    }

    mvwprintw(wdevices, wdevice_size.height-1, 2, "[/ - about]");

    if (show_mode == ShowModeTable) {
        mvwprintw(wdevices, wdevice_size.height-1, 15, "[T - tree]");
    }
    else if (show_mode == ShowModeTree) {
        mvwprintw(wdevices, wdevice_size.height-1, 15, "[T - table]");
    }
    mvwprintw(wdevices, wdevice_size.height-1, wdevice_size.width-14, "[F10 - exit]");

    touchwin(wdevices);
    wrefresh(wdevices);
}

void draw_waboutd(void)
{
    wclear(waboutd);
    box(waboutd, ACS_VLINE, ACS_HLINE);

    int x = 2;
    int y = 0;

    mvwprintw(waboutd, y, x, "{%04X %04X %d}",
            selected_device->pci_config.uni.vendor_id,
            selected_device->pci_config.uni.device_id,
            selected_device->pci_index
    );

    mvwprintw(waboutd, y, x+16, "{%d %d %d}",
            selected_device->bus,
            selected_device->device,
            selected_device->function
    );

    y++;

    if (selected_device->desc_vendor) {
        mvwprintw(waboutd, y++, x, "Vendor: %s", selected_device->desc_vendor);
    }
    if (selected_device->desc_device) {
        mvwprintw(waboutd, y++, x, "Device: %s", selected_device->desc_device);
    }

    if (selected_device->type == TypeDevice) {
        int i;
        for (i = 0; i < DEVICE_BAR_COUNT; i++) {
            if (selected_device->bar_sizes[i]) {
                mvwprintw(waboutd, y++, x, "BAR %d: %X (%u)",
                    i, selected_device->pci_config.device.bars[i],
                    selected_device->bar_sizes[i]
                );
            }
        }
    }
    else if (selected_device->type == TypeBridge) {
        mvwprintw(waboutd, y++, x, "Primary Bus: %d", selected_device->pci_config.bridge.primary_bus);
        mvwprintw(waboutd, y++, x, "Secondary Bus: %d", selected_device->pci_config.bridge.secondary_bus);
        mvwprintw(waboutd, y++, x, "Subordinate Bus: %d", selected_device->pci_config.bridge.subordinate_bus);
        int i;
        for (i = 0; i < BRIDGE_BAR_COUNT; i++) {
            if (selected_device->bar_sizes[i]) {
                mvwprintw(waboutd, y++, x, "BAR %d: %X (%u)",
                    i, selected_device->pci_config.bridge.bars[i],
                    selected_device->bar_sizes[i]
                );
            }
        }
    }

    touchwin(waboutd);
    wrefresh(waboutd);
}

void draw_device(BridgeDevice *d, int y, int x)
{
    if (use_color && d == selected_device) {
        wattron(wdevices, COLOR_PAIR(COLOR_PAIR_TEXT_INV));
    }

    if (show_mode == ShowModeTable) {
        mvwprintw(wdevices, y, x,
            "%s %2d %2d %d %04X %04X %2d %04X %04X %02X %02X %02X %02X %02X %02X %02X %02X",
            d->type == TypeBridge ? "Bri" : "Dev",
            d->bus,
            d->device,
            d->function,
            d->pci_config.uni.vendor_id,
            d->pci_config.uni.device_id,
            d->pci_index,
            d->pci_config.uni.command,
            d->pci_config.uni.status,
            d->pci_config.uni.revision,
            d->pci_config.uni.class,
            d->pci_config.uni.subclass,
            d->pci_config.uni.interface,
            d->pci_config.uni.cache_line_size,
            d->pci_config.uni.latency_timer,
            d->pci_config.uni.header_type,
            d->pci_config.uni.bist
        );
    }
    else if (show_mode == ShowModeTree) {
        mvwprintw(wdevices, y, x,
            "%*s %s %2d %2d %d %04X %04X %2d",
            d->nesting, "",
            d->type == TypeBridge ? "Bri" : "Dev",
            d->bus,
            d->device,
            d->function,
            d->pci_config.uni.vendor_id,
            d->pci_config.uni.device_id,
            d->pci_index
        );
    }

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

    BridgeDevice *previous_device = NULL;
    BridgeDevice **next_device = &first_device;

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
                    *next_device = malloc(sizeof(BridgeDevice));
                    if (*next_device == NULL) {
                        perror("Unable to get bar");
                        exit(1);
                    }

                    (*next_device)->previous = previous_device;
                    (*next_device)->next = NULL;

                    (*next_device)->bus = bus;
                    (*next_device)->device = dev;
                    (*next_device)->function = func;

                    (*next_device)->first_connected_device = NULL;
                    (*next_device)->last_connected_device = NULL;
                    (*next_device)->previous_connected_device = NULL;
                    (*next_device)->next_connected_device = NULL;

                    (*next_device)->nesting = 0;

                    (*next_device)->pci_index = get_pci_index(&dev_info);

                    update_device_data(*next_device);

                    pci_detach_device(handleDevice);

                    (*next_device)->desc_vendor = get_vendor_description(&dev_info);
                    (*next_device)->desc_device = get_device_description(&dev_info);

                    previous_device = *next_device;
                    last_device = previous_device;
                    next_device = &(*next_device)->next;
                }
            }
        }
    }

    selected_device = first_device;

    fill_tree();
}

char *get_vendor_description(struct pci_dev_info *dev_info)
{
    int i;
    for (i = 0; i < PCI_VENTABLE_LEN; i++) {
        if (PciVenTable[i].VenId == dev_info->VendorId) {
            return PciVenTable[i].VenFull;
        }
    }
    return NULL;
}

char *get_device_description(struct pci_dev_info *dev_info)
{
    int i;
    for (i = 0; i < PCI_DEVTABLE_LEN; i++) {
        if (PciDevTable[i].VenId == dev_info->VendorId &&
            PciDevTable[i].DevId == dev_info->DeviceId) {
            return PciDevTable[i].ChipDesc;
        }
    }
    return NULL;
}

uint8_t get_pci_index(const struct pci_dev_info *dev_info)
{
    unsigned int bus;
    unsigned int dev_func;

    uint8_t i;
    for (i = 0; i < 0xFF; i++) {
        if (pci_find_device(
            dev_info->DeviceId, dev_info->VendorId, i, &bus, &dev_func
        ) != PCI_SUCCESS) {
            break;
        }
        if (bus == dev_info->BusNumber && dev_func == dev_info->DevFunc) {
            return i;
        }
    }

    return 0xFF;
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

void switch_waboutd(void)
{
    visible_windows.waboutd = !visible_windows.waboutd;
}

void switch_show_mode(void)
{
    if (show_mode == ShowModeTable)         show_mode = ShowModeTree;
    else if (show_mode == ShowModeTree)     show_mode = ShowModeTable;
}

void hide_all_subwindows(void)
{
    memset(&visible_windows, 0, sizeof(visible_windows));
    visible_windows.wdevices = 1;
}

int devices_between(BridgeDevice *device_1, BridgeDevice *device_2)
{
    if (device_1 == NULL || device_2 == NULL || device_1 == device_2) {
        return 0;
    }

    int i = 0;
    BridgeDevice *d = device_1->next;
    while (d != NULL) {
        if (d == device_2) {
            return i;
        }
        i++;
        d = d->next;
    }

    return devices_between(device_2, device_1);
}

int devices_before(BridgeDevice *device)
{
    int i = 0;
    if (device != NULL) {
        BridgeDevice *d;
        for (d = device->previous; d != NULL; d = d->previous) {
            i++;
        }
    }

    return i;
}

int devices_after(BridgeDevice *device)
{
    int i = 0;
    if (device != NULL) {
        BridgeDevice *d;
        for (d = device->next; d != NULL; d = d->next) {
            i++;
        }
    }

    return i;
}

void update_device_data(BridgeDevice *device)
{
    if (pci_read_config8(
        device->bus, TO_DEVFUNC(device->device, device->function),
        0, sizeof(device->pci_config), &device->pci_config
    ) != PCI_SUCCESS) {
        perror("Unable to get pci config");
        exit(1);
    }

    device->type = device->pci_config.uni.class == CLASS_BRIDGE ? TypeBridge : TypeDevice;

    memset(device->bar_sizes, 0, sizeof(device->bar_sizes));

    int bar_count = device->type == TypeBridge ? BRIDGE_BAR_COUNT : DEVICE_BAR_COUNT;
    int i;
    for (i = 0; i < bar_count; i++) {
        uint32_t data = 0xFFFFFFFF;
        uint32_t address;
        if (pci_read_config32(
            device->bus, TO_DEVFUNC(device->device, device->function),
            0x10+0x04*i, 1, &address
        ) != PCI_SUCCESS) {
            perror("Unable to get bar");
            exit(1);
        }
        if (pci_write_config32(
            device->bus, TO_DEVFUNC(device->device, device->function),
            0x10+0x04*i, 1, &data
        ) != PCI_SUCCESS) {
            perror("Unable to write bar");
            exit(1);
        }
        if (pci_read_config32(
            device->bus, TO_DEVFUNC(device->device, device->function),
            0x10+0x04*i, 1, &device->bar_sizes[i]
        ) != PCI_SUCCESS) {
            perror("Unable to get bar");
            exit(1);
        }
        if (pci_write_config32(
            device->bus, TO_DEVFUNC(device->device, device->function),
            0x10+0x04*i, 1, &address
        ) != PCI_SUCCESS) {
            perror("Unable to write bar");
            exit(1);
        }

        if (device->bar_sizes[i] & 0x1 && device->bar_sizes[i] & 0x2) {
            device->bar_sizes[i] = 0;
        }
        else {
            if (device->bar_sizes[i] & 0x1) {
                device->bar_sizes[i] &= ~0x3;
            }
            else {
                device->bar_sizes[i] &= ~0xF;
            }

            device->bar_sizes[i] &= (~device->bar_sizes[i])+1;
        }
    }
}

void fill_tree(void)
{
    int nesting = 0;
    BridgeDevice *d;
    for (d = first_device; d != NULL; d = d->next) {
        if (d->type == TypeBridge && d->pci_config.uni.subclass == 0) {
            if (root_device == NULL) {
                root_device = d;
                root_device->nesting = nesting;
            }
            else {
                fprintf(stderr, "Too many root devices!\n");
                exit(1);
            }
        }
    }

    fill_bridge(root_device, nesting+1);
}

void fill_bridge(BridgeDevice *bridge, int nesting)
{
    BridgeDevice *d;
    for (d = first_device; d != NULL; d = d->next) {
        int is_host_bridge = d->type == TypeBridge && d->pci_config.uni.subclass == 0;
        int is_on_this_bus = bridge->pci_config.bridge.secondary_bus == d->bus;

        if (!is_host_bridge && is_on_this_bus) {
            d->nesting = nesting;
            if (bridge->last_connected_device == NULL) {
                bridge->first_connected_device = d;
                bridge->last_connected_device = d;
            }
            else {
                bridge->last_connected_device->next_connected_device = d;
                d->previous_connected_device = bridge->last_connected_device;
                bridge->last_connected_device = d;
            }

            if (d->type == TypeBridge && d->pci_config.uni.subclass == SUBCLASS_BRIDGE_PCI) {
                fill_bridge(d, nesting+1);
            }
        }
    }
}

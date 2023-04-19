#ifndef VGA_H
#define VGA_H

#include "debug.h"
#include "config.h"
#include "u8250.h"
#include "smp.h"
#include "machine.h"
#include "kernel.h"
#include "heap.h"
#include "threads.h"
#include "pit.h"
#include "idt.h"
#include "crt.h"
#include "stdint.h"
#include "port.h"
#include "ext2.h"
#include "pit.h"
#include "names.h"

// OSDEV VGA PORTS: https://wiki.osdev.org/VGA_Hardware
// ALL INFO GATHERED FROM OSDEV AND CIRRUS CL-GD5446

#define VIDEO_MEM_BUF 0XB8000
/*
* Initial state is unknown
* write both index and data bytes to same port. VGA keeps track of 
* whether next write is the index or data byte.
* Read from port 3DA to go to index state.
* To read:
* feed index into port 3C0, *confused on this step to the next
* then read the value from port 3C1
* then restart from port 3DA because we don't know to expect a index or data byte.
*/
#define ATTRIBUTE_INDEX_WRITE 0x3C0
#define ATTRIBUTE_RESET_INDEX 0x3DA
#define ATTRIBUTE_READ 0X3C1

// Miscellaneous Output Register
/*
* uses port 3C2 for writing, 3CC for reading.
* Bit 0 is assumed to be set.
* Bit 0 of this register controls the location of several other registers (?)
* if cleared, port 3D4 is mapped to 3B4 and port 3DA is mapped to 3BA
*/
#define MISC_WRITE 0x3C2 // bit 0 is relevant
#define MISC_READ 0x3CC

// INDEXED REGISTERS control the color palette

// INDEXED REGISTER
// Sequencer Register
/*
* Index byte is written to given port (3C4, 3CE, 3D4), 
* data byte read/written from port+1 (3C5, 3CF, 3D5) -- 8 bits each?
*/

// INDEXED REGISTER
#define SEQ_INDEX 0x3C4
#define SEQ_RW 0x3C5

// INDEXED REGISTER
// Graphics Control Register
#define GRAPHICS_CTRL_INDEX 0x3CE
#define GRAPHICS_CTRL_RW 0x3CF

// INDEXED REGISTER
// CRT Control Registers
/*
* 3D4 has some extra requirements:
* Requires bit 0 of 3C2 (misc output register) to be set before 
* it responds to this address (if bit 0 is 0, ports appear at 3B4)
* registers 0-7 of 3D4 are write protected by protect bit (bit 7 of index 0x11)
*/
#define CRTC_COLOR_INDEX 0x3D4 // if MISC_WRITE[0] is set
#define CRTC_COLOR_WRITE 0x3D5
#define CRTC_MONOCHROME_INDEX 0x3B4 // if MISC_WRITE[0] == 0
#define CRTC_MONOCHROME_WRITE 0x3B5

/*
 * only contains DAC (Digital to Analog Converter) mask register, can be 
 * accessed by a read/write operation on this port.
 * under normal conditions, it should contain 0xFF
 */
#define DAC_MASK 0x3C6

/*
* 3C8, 3C9, 3C7 control the DAC. Each register in DAC has 18 bits, 6 bits for each color (RGB)
* To write a color, write color index to port 3C8, write 3 bytes to 3C9 in RGB order.
* If you want to write multiple consecutive DAC entries, only write the first entry's index
* to 3C8 then write all values to 3C9, so like: (RGB-RGB-RGB). Accessed DAC entry will automatically
* move to next three bytes (next color) whenever you access one.
To read DAC entries:
* write index to be read to 3C7
* read bytes from port 3C9 in similar fashion (like writing, it will increment 3 bytes automatically)
*/
#define COLOR_PALETTE_INDEX_READ 0x3C7
#define COLOR_PALETTE_INDEX_WRITE 0x3C8
#define COLOR_PALETTE_DATA_RW 0x3C9

// colors
enum vga_colors {
    BLACK,          // (0, 0, 0)
    NAVY_BLUE,           // (0, 0, 255)      
    GREEN,          // (0, 255, 0)      
    TEAL,           // (0, 100, 100)    
    RED,            // (255, 0, 0)      
    PINKISH_PURPLE,        // (255, 0, 255)
    MUSTARD,          // (150, 75, 0)
    GRAY,           // (128, 128, 128)
    DARK_BLUE,      // (90, 90, 90)
    BLUE,    // (0, 150, 255)
    MINT,   // (170, 255, 0)
    SKY_BLUE,    // (0, 100, 100)
    RED_WINE,     // (238, 75, 43)
    BRIGHTER_PURPLE, // (255, 0, 255)
    DIJON_MUSTARD,         // (255, 255, 0)
    LAVENDER,
    DARK_GREEN,
    DENIM_BLUE,
    NEON_GREEN,
    BLUEISH_GREEN,
    BROWN,
    PASTEL_PURPLE,
    YELLOW_GREEN,
    WHITEISH_GREEN,
    GASOLINE_GREEN,
    ELECTRIC_BLUE,
    GREEN_TWO,
    TURQUOISE,
    RED_CLAY,
    BRIGHT_PURPLE,
    YELLOW_GREEN_TWO,
    REALLY_BRIGHT_SKY,
    MAROON,
    JOKER_PURPLE,
    SLIGHTLY_DARK_GREEN,
    DARK_TEAL,
    BRIGHT_RED,
    NEON_PINK,
    ORANGE_JUICE,
    SALMON,
    GRAPE,
    PURPLEISH_BLUE,
    GREEN_THREE,
    SKY_BLUE_TWO,
    PINKISH_RED,
    HOT_PINK,
    LIGHT_MUSTARD, 
    LILAC,
    FOREST_GREEN,
    PASTEL_INDIGO,
    SPEARAMINT,
    MOUTHWASH_GREEN,
    ORANGE,
    PINK,
    BRIGHT_YELLOW,
    WHITE_YELLOW,
    DARK_GRAY,
    INDIGO,
    GREEN_FOUR,
    TURQUOISE_TWO,
    CORAL,
    HOT_PINK_TWO,
    YELLOW,
    WHITE            // (255, 255, 255)
};

class VGA {
    public:
    Port attribute_port;
    Port misc_port;
    Port seq_port;
    Port graphics_ctrl;
    Port crt_ctrl_color;
    Port crt_ctrl_mono;
    Port dac_mask_port;
    Port color_pallete_port_write;
    Port color_pallete_port_read;
    uint8_t bg_color;
    volatile bool playing = 0;
    volatile bool new_song = 1;
    
    Atomic<uint32_t> elapsed_time {0};
    uint32_t last_jif;

    Shared<Names_List> fs;
    Shared<File_Node> curr; 

    uint32_t length;
    uint32_t width;
    
    VGA(){};

    void set_miscellaneous_registers();
    void set_sequencer_registers();
    void set_crt_controller_registers();
    void set_graphics_controller_registers();
    void set_attribute_controller_registers();
    void setup(Shared<Names_List> root_fs, Shared<File_Node> curr, bool isGraphics);

    void initializeGraphics();

    void initializePalette();

    bool setPortsText(unsigned char*);

    void initializePorts();

    void initializeScreen(uint8_t color);

    void drawCircle(int centerX, int centerY, int radius, uint8_t color);

    void drawPauseCircle(uint32_t c_x, uint32_t c_y, uint32_t r, uint8_t color);

    uint8_t getColor(uint8_t r, uint8_t g, uint8_t b);

    void putPixel(uint16_t x, uint16_t y, uint8_t color);

    void drawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t color);

    void playingSong(uint32_t);

    void drawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t color, bool fill);

    void drawTriangle(uint16_t x1, uint16_t y1, uint16_t length, uint8_t color, bool flip);
    
    uint8_t* getFrameBuffer();

    void useTextMode(char* buf, uint32_t size);

    void initTextMode();

    void drawChar(int x, int y, char c, uint8_t color);

    void drawString(int x, int y, const char* str, uint8_t color);

    void homeScreen(const char* name);

    // void spotify(const char* name, bool willPlay);
    void spotify(Shared<File_Node> song, bool willPlay);

    void spotify_move(Shared<File_Node> song, bool willPlay, bool skip);

    void play_pause();

    void place_bmp(uint32_t x, uint32_t ending_y, uint32_t pic_width, uint32_t pic_length, char* rgb_buf);

    void moveOutPic(uint32_t x, uint32_t y, Shared<File_Node> fn, uint32_t pic_width, uint32_t pic_length, bool isLeft);

    uint8_t vga_font[128][8] = {
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0000 (nul)
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0001
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0002
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0003
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0004
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0005
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0006
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0007
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0008
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0009
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+000A
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+000B
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+000C
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+000D
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+000E
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+000F
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0010
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0011
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0012
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0013
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0014
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0015
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0016
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0017
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0018
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0019
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+001A
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+001B
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+001C
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+001D
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+001E
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+001F
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0020 (space)
        { 0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00},   // U+0021 (!)
        { 0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0022 (")
        { 0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00},   // U+0023 (#)
        { 0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00},   // U+0024 ($)
        { 0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00},   // U+0025 (%)
        { 0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00},   // U+0026 (&)
        { 0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0027 (')
        { 0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00},   // U+0028 (()
        { 0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00},   // U+0029 ())
        { 0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00},   // U+002A (*)
        { 0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00},   // U+002B (+)
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x06},   // U+002C (,)
        { 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00},   // U+002D (-)
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00},   // U+002E (.)
        { 0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00},   // U+002F (/)
        { 0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00},   // U+0030 (0)
        { 0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00},   // U+0031 (1)
        { 0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00},   // U+0032 (2)
        { 0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00},   // U+0033 (3)
        { 0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00},   // U+0034 (4)
        { 0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00},   // U+0035 (5)
        { 0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00},   // U+0036 (6)
        { 0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00},   // U+0037 (7)
        { 0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00},   // U+0038 (8)
        { 0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00},   // U+0039 (9)
        { 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00},   // U+003A (:)
        { 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x06},   // U+003B (//)
        { 0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00},   // U+003C (<)
        { 0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00, 0x00},   // U+003D (=)
        { 0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00},   // U+003E (>)
        { 0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00},   // U+003F (?)
        { 0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00},   // U+0040 (@)
        { 0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00},   // U+0041 (A)
        { 0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00},   // U+0042 (B)
        { 0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00},   // U+0043 (C)
        { 0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00},   // U+0044 (D)
        { 0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00},   // U+0045 (E)
        { 0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00},   // U+0046 (F)
        { 0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00},   // U+0047 (G)
        { 0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00},   // U+0048 (H)
        { 0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},   // U+0049 (I)
        { 0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00},   // U+004A (J)
        { 0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00},   // U+004B (K)
        { 0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00},   // U+004C (L)
        { 0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00},   // U+004D (M)
        { 0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00},   // U+004E (N)
        { 0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00},   // U+004F (O)
        { 0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00},   // U+0050 (P)
        { 0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00},   // U+0051 (Q)
        { 0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00},   // U+0052 (R)
        { 0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00},   // U+0053 (S)
        { 0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},   // U+0054 (T)
        { 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00},   // U+0055 (U)
        { 0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00},   // U+0056 (V)
        { 0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00},   // U+0057 (W)
        { 0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00},   // U+0058 (X)
        { 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00},   // U+0059 (Y)
        { 0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00},   // U+005A (Z)
        { 0x1E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1E, 0x00},   // U+005B ([)
        { 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00},   // U+005C (\)
        { 0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x00},   // U+005D (])
        { 0x08, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00},   // U+005E (^)
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF},   // U+005F (_)
        { 0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0060 (`)
        { 0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00},   // U+0061 (a)
        { 0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3B, 0x00},   // U+0062 (b)
        { 0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00},   // U+0063 (c)
        { 0x38, 0x30, 0x30, 0x3e, 0x33, 0x33, 0x6E, 0x00},   // U+0064 (d)
        { 0x00, 0x00, 0x1E, 0x33, 0x3f, 0x03, 0x1E, 0x00},   // U+0065 (e)
        { 0x1C, 0x36, 0x06, 0x0f, 0x06, 0x06, 0x0F, 0x00},   // U+0066 (f)
        { 0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x1F},   // U+0067 (g)
        { 0x07, 0x06, 0x36, 0x6E, 0x66, 0x66, 0x67, 0x00},   // U+0068 (h)
        { 0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},   // U+0069 (i)
        { 0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E},   // U+006A (j)
        { 0x07, 0x06, 0x66, 0x36, 0x1E, 0x36, 0x67, 0x00},   // U+006B (k)
        { 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},   // U+006C (l)
        { 0x00, 0x00, 0x33, 0x7F, 0x7F, 0x6B, 0x63, 0x00},   // U+006D (m)
        { 0x00, 0x00, 0x1F, 0x33, 0x33, 0x33, 0x33, 0x00},   // U+006E (n)
        { 0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00},   // U+006F (o)
        { 0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F},   // U+0070 (p)
        { 0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x78},   // U+0071 (q)
        { 0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00},   // U+0072 (r)
        { 0x00, 0x00, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x00},   // U+0073 (s)
        { 0x08, 0x0C, 0x3E, 0x0C, 0x0C, 0x2C, 0x18, 0x00},   // U+0074 (t)
        { 0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6E, 0x00},   // U+0075 (u)
        { 0x00, 0x00, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00},   // U+0076 (v)
        { 0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00},   // U+0077 (w)
        { 0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00},   // U+0078 (x)
        { 0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F},   // U+0079 (y)
        { 0x00, 0x00, 0x3F, 0x19, 0x0C, 0x26, 0x3F, 0x00},   // U+007A (z)
        { 0x38, 0x0C, 0x0C, 0x07, 0x0C, 0x0C, 0x38, 0x00},   // U+007B ({)
        { 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00},   // U+007C (|)
        { 0x07, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0x07, 0x00},   // U+007D (})
        { 0x6E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+007E (~)
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}    // U+007F
    };

    uint8_t palette[192] = {
        0x00, 0x00, 0x00,
        0x00, 0x00, 0x55,
        0x00, 0x00, 0xAA,
        0x00, 0x00, 0xFF,
        0x00, 0x55, 0x00,
        0x00, 0x55, 0x55,
        0x00, 0x55, 0xAA,
        0x00, 0x55, 0xFF,
        0x00, 0xAA, 0x00,
        0x00, 0xAA, 0x55,
        0x00, 0xAA, 0xAA,
        0x00, 0xAA, 0xFF,
        0x00, 0xFF, 0x00,
        0x00, 0xFF, 0x55,
        0x00, 0xFF, 0xAA,
        0x00, 0xFF, 0xFF,
        0x55, 0x00, 0x00,
        0x55, 0x00, 0x55,
        0x55, 0x00, 0xAA,
        0x55, 0x00, 0xFF,
        0x55, 0x55, 0x00,
        0x55, 0x55, 0x55,
        0x55, 0x55, 0xAA,
        0x55, 0x55, 0xFF,
        0x55, 0xAA, 0x00,
        0x55, 0xAA, 0x55,
        0x55, 0xAA, 0xAA,
        0x55, 0xAA, 0xFF,
        0x55, 0xFF, 0x00,
        0x55, 0xFF, 0x55,
        0x55, 0xFF, 0xAA,
        0x55, 0xFF, 0xFF,
        0xAA, 0x00, 0x00,
        0xAA, 0x00, 0x55,
        0xAA, 0x00, 0xAA,
        0xAA, 0x00, 0xFF,
        0xAA, 0x55, 0x00,
        0xAA, 0x55, 0x55,
        0xAA, 0x55, 0xAA,
        0xAA, 0x55, 0xFF,
        0xAA, 0xAA, 0x00,
        0xAA, 0xAA, 0x55,
        0xAA, 0xAA, 0xAA,
        0xAA, 0xAA, 0xFF,
        0xAA, 0xFF, 0x00,
        0xAA, 0xFF, 0x55,
        0xAA, 0xFF, 0xAA,
        0xAA, 0xFF, 0xFF,
        0xFF, 0x00, 0x00,
        0xFF, 0x00, 0x55,
        0xFF, 0x00, 0xAA,
        0xFF, 0x00, 0xFF,
        0xFF, 0x55, 0x00,
        0xFF, 0x55, 0x55,
        0xFF, 0x55, 0xAA,
        0xFF, 0x55, 0xFF,
        0xFF, 0xAA, 0x00,
        0xFF, 0xAA, 0x55,
        0xFF, 0xAA, 0xAA,
        0xFF, 0xAA, 0xFF,
        0xFF, 0xFF, 0x00,
        0xFF, 0xFF, 0x55,
        0xFF, 0xFF, 0xAA,
        0xFF, 0xFF, 0xFF
    };

};

#endif

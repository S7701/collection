
const uint16_t font[] =
{
   // Zeichensatz ASCII-Zeichen ab 0
   0x80, 0x080, 0x080, 0x080, 0x080, 0x000,   // Balken1 ASCII 0x00
   0xC0, 0x0C0, 0x0C0, 0x0C0, 0x0C0, 0x000,   // Balken2 ASCII 0x01
   0xE0, 0x0E0, 0x0E0, 0x0E0, 0x0E0, 0x000,   // Balken3 ASCII 0x02
   0xF0, 0x0F0, 0x0F0, 0x0F0, 0x0F0, 0x000,   // Balken4 ASCII 0x03
   0xF8, 0x0F8, 0x0F8, 0x0F8, 0x0F8, 0x000,   // Balken5 ASCII 0x04
   0xFC, 0x0FC, 0x0FC, 0x0FC, 0x0FC, 0x000,   // Balken6 ASCII 0x05
   0xFE, 0x0FE, 0x0FE, 0x0FE, 0x0FE, 0x000,   // Balken7 ASCII 0x06
   0xFF, 0x0FF, 0x0FF, 0x0FF, 0x0FF, 0x000,   // Balken8 ASCII 0x07
   0x80, 0x080, 0x080, 0x080, 0x080, 0x000,   // Balken1 ASCII 0x08
   0x00, 0x000, 0x000, 0x000, 0x000, 0x000,   // ASCII 0x09
   0x00, 0x000, 0x000, 0x000, 0x000, 0x000,   // ASCII 0x0A
   0x00, 0x000, 0x000, 0x000, 0x000, 0x000,   // ASCII 0x0B
   0x00, 0x000, 0x000, 0x000, 0x000, 0x000,   // ASCII 0x0C
   0x00, 0x000, 0x000, 0x000, 0x000, 0x000,   // ASCII 0x0D
   0x00, 0x000, 0x000, 0x000, 0x000, 0x000,   // ASCII 0x0E
   0x00, 0x000, 0x000, 0x000, 0x000, 0x000,   // ASCII 0x0F
   0x00, 0x000, 0x000, 0x000, 0x000, 0x000,   // ASCII 0x10
   0x00, 0x000, 0x000, 0x000, 0x000, 0x000,   // ASCII 0x11
   0x00, 0x000, 0x000, 0x000, 0x000, 0x000,   // ASCII 0x12
   0x00, 0x000, 0x000, 0x000, 0x000, 0x000,   // ASCII 0x13
   0x00, 0x000, 0x000, 0x000, 0x000, 0x000,   // ASCII 0x14
   0x00, 0x000, 0x000, 0x000, 0x000, 0x000,   // ASCII 0x15
   0x00, 0x000, 0x000, 0x000, 0x000, 0x000,   // ASCII 0x16
   0x00, 0x000, 0x000, 0x000, 0x000, 0x000,   // ASCII 0x17
   0x00, 0x000, 0x000, 0x000, 0x000, 0x000,   // ASCII 0x18
   0x00, 0x000, 0x000, 0x000, 0x000, 0x000,   // ASCII 0x19
   0x00, 0x000, 0x000, 0x000, 0x000, 0x000,   // ASCII 0x1A
   0x00, 0x000, 0x000, 0x000, 0x000, 0x000,   // ASCII 0x1B
   0x00, 0x000, 0x000, 0x000, 0x000, 0x000,   // ASCII 0x1C
   0x00, 0x000, 0x000, 0x000, 0x000, 0x000,   // ASCII 0x1D
   0x00, 0x000, 0x000, 0x000, 0x000, 0x000,   // ASCII 0x1E
   0x00, 0x000, 0x000, 0x000, 0x000, 0x000,   // ASCII 0x1F
   0x00, 0x000, 0x000, 0x000, 0x000, 0x000,   // Leerzeichen
   0x00, 0x000, 0x04F, 0x000, 0x000, 0x000,   // !
   0x00, 0x007, 0x000, 0x007, 0x000, 0x000,   // "
   0x14, 0x07F, 0x014, 0x07F, 0x014, 0x000,   // #
   0x24, 0x02A, 0x07F, 0x02A, 0x012, 0x000,   // $
   0x23, 0x013, 0x008, 0x064, 0x062, 0x000,   // %
   0x36, 0x049, 0x055, 0x021, 0x050, 0x000,   // &
   0x00, 0x005, 0x003, 0x000, 0x000, 0x000,   // '
   0x00, 0x01C, 0x022, 0x041, 0x000, 0x000,   // (
   0x00, 0x041, 0x022, 0x01C, 0x000, 0x000,   // )
   0x14, 0x008, 0x03E, 0x008, 0x014, 0x000,   // *
   0x08, 0x008, 0x03E, 0x008, 0x008, 0x000,   // +
   0x00, 0x0A0, 0x060, 0x000, 0x000, 0x000,   // ,
   0x08, 0x008, 0x008, 0x008, 0x008, 0x000,   // -
   0x00, 0x060, 0x060, 0x000, 0x000, 0x000,   // .
   0x20, 0x010, 0x008, 0x004, 0x002, 0x000,   // /
   0x3E, 0x051, 0x049, 0x045, 0x03E, 0x000,   // 0
   0x00, 0x042, 0x07F, 0x040, 0x000, 0x000,   // 1
   0x42, 0x061, 0x051, 0x049, 0x046, 0x000,   // 2
   0x21, 0x041, 0x045, 0x047, 0x039, 0x000,   // 3
   0x18, 0x014, 0x012, 0x07F, 0x010, 0x000,   // 4
   0x27, 0x045, 0x045, 0x045, 0x039, 0x000,   // 5
   0x3C, 0x04A, 0x049, 0x049, 0x030, 0x000,   // 6
   0x01, 0x071, 0x009, 0x005, 0x003, 0x000,   // 7
   0x36, 0x049, 0x049, 0x049, 0x036, 0x000,   // 8
   0x06, 0x049, 0x049, 0x029, 0x01E, 0x000,   // 9
   0x00, 0x06C, 0x06C, 0x000, 0x000, 0x000,   // :
   0x00, 0x0AC, 0x06C, 0x000, 0x000, 0x000,   // ;
   0x08, 0x014, 0x022, 0x041, 0x000, 0x000,   // <
   0x14, 0x014, 0x014, 0x014, 0x014, 0x000,   // =
   0x00, 0x041, 0x022, 0x014, 0x008, 0x000,   // >
   0x02, 0x001, 0x051, 0x009, 0x006, 0x000,   // ?
   0x32, 0x049, 0x079, 0x041, 0x03E, 0x000,   // @
   0x7E, 0x009, 0x009, 0x009, 0x07E, 0x000,   // A
   0x7F, 0x049, 0x049, 0x049, 0x036, 0x000,   // B
   0x3E, 0x041, 0x041, 0x041, 0x022, 0x000,   // C
   0x7F, 0x041, 0x041, 0x041, 0x03E, 0x000,   // D
   0x7F, 0x049, 0x049, 0x049, 0x049, 0x000,   // E
   0x7F, 0x009, 0x009, 0x009, 0x009, 0x000,   // F
   0x3E, 0x041, 0x049, 0x049, 0x03A, 0x000,   // G
   0x7F, 0x008, 0x008, 0x008, 0x07F, 0x000,   // H
   0x00, 0x041, 0x07F, 0x041, 0x000, 0x000,   // I
   0x21, 0x041, 0x041, 0x041, 0x03F, 0x000,   // J
   0x7F, 0x008, 0x014, 0x022, 0x041, 0x000,   // K
   0x7F, 0x040, 0x040, 0x040, 0x040, 0x000,   // L
   0x7F, 0x002, 0x004, 0x002, 0x07F, 0x000,   // M
   0x7F, 0x002, 0x004, 0x008, 0x07F, 0x000,   // N
   0x3E, 0x041, 0x041, 0x041, 0x03E, 0x000,   // O
   0x7F, 0x009, 0x009, 0x009, 0x006, 0x000,   // P
   0x3E, 0x041, 0x061, 0x041, 0x0BE, 0x000,   // Q
   0x7F, 0x009, 0x019, 0x029, 0x046, 0x000,   // R
   0x26, 0x049, 0x049, 0x049, 0x032, 0x000,   // S
   0x01, 0x001, 0x07F, 0x001, 0x001, 0x000,   // T
   0x3F, 0x040, 0x040, 0x040, 0x03F, 0x000,   // U
   0x1F, 0x020, 0x040, 0x020, 0x01F, 0x000,   // V
   0x7F, 0x020, 0x010, 0x020, 0x07F, 0x000,   // W
   0x63, 0x014, 0x008, 0x014, 0x063, 0x000,   // X
   0x03, 0x004, 0x078, 0x004, 0x003, 0x000,   // Y
   0x61, 0x051, 0x049, 0x045, 0x043, 0x000,   // Z
   0x00, 0x07F, 0x041, 0x041, 0x000, 0x000,   // [
   0x02, 0x004, 0x008, 0x010, 0x020, 0x000,   // \ Backslash
   0x00, 0x041, 0x041, 0x07F, 0x000, 0x000,   // ]
   0x04, 0x002, 0x001, 0x002, 0x004, 0x000,   // ^
   0x40, 0x040, 0x040, 0x040, 0x040, 0x000,   // _
   0x00, 0x001, 0x002, 0x004, 0x000, 0x000,   // `
   0x20, 0x054, 0x054, 0x054, 0x078, 0x000,   // a
   0x7F, 0x048, 0x044, 0x044, 0x038, 0x000,   // b
   0x38, 0x044, 0x044, 0x044, 0x044, 0x000,   // c
   0x38, 0x044, 0x044, 0x048, 0x07F, 0x000,   // d
   0x38, 0x054, 0x054, 0x054, 0x018, 0x000,   // e
   0x08, 0x07E, 0x009, 0x001, 0x002, 0x000,   // f
   0x18, 0x0A4, 0x0A4, 0x0A4, 0x07C, 0x000,   // g
   0x7F, 0x008, 0x004, 0x004, 0x078, 0x000,   // h
   0x00, 0x044, 0x07D, 0x040, 0x000, 0x000,   // i
   0x20, 0x040, 0x044, 0x03D, 0x000, 0x000,   // j
   0x7F, 0x010, 0x028, 0x044, 0x000, 0x000,   // k
   0x00, 0x041, 0x07F, 0x040, 0x000, 0x000,   // l
   0x7C, 0x004, 0x018, 0x004, 0x078, 0x000,   // m
   0x7C, 0x008, 0x004, 0x004, 0x078, 0x000,   // n
   0x38, 0x044, 0x044, 0x044, 0x038, 0x000,   // o
   0xFC, 0x024, 0x024, 0x024, 0x018, 0x000,   // p
   0x18, 0x024, 0x024, 0x024, 0x0FC, 0x000,   // q
   0x7C, 0x008, 0x004, 0x004, 0x008, 0x000,   // r
   0x48, 0x054, 0x054, 0x054, 0x020, 0x000,   // s
   0x04, 0x07F, 0x044, 0x040, 0x020, 0x000,   // t
   0x3C, 0x040, 0x040, 0x020, 0x07C, 0x000,   // u
   0x1C, 0x020, 0x040, 0x020, 0x01C, 0x000,   // v
   0x3C, 0x040, 0x030, 0x040, 0x03C, 0x000,   // w
   0x44, 0x028, 0x010, 0x028, 0x044, 0x000,   // x
   0x0C, 0x050, 0x050, 0x050, 0x03C, 0x000,   // y
   0x44, 0x064, 0x054, 0x04C, 0x044, 0x000,   // z
   0x00, 0x008, 0x036, 0x041, 0x000, 0x000,   // {
   0x00, 0x000, 0x07F, 0x000, 0x000, 0x000,   // |
   0x00, 0x041, 0x036, 0x008, 0x000, 0x000,   // }
   0x08, 0x004, 0x008, 0x010, 0x008, 0x000,   // ~
   0x00, 0x000, 0x000, 0x000, 0x000, 0x000,   // (ASCII 127)
   // (ab hier nicht genormt)
   0x20, 0x055, 0x054, 0x055, 0x078, 0x000,   // � (ASCII 128)
   0x38, 0x045, 0x044, 0x045, 0x038, 0x000,   // � (ASCII 129)
   0x38, 0x042, 0x040, 0x042, 0x038, 0x000,   // � (ASCII 130)
   0x7C, 0x00B, 0x00A, 0x00B, 0x07C, 0x000,   // � (ASCII 131)
   0x3C, 0x043, 0x042, 0x043, 0x03C, 0x000,   // � (ASCII 132)
   0x3C, 0x041, 0x040, 0x041, 0x03C, 0x000,   // � (ASCII 133)
   0xFE, 0x009, 0x049, 0x049, 0x036, 0x000,   // � (ASCII 134)
   0x06, 0x009, 0x009, 0x006, 0x000, 0x000    // � (ASCII 135)
};


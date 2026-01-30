/* read and write BMP files.
 */

#include "HamClock.h"


static bool we_are_big_endian;                  // set if arch is BE


/* return value of four bytes starting at buf as 32 bit little endian number.
 * we don't assume we can access unaligned 32 bit values nor that we are little endian.
 */
static inline int32_t unpackLE4 (const char buf[4])
{
    union {
        int32_t le4;
        char a[4];
    } le4;

    if (we_are_big_endian) {
        le4.a[0] = buf[3];
        le4.a[1] = buf[2];
        le4.a[2] = buf[1];
        le4.a[3] = buf[0];
    } else
        memcpy (le4.a, buf, 4);

    return (le4.le4);
}

/* return value of two bytes starting at buf as 16 bit little endian number.
 * we don't assume we can access unaligned 32 bit values nor that we are little endian.
 */
static inline int16_t unpackLE2 (const char buf[2])
{
    union {
        int16_t le2;
        char a[2];
    } le2;

    if (we_are_big_endian) {
        le2.a[0] = buf[1];
        le2.a[1] = buf[0];
    } else {
        le2.a[0] = buf[0];
        le2.a[1] = buf[1];
    }

    return (le2.le2);
}

/* set the given 4-byte value at offset within buf, then update offset by 4
 */
static inline void packLE4 (uint8_t *buf, int &offset, int v)
{
    if (debugLevel (DEBUG_BMP, 2))
        Serial.printf ("BMP: LE4 offset %3d value %d\n", offset, v);

    if (we_are_big_endian) {
        offset += 4;
        buf[--offset] = (uint8_t) v;
        buf[--offset] = (uint8_t) (v >>= 8);
        buf[--offset] = (uint8_t) (v >>= 8);
        buf[--offset] = (uint8_t) (v >>= 8);
        offset += 4;
    } else {
        buf[offset++] = (uint8_t) v;
        buf[offset++] = (uint8_t) (v >>= 8);
        buf[offset++] = (uint8_t) (v >>= 8);
        buf[offset++] = (uint8_t) (v >>= 8);
    }
}

/* set the given 2-byte value at offset within buf, then update offset by 2
 */
static inline void packLE2 (uint8_t *buf, int &offset, int v)
{
    if (debugLevel (DEBUG_BMP, 2))
        Serial.printf ("BMP: LE2 offset %3d value %d\n", offset, v);

    if (we_are_big_endian) {
        offset += 2;
        buf[--offset] = (uint8_t) v;
        buf[--offset] = (uint8_t) (v >>= 8);
        offset += 2;
    } else {
        buf[offset++] = (uint8_t) v;
        buf[offset++] = (uint8_t) (v >>= 8);
    }
}

/* read next two bytes as int
 */
static inline bool read2ByteInt (GenReader &gr, int &byte_os, int &v)
{
    char i2[2];
    if (!gr.getChar(&i2[0]) || !gr.getChar(&i2[1]))
        return (false);
    byte_os += 2;
    v = unpackLE2 (i2);
    return (true);
}

/* read next four bytes as int
 */
static inline bool read4ByteInt (GenReader &gr, int &byte_os, int &v)
{
    char i4[4];
    if (!gr.getChar(&i4[0]) || !gr.getChar(&i4[1]) || !gr.getChar(&i4[2]) || !gr.getChar(&i4[3]))
        return (false);
    byte_os += 4;
    v = unpackLE4 (i4);
    return (true);
}

/* read more from gr until byte_to equals to
 */
static inline bool skipGRToPos (GenReader &gr, int &byte_os, int to)
{
    char c;
    while (byte_os < to) {
        if (!gr.getChar(&c)) {
            return (false);
        }
        byte_os++;
    }
    return (true);
}

/* return whether we are big-endian architecture
 */
static inline bool determineBigEndian (void)
{
    union {
        uint16_t e2;
        uint8_t a[2];
    } e2;

    e2.e2 = 1;
    bool be = e2.a[1] == 1;
    if (debugLevel (DEBUG_BMP, 2))
        Serial.printf ("BMP: we are %s-endian\n", be ? "big" : "lil");
    return (be);
}


/* read the next 24 bpp pixel from gr as RGB565 pixel
 */
static inline bool next24RGB565 (GenReader &gr, uint16_t *pix_p)
{
    char b, g, r;                                               // note order
    if (!gr.getChar (&b) || !gr.getChar (&g) || !gr.getChar (&r))
        return (false);
    *pix_p = RGB565((uint8_t)r,(uint8_t)g,(uint8_t)b);
    return (true);
}

/* read the next 16 bpp pixel from gr as RGB565 pixel
 */
static inline bool next16RGB565 (GenReader &gr, uint16_t *pix_p)
{
    int byte_os = 0;                                            // unused
    int p;
    if (!read2ByteInt (gr, byte_os, p))
        return (false);
    *pix_p = (uint16_t)p;
    return (true);
}


/* read n_pix from gr to the given array.
 * return ok else why not.
 */
static bool copyBMPperfectFit (GenReader &gr, uint16_t *&pix_565, int n_pix, Message &ynot)
{
    uint16_t *pix = pix_565, *pix_end = pix + n_pix;
    while (pix < pix_end) {
        if (!next16RGB565 (gr, pix++)) {
            ynot.printf ("pixels are short %d < %d", (int)(pix - pix_565), n_pix);
            return (false);
        }
    }
    return (true);
}

/* read image converting to RGB565 pixels laid out top-to-bottom and inverting rows if img_h > 0.
 * return ok else reason in ynot.
 */
static bool read565TB (GenReader &gr, uint16_t *img_565, int img_w, int img_h, int img_bpp, int img_pad,
Message &ynot)
{
    // time
    struct timeval tv0;
    if (debugLevel(DEBUG_BMP, 1))
        gettimeofday (&tv0, NULL);

    // img_h > 0 means image stores pixels bottom-to-top so we must flip top-to-bottom
    int pix_row = img_h > 0 ? img_h-1 : 0;                      // starting row
    const int row_del = img_h > 0 ? -1 : 1;                     // row increment

    // now just work with positive rows
    img_h = abs(img_h);

    if (img_bpp == 16) {
        // one image pixel is every 2 bytes plus any row padding
        for (int img_y = 0; img_y < img_h; img_y++) {
            // start next row
            int pix_i = pix_row * img_w;
            for (int img_x = 0; img_x < img_w; img_x++) {
                if (!next16RGB565 (gr, &img_565[pix_i++])) {
                    ynot.printf ("pixels are short < %d", img_w * img_h);
                    return (false);
                }
            }
            // discard img padding
            for (int pad = 0; pad < img_pad; pad++) {
                char pad_c;
                (void) gr.getChar(&pad_c);
            }
            // advance to next row
            pix_row += row_del;
        }
    } else {
        // one image pixel is every 3 bytes plus any row padding
        for (int img_y = 0; img_y < img_h; img_y++) {
            // start next row
            int pix_i = pix_row * img_w;
            for (int img_x = 0; img_x < img_w; img_x++) {
                if (!next24RGB565 (gr, &img_565[pix_i++])) {
                    ynot.printf ("pixels are short < %d", img_w * img_h);
                    return (false);
                }
            }
            // discard img padding
            for (int pad = 0; pad < img_pad; pad++) {
                char pad_c;
                (void) gr.getChar(&pad_c);
            }
            // advance to next row
            pix_row += row_del;
        }
    }

    if (debugLevel(DEBUG_BMP, 1)) {
        struct timeval tv1;
        gettimeofday (&tv1, NULL);
        Serial.printf ("BMP: read %d x %d x %d time %ld us\n", img_w, img_h, img_bpp, (long)TVDELUS(tv0,tv1));
    }

    // ok
    return (true);
}

/* copy img_565 with dimensions img_w/h to box_565 with the given box dimensions so as to
 * expand the image to exactly fill the box.
 * all pixelsa are RGB565 uint16_t
 */
static void fillU16Image (const uint16_t *img_565, int img_w, int img_h, uint16_t *box_565, const SBox &box)
{
    for (int box_dy = 0; box_dy < box.h; box_dy++) {
        int img_y = box_dy * img_h / box.h;
        for (int box_dx = 0; box_dx < box.w; box_dx++) {
            int img_x = box_dx * img_w / box.w;
            box_565[(box.y+box_dy)*box.w + (box.x+box_dx)] = img_565[img_y*img_w + img_x];
        }
    }
}



/* copy img_565 with dimensions img_w/h to box_565 with the given box dimensions so as to
 * resize the image AMAP while maintaining its aspect ratio.
 * all pixelsa are RGB565 uint16_t
 */
static void resizeU16Image (const uint16_t *img_565, int img_w, int img_h, uint16_t *box_565, const SBox &box)
{
    // time
    struct timeval tv0;
    if (debugLevel(DEBUG_BMP, 1))
        gettimeofday (&tv0, NULL);

    // black
    int n_box_bytes = (int)box.w * (int)box.h * sizeof(uint16_t);
    memset (box_565, 0, n_box_bytes);

    if (img_w > img_h * box.w / box.h) {
        // image aspect is wider than box aspect: full width and center vertically
        if (debugLevel (DEBUG_BMP, 1))
            Serial.printf ("BMP: img wider aspect: img %d x %d box %d x %d\n", img_w, img_h, box.w, box.h);
        int box_v_h = box.w * img_h / img_w;                    // visible height
        int box_v_gap = (box.h - box_v_h)/2;                    // vertical gap on top and bottom
        for (int box_dy = 0; box_dy < box_v_h; box_dy++) {      // scan y through visible portion of box
            int img_y = box_dy * img_h / box_v_h;               // closest image y coord
            for (int box_dx = 0; box_dx < box.w; box_dx++) {    // scan x full width
                int img_x = box_dx * img_w / box.w;             // closest image x coord
                box_565[(box_dy+box_v_gap)*box.w + box_dx] = img_565[img_y*img_w + img_x];
            }
        }

    } else if (img_h > img_w * box.h / box.w) {
        // image aspect is taller than box aspect: full height and center horizontally
        if (debugLevel (DEBUG_BMP, 1))
            Serial.printf ("BMP: img taller aspect: img %d x %d box %d x %d\n", img_w, img_h, box.w, box.h);
        int box_v_w = box.h * img_w / img_h;                    // visible width
        int box_h_gap = (box.w - box_v_w)/2;                    // horizontal gap on each side
        for (int box_dy = 0; box_dy < box.h; box_dy++) {        // scan y full height
            int img_y = box_dy * img_h / box.h;                 // closest image y coord
            for (int box_dx = 0; box_dx < box_v_w; box_dx++) {  // scan across vis portion
                int img_x = box_dx * img_w / box_v_w;           // closest image x coord
                box_565[box_dy*box.w + (box_dx+box_h_gap)] = img_565[img_y*img_w + img_x];
            }
        }

    } else {
        // aspect ratios match: no gaps
        if (debugLevel (DEBUG_BMP, 1))
            Serial.printf ("BMP: equal aspect: img %d x %d box %d x %d\n", img_w, img_h, box.w, box.h);
        for (int box_dy = 0; box_dy < box.h; box_dy++) {        // scan y full height
            int img_y = box_dy * img_h / box.h;                 // closest image y coord
            for (int box_dx = 0; box_dx < box.w; box_dx++) {    // scan x full width
                int img_x = box_dx * img_w / box.w;             // closest image x coord
                box_565[box_dy*box.w + box_dx] = img_565[img_y*img_w + img_x];
            }
        }
    }

    if (debugLevel(DEBUG_BMP, 1)) {
        struct timeval tv1;
        gettimeofday (&tv1, NULL);
        Serial.printf ("BMP: resize time %ld us\n", (long)TVDELUS(tv0,tv1));
    }
}

/* copy img_565 with dimensions img_w/h to box_565 with the given box dimensions so as to
 * crop and center the image into the box without changing its pixel density.
 * all pixelsa are RGB565 uint16_t
 */
static void cropU16Image (const uint16_t *img_565, int img_w, int img_h, uint16_t *box_565, const SBox &box)
{
    // time
    struct timeval tv0;
    if (debugLevel(DEBUG_BMP, 1))
        gettimeofday (&tv0, NULL);

    int n_box_bytes = (int)box.w * (int)box.h * sizeof(uint16_t);

    if (img_w == box.w && img_h == box.h) {
        // exact size match
        memcpy (box_565, img_565, n_box_bytes);

    } else if (img_w > box.w && img_h > box.h) {
        // image is larger than box: use center portion
        int img_lgap = (img_w - box.w)/2;                       // image left gap
        int img_tgap = (img_h - box.h)/2;                       // image top gap
        for (int dy = 0; dy < box.h; dy++)                      // box dy
            for (int dx = 0; dx < box.w; dx++)                  // box dx
                box_565[dy*box.w + dx] = img_565[(img_tgap+dy)*img_w + (img_lgap+dx)];

    } else if (img_w < box.w && img_h < box.h) {
        // image is smaller than box: center within box
        memset (box_565, 0, n_box_bytes);                       // init box_565 black
        int box_lgap = (box.w - img_w)/2;                       // box left gap
        int box_tgap = (box.h - img_h)/2;                       // box top gap
        for (int dy = 0; dy < img_h; dy++)                      // image dy
            for (int dx = 0; dx < img_w; dx++)                  // image dx
                box_565[(box_tgap+dy)*box.w + (box_lgap+dx)] = img_565[dy*img_w + dx];

    } else if (img_w < box.w && img_h > box.h) {
        // image is narrower but taller than box: show full img width centered horizontally
        memset (box_565, 0, n_box_bytes);                       // init box_565 black
        int box_lgap = (box.w - img_w)/2;                       // box left gap
        int img_tgap = (img_h - box.h)/2;                       // image top gap
        for (int dy = 0; dy < box.h; dy++)                      // box dy
            for (int dx = 0; dx < img_w; dx++)                  // image dx
                box_565[dy*box.w + (box_lgap+dx)] = img_565[(img_tgap+dy)*img_w + dx];

    } else if (img_w > box.w && img_h < box.h) {
        // image is wider but shorter than box: show full img height centered vertically
        memset (box_565, 0, n_box_bytes);                       // init box_565 black
        int box_tgap = (box.h - img_h)/2;                       // box top gap
        int img_lgap = (img_w - box.w)/2;                       // image left gap
        for (int dy = 0; dy < img_h; dy++)                      // image dy
            for (int dx = 0; dx < box.w; dx++)                  // box dx
                box_565[(box_tgap+dy)*box.w + dx] = img_565[dy*img_w + (img_lgap+dx)];

    } else {
        fatalError ("cropU16Image bad overlap img %d x %d box %d x %d", img_w, img_h, box.w, box.h);
    }

    if (debugLevel(DEBUG_BMP, 1)) {
        struct timeval tv1;
        gettimeofday (&tv1, NULL);
        Serial.printf ("BMP: crop time %ld us\n", (long)TVDELUS(tv0,tv1));
    }
}

/* read the header from the given BMP.
 *   img_w    n pixels wide
 *   img_h    n pixels height, <0 means first pixel is at upper left, else lower left
 *   img_bpp  bits per pixel, only 16 and 24 are supported
 *   img_pad  extra bytes per row if necessary so row is always mult of 4 bytes long
 * if ok return true with gr positioned at start of pixels, else false with excuse in ynot.
 */
bool readBMPHeader (GenReader &gr, int &img_w, int &img_h, int &img_bpp, int &img_pad, Message &ynot)
{
    // set endian
    we_are_big_endian = determineBigEndian();

    // size of initial header common to all formats and size of original BMP subsequent header
    #define COMMONHEADER        14
    #define BITMAPCOREHEADER    12

    // walking offset
    int byte_os = 0;


    // read first two bytes to confirm correct format
    char c;
    if (!gr.getChar(&c) || c != 'B' || !gr.getChar(&c) || c != 'M') {
        ynot.set ("File not BMP");
        return (false);
    }
    byte_os += 2;


    // next 4 bytes are total file size -- use for sanity check once we know the dimensions and bpp
    int file_size;
    if (!read4ByteInt (gr, byte_os, file_size)) {
        ynot.set ("can not read file size");
        return (false);
    }


    // skip down to byte 10 to the pixels offset
    int pix_start;
    if (!skipGRToPos (gr, byte_os, 10)) {
        ynot.set ("Too short to find pixels");
        return (false);
    }
    if (!read4ByteInt (gr, byte_os, pix_start)) {
        ynot.set ("can not read pix start");
        return (false);
    }


    // next 4 bytes are size of this sub-header -- determines header type
    int dib_size;
    if (!read4ByteInt (gr, byte_os, dib_size)) {
        ynot.set ("can not read hdr size");
        return (false);
    }


    // get image dimensions
    if (dib_size == BITMAPCOREHEADER) {

        // only this original header uses 2-byte sizes

        if (!read2ByteInt (gr, byte_os, img_w)) {
            ynot.set ("can not read width");
            return (false);
        }
        if (!read2ByteInt (gr, byte_os, img_h)) {
            ynot.set ("can not read height");
            return (false);
        }

    } else {

        if (!read4ByteInt (gr, byte_os, img_w)) {
            ynot.set ("can not read width");
            return (false);
        }
        if (!read4ByteInt (gr, byte_os, img_h)) {
            ynot.set ("can not read height");
            return (false);
        }

    }


    // next is number of color planes -- must be 1
    int n_planes;
    if (!read2ByteInt (gr, byte_os, n_planes)) {
        ynot.set ("can not read n planes");
        return (false);
    }
    if (n_planes != 1) {
        ynot.printf ("N planes %d != 1", n_planes);
        return (false);
    }


    // next is bits per pixel -- must be 16 or 24
    if (!read2ByteInt (gr, byte_os, img_bpp)) {
        ynot.set ("can not read bpp");
        return (false);
    }
    if (img_bpp != 16 && img_bpp != 24) {
        ynot.printf ("BPP %d not 16 or 24", img_bpp);
        return (false);
    }


    // now we can check for consistency
    int img_bpr = 4*((img_bpp*img_w+31)/32);              // pad row length to mult of 4
    img_pad = img_bpr - img_w * img_bpp/8;
    if (file_size != pix_start + img_bpr*abs(img_h)) {
        ynot.set ("broken file format");
        Serial.printf ("BMP: broken dib_size= %d file_size= %d pix_start= %d w= %d h= %d bpp= %d pad= %d\n",
                             dib_size, file_size, pix_start, img_w, img_h, img_bpp, img_pad);
        return (false);
    }


    // next is compression method unless original header -- must be 0 or 3
    if (dib_size != BITMAPCOREHEADER) {
        int comp;
        if (!read4ByteInt (gr, byte_os, comp)) {
            ynot.set ("can not read compression");
            return (false);
        }
        if (comp != 0 && comp != 3) {
            ynot.printf ("unsupported compression %d", comp);
            return (false);
        }
    }


    // skip down to start of pixels
    if (!skipGRToPos (gr, byte_os, pix_start)) {
        ynot.set ("can not find pixels");
        return (false);
    }


    // ok: gr is now at start of first pixel
    return (true);
}

/* read any BMP file and pass back a malloced array of RGB565 pixels ready for box, else why not.
 * N.B. images with odd width will have last column truncated.
 * N.B. if we return true then caller must free box_565
 */
bool readBMPImage (GenReader &gr, const SBox &box, uint16_t *&box_565, ImageRefit fit, Message &ynot)
{
    // set endian
    we_are_big_endian = determineBigEndian();

    // get size info and position gr at first pixel
    int img_w, img_h, img_bpp, img_pad;
    if (!readBMPHeader (gr, img_w, img_h, img_bpp, img_pad, ynot))
        return (false);

    // discard last column if width is odd because we don't want our 565 to have padding
    if (img_w & 1) {
        img_w -= 1;
        img_pad += img_bpp/8;
    }

    // get memory for box
    const int n_box_pix = (int)box.w * (int)box.h;
    const int n_box_bytes = n_box_pix * sizeof(uint16_t);
    box_565 = (uint16_t *) malloc (n_box_bytes);                // N.B. free if we fail
    if (!box_565)
        fatalError ("no mem for %d x %d BMP box", box.w, box.h);

    // handle a perfect fit as a fast special case -- recall img_h < 0 means pixels are top-to-bottom
    if (img_w == box.w && -img_h == box.h && img_bpp == 16) {
        bool ok = copyBMPperfectFit (gr, box_565, n_box_pix, ynot);
        if (!ok)
            free (box_565);
        return (ok);
    }

    // get memory for reading RGB565 image
    const int n_img_pix = img_w * abs(img_h);
    const int n_img_bytes = n_img_pix * sizeof(uint16_t);
    StackMalloc new_mem(n_img_bytes);
    uint16_t *img_565 = (uint16_t *) new_mem.getMem();
    if (!img_565)
        fatalError ("no mem for %d x %d BMP image", img_w, img_h);

    // read image converting to RGB565 pixels, inverting rows if img_h > 0
    if (!read565TB (gr, img_565, img_w, img_h, img_bpp, img_pad, ynot)) {
        free (box_565);
        return (false);
    }

    // fit img to box using desired method
    switch (fit) {
    case FIT_CROP:
        cropU16Image (img_565, img_w, abs(img_h), box_565, box);
        break;
    case FIT_RESIZE:
        resizeU16Image (img_565, img_w, abs(img_h), box_565, box);
        break;
    case FIT_FILL:
        fillU16Image (img_565, img_w, abs(img_h), box_565, box);
        break;
    default:
        fatalError ("readBMPImage bogus fit %d", (int)fit);
        break;
    }

    // ok!
    return (true);
}

/* create a new malloced BMP header for RGB565 pixels of the given dimensions, including key metrics.
 * N.B. header will specify pixels are stored top-to-bottom.
 * N.B. only even img_w allowed to insure not padding
 * N.B. caller must free hdr if we return true
 * see https://www.fileformat.info/format/bmp/egff.htm for info on CSType field
 */
bool createBMP565Header (uint8_t *&hdr, int &hdr_len, int &file_bytes, int img_w, int img_h)
{
    // w must be even
    if (img_w & 1) {
        Serial.printf ("BMP createBMP565Header called with unsupported odd width %d\n", img_w);
        return (false);
    }

    // basic alloc
    const int core_size = 14;                                   // common to all types
    const int dib_size = 108;                                   // BITMAPV4HEADER, first to allow 565 masks
    hdr_len = core_size + dib_size;
    hdr = (uint8_t *) calloc (hdr_len, sizeof(uint8_t));
    if (!hdr)
        fatalError ("BMP: createBMP565Header failed to allocate %d", hdr_len);

    // determine sizes
    const int bpp = 16;                                         // bits per RGB565 pixel
    int row_bytes = 4*((bpp*img_w+31)/32);                      // bytes per row, with padding
    int pix_bytes = row_bytes*img_h;                            // bytes of pixel data
    file_bytes = hdr_len + pix_bytes;                           // overall file size

    if (debugLevel (DEBUG_BMP, 1))
        Serial.printf ("BMP: createBMP565Header %d x %d: row_bytes %d file_bytes %d\n", img_w, img_h,
                            row_bytes, file_bytes);

    // fill in the common header -- https://en.wikipedia.org/wiki/BMP_file_format
    hdr[0] = 'B';                                               // manditory preamble
    hdr[1] = 'M';
    int offset = 2;                                             // walking header byte offset
    packLE4 (hdr, offset, file_bytes);                          // file size
    packLE2 (hdr, offset, 0);                                   // reserved
    packLE2 (hdr, offset, 0);                                   // reserved
    packLE4 (hdr, offset, hdr_len);                             // offset to start of pixels

    // fill in BITMAPV4HEADER
    packLE4 (hdr, offset, dib_size);                            // size of this dib
    packLE4 (hdr, offset, img_w);                               // pixels wide
    packLE4 (hdr, offset, -img_h);                              // pixels high, <0 to display top-down
    packLE2 (hdr, offset, 1);                                   // number of color planes
    packLE2 (hdr, offset, 16);                                  // number of bits per pixel
    packLE4 (hdr, offset, 3);                                   // use bitmaks but no compression
    packLE4 (hdr, offset, pix_bytes);                           // n pixel bytes
    packLE4 (hdr, offset, 0);                                   // h printer resolution
    packLE4 (hdr, offset, 0);                                   // v printer resolution
    packLE4 (hdr, offset, 0);                                   // n colors in palette
    packLE4 (hdr, offset, 0);                                   // n important colors !
    packLE4 (hdr, offset, 0xF800);                              // red mask
    packLE4 (hdr, offset, 0x07E0);                              // green mask
    packLE4 (hdr, offset, 0x001F);                              // blue mask
    packLE4 (hdr, offset, 0x0000);                              // alpha mask
    packLE4 (hdr, offset, 1);                                   // CSType: 1 means ignore all remaining!
    packLE4 (hdr, offset, 0);                                   // RedX
    packLE4 (hdr, offset, 0);                                   // RedY
    packLE4 (hdr, offset, 0);                                   // RedZ
    packLE4 (hdr, offset, 0);                                   // GreenX
    packLE4 (hdr, offset, 0);                                   // GreenY
    packLE4 (hdr, offset, 0);                                   // GreenZ
    packLE4 (hdr, offset, 0);                                   // BlueX
    packLE4 (hdr, offset, 0);                                   // BlueY
    packLE4 (hdr, offset, 0);                                   // BlueZ
    packLE4 (hdr, offset, 0);                                   // GammaRed
    packLE4 (hdr, offset, 0);                                   // GammaGreen
    packLE4 (hdr, offset, 0);                                   // GammaBlue

    // sanity check
    if (offset != hdr_len)
        fatalError ("createBMP565Header bogus header %d != %d", offset, hdr_len);

    // good!
    return (true);
}

/* write the given RGB565 pixels to a new file in our working directory.
 * N.B. pixels are assumed even n columns and arranged such that first is in upper left corner.
 * return ok or why not.
 */
bool writeBMP565File (const char *filename, uint16_t *&pix_565, int img_w, int img_h, Message &ynot)
{
    if (debugLevel (DEBUG_BMP, 1))
        Serial.printf ("BMP: writing %s %d x %d\n", filename, img_w, img_h);

    // create file. N.B. fclose!
    FILE *fp = fopenOurs (filename, "w");
    if (!fp) {
        ynot.printf ("%s: %s", filename, strerror(errno));
        return (false);
    }

    // create header
    uint8_t *hdr;
    int hdr_len, n_bytes;
    createBMP565Header (hdr, hdr_len, n_bytes, img_w, img_h);

    // write header then pixels
    (void) fwrite (hdr, 1, hdr_len, fp);
    (void) fwrite (pix_565, 1, img_w*img_h*2, fp);

    // finished with header
    free (hdr);

    // get status before closing
    bool ok = !ferror(fp);
    fclose (fp);

    if (!ok)
        ynot.printf ("%s write failed: %s", filename, strerror(errno));

    return (ok);
}

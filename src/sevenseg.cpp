/* support for drawing 7-segment digits and a few characters using tft drawing or direct img array.
 */

#include "HamClock.h"


// handy image array segment drawers

#define ImgHLine(x,y,w) for(int i = 0; i < (w); i++) \
                     memcpy (&img[(y)*LIVE_BYPPIX*BUILD_W + ((x)+i)*LIVE_BYPPIX], txt_clr, LIVE_BYPPIX)

#define ImgVLine(x,y,h) for(int i = 0; i < (h); i++) \
                     memcpy (&img[((y)+i)*LIVE_BYPPIX*BUILD_W + (x)*LIVE_BYPPIX], txt_clr, LIVE_BYPPIX)

#define ImgForwardslash(x,y,h) for(int i = 0; i < (h); i++) \
                 memcpy (&img[((y)+(h)-1-i)*LIVE_BYPPIX*BUILD_W + ((x)+i)*LIVE_BYPPIX], txt_clr, LIVE_BYPPIX)

#define ImgBackslash(x,y,h) for(int i = 0; i < (h); i++) \
                 memcpy (&img[((y)+i)*LIVE_BYPPIX*BUILD_W + ((x)+i)*LIVE_BYPPIX], txt_clr, LIVE_BYPPIX)


/* draw a digit in the given frame buffer of size and location given by the given box.
 */
void drawImgDigit (unsigned digit, uint8_t *img, const SBox &b, const uint8_t txt_clr[LIVE_BYPPIX])
{

    switch (digit) {

    #if BUILD_W == 800
        // only room for square corners

        case 0:
            ImgHLine(b.x, b.y, b.w);
            ImgVLine(b.x, b.y, b.h);
            ImgVLine(b.x+b.w-1, b.y, b.h);
            ImgHLine(b.x, b.y+b.h-1, b.w);
            break;
        case 1:
            ImgHLine(b.x, b.y, b.w/2);
            ImgVLine(b.x+b.w/2, b.y, b.h);
            ImgHLine(b.x, b.y+b.h-1, b.w);
            break;
        case 2:
            ImgHLine(b.x, b.y, b.w);
            ImgVLine(b.x+b.w-1, b.y, b.h/2);
            ImgHLine(b.x, b.y+b.h/2, b.w);
            ImgVLine(b.x, b.y+b.h/2, b.h/2);
            ImgHLine(b.x, b.y+b.h-1, b.w);
            break;
        case 3:
            ImgHLine(b.x, b.y, b.w);
            ImgVLine(b.x+b.w-1, b.y, b.h);
            ImgHLine(b.x, b.y+b.h/2, b.w);
            ImgHLine(b.x, b.y+b.h-1, b.w);
            break;
        case 4:
            ImgVLine(b.x, b.y, b.h/2);
            ImgVLine(b.x+b.w-1, b.y, b.h);
            ImgHLine(b.x, b.y+b.h/2, b.w);
            break;
        case 5:
            ImgHLine(b.x, b.y, b.w);
            ImgVLine(b.x, b.y, b.h/2);
            ImgHLine(b.x, b.y+b.h/2, b.w);
            ImgVLine(b.x+b.w-1, b.y+b.h/2, b.h/2);
            ImgHLine(b.x, b.y+b.h-1, b.w);
            break;
        case 6:
            ImgHLine(b.x, b.y, b.w);
            ImgVLine(b.x, b.y, b.h);
            ImgHLine(b.x, b.y+b.h/2, b.w);
            ImgVLine(b.x+b.w-1, b.y+b.h/2, b.h/2);
            ImgHLine(b.x, b.y+b.h-1, b.w);
            break;
        case 7:
            ImgHLine(b.x, b.y, b.w);
            ImgVLine(b.x+b.w-1, b.y, b.h);
            break;
        case 8:
            ImgHLine(b.x, b.y, b.w);
            ImgVLine(b.x, b.y, b.h);
            ImgVLine(b.x+b.w-1, b.y, b.h);
            ImgHLine(b.x, b.y+b.h/2, b.w);
            ImgHLine(b.x, b.y+b.h-1, b.w);
            break;
        case 9:
            ImgHLine(b.x, b.y, b.w);
            ImgVLine(b.x, b.y, b.h/2);
            ImgHLine(b.x, b.y+b.h/2, b.w);
            ImgVLine(b.x+b.w-1, b.y, b.h);
            ImgHLine(b.x, b.y+b.h-1, b.w);
            break;

    #else
        // BUILD_W != 800 has room for rounded corners

        case 0:
            ImgHLine(b.x+1, b.y, b.w-2);
            ImgVLine(b.x, b.y+1, b.h-2);
            ImgVLine(b.x+b.w-1, b.y+1, b.h-2);
            ImgHLine(b.x+1, b.y+b.h-1, b.w-2);
            ImgForwardslash(b.x, b.y+b.h/4, b.w);
            break;
        case 1:
            ImgHLine(b.x+1, b.y, b.w/2-1);
            ImgVLine(b.x+b.w/2, b.y, b.h);
            ImgHLine(b.x, b.y+b.h-1, b.w);
            break;
        case 2:
            ImgHLine(b.x, b.y, b.w-1);
            ImgVLine(b.x+b.w-1, b.y+1, b.h/2-1);
            ImgHLine(b.x+1, b.y+b.h/2, b.w-2);
            ImgVLine(b.x, b.y+b.h/2+1, b.h/2-2);
            ImgHLine(b.x+1, b.y+b.h-1, b.w-1);
            break;
        case 3:
            ImgHLine(b.x, b.y, b.w-1);
            ImgVLine(b.x+b.w-1, b.y+1, b.h-2);
            ImgHLine(b.x, b.y+b.h/2, b.w);
            ImgHLine(b.x, b.y+b.h-1, b.w-1);
            break;
        case 4:
            ImgVLine(b.x, b.y, b.h/2);
            ImgVLine(b.x+b.w-1, b.y, b.h);
            ImgHLine(b.x+1, b.y+b.h/2, b.w-1);
            break;
        case 5:
            ImgHLine(b.x+1, b.y, b.w-1);
            ImgVLine(b.x, b.y+1, b.h/2-1);
            ImgHLine(b.x+1, b.y+b.h/2, b.w-1);
            ImgVLine(b.x+b.w-1, b.y+b.h/2+1, b.h/2-1);
            ImgHLine(b.x, b.y+b.h-1, b.w-1);
            break;
        case 6:
            ImgHLine(b.x+1, b.y, b.w-1);
            ImgVLine(b.x, b.y+1, b.h-2);
            ImgHLine(b.x+1, b.y+b.h/2, b.w-2);
            ImgVLine(b.x+b.w-1, b.y+b.h/2+1, b.h/2-1);
            ImgHLine(b.x+1, b.y+b.h-1, b.w-2);
            break;
        case 7:
            ImgHLine(b.x, b.y, b.w-1);
            ImgVLine(b.x+b.w-1, b.y+1, b.h-1);
            break;
        case 8:
            ImgHLine(b.x+1, b.y, b.w-2);
            ImgVLine(b.x, b.y+1, b.h-2);
            ImgVLine(b.x+b.w-1, b.y+1, b.h-2);
            ImgHLine(b.x, b.y+b.h/2, b.w);
            ImgHLine(b.x+1, b.y+b.h-1, b.w-2);
            break;
        case 9:
            ImgHLine(b.x+1, b.y, b.w-2);
            ImgVLine(b.x, b.y+1, b.h/2-1);
            ImgHLine(b.x+1, b.y+b.h/2, b.w-1);
            ImgVLine(b.x+b.w-1, b.y+1, b.h-2);
            ImgHLine(b.x, b.y+b.h-1, b.w-1);
            break;

    #endif // BUILD_W == 800
    }
}

void drawImgNumber (unsigned n, uint8_t *img, SBox &b, const uint8_t txt_clr[LIVE_BYPPIX])
{
    int n10 = n/10;
    if (n10)
        drawImgNumber (n10, img, b, txt_clr);
    drawImgDigit (n%10, img, b, txt_clr);
    b.x += 3*b.w/2;
}

void drawImgR (uint8_t *img, SBox b, const uint8_t txt_clr[LIVE_BYPPIX])
{
    #if BUILD_W == 800

        // square corners

        ImgHLine(b.x, b.y, b.w);
        ImgVLine(b.x, b.y, b.h);
        ImgVLine(b.x+b.w-1, b.y, b.h/2);
        ImgHLine(b.x, b.y+b.h/2, b.w);
        ImgBackslash(b.x, b.y+b.h/2, b.h/2+1);

    #else

        // room for rounded corners

        ImgBackslash(b.x, b.y+b.h/2, b.h/2);
        ImgHLine(b.x, b.y, b.w-1);
        ImgVLine(b.x, b.y+1, b.h-1);
        ImgVLine(b.x+b.w-1, b.y+1, b.h/2-1);
        ImgHLine(b.x+1, b.y+b.h/2, b.w-2);
        ImgBackslash(b.x, b.y+b.h/2-1, b.h/2+1);

    #endif
}

void drawImgO (uint8_t *img, SBox b, const uint8_t txt_clr[LIVE_BYPPIX])
{
    #if BUILD_W == 800

        // square corners

        ImgHLine(b.x, b.y, b.w);
        ImgVLine(b.x, b.y, b.h);
        ImgVLine(b.x+b.w-1, b.y, b.h);
        ImgHLine(b.x, b.y+b.h-1, b.w);

    #else

        // room for rounded corners

        ImgHLine(b.x+1, b.y, b.w-2);
        ImgVLine(b.x, b.y+1, b.h-2);
        ImgVLine(b.x+b.w-1, b.y+1, b.h-2);
        ImgHLine(b.x+1, b.y+b.h-1, b.w-2);
    #endif
}

void drawImgW (uint8_t *img, SBox b, const uint8_t txt_clr[LIVE_BYPPIX])
{
    #if BUILD_W == 800

        // extra wide for center line

        ImgVLine(b.x, b.y, b.h);
        ImgVLine(b.x+b.w/2+1, b.y+b.h/2, b.h/2);
        ImgVLine(b.x+b.w+1, b.y, b.h);
        ImgHLine(b.x, b.y+b.h-1, b.w+2);

    #else

        // room for rounded corners

        ImgVLine(b.x, b.y, b.h-1);
        ImgVLine(b.x+b.w/2, b.y+b.h/2, b.h/2);
        ImgVLine(b.x+b.w, b.y, b.h-1);
        ImgHLine(b.x+1, b.y+b.h-1, b.w-1);

    #endif
}

/* draw the given digit in the given bounding box with the given line thickness.
 */
void drawDigit (const SBox &b, int digit, uint16_t lt, uint16_t bg, uint16_t fg)
{
    // insure all dimensions are even so two halves make a whole
    uint16_t t = (lt+1) & ~1U;
    uint16_t w = (b.w+1) & ~1U;
    uint16_t h = (b.h+1) & ~1U;
    uint16_t to2 = t/2;
    uint16_t wo2 = w/2;
    uint16_t ho2 = h/2;

    // erase 
    tft.fillRect (b.x, b.y, w, h, bg);

    // draw digit -- replace with drawRect to check boundaries
    switch (digit) {
    case 0:
        tft.fillRect (b.x, b.y, w, t, fg);
        tft.fillRect (b.x, b.y+t, t, h-2*t, fg);
        tft.fillRect (b.x, b.y+h-t, w, t, fg);
        tft.fillRect (b.x+w-t, b.y+t, t, h-2*t, fg);
        break;
    case 1:
        tft.fillRect (b.x+wo2-to2, b.y, t, h, fg);      // center column? a little right?
        break;
    case 2:
        tft.fillRect (b.x, b.y, w, t, fg);
        tft.fillRect (b.x+w-t, b.y+t, t, ho2-t-to2, fg);
        tft.fillRect (b.x, b.y+ho2-to2, w, t, fg);
        tft.fillRect (b.x, b.y+ho2+to2, t, ho2-t-to2, fg);
        tft.fillRect (b.x, b.y+h-t, w, t, fg);
        break;
    case 3:
        tft.fillRect (b.x, b.y, w, t, fg);
        tft.fillRect (b.x+t, b.y+ho2-to2, w-t, t, fg);
        tft.fillRect (b.x, b.y+h-t, w, t, fg);
        tft.fillRect (b.x+w-t, b.y+t, t, h-2*t, fg);
        break;
    case 4:
        tft.fillRect (b.x, b.y, t, ho2+to2, fg);
        tft.fillRect (b.x+t, b.y+ho2-to2, w-2*t, t, fg);
        tft.fillRect (b.x+w-t, b.y, t, h, fg);
        break;
    case 5:
        tft.fillRect (b.x, b.y, w, t, fg);
        tft.fillRect (b.x, b.y+t, t, ho2-t-to2, fg);
        tft.fillRect (b.x, b.y+ho2-to2, w, t, fg);
        tft.fillRect (b.x+w-t, b.y+ho2+to2, t, ho2-t-to2, fg);
        tft.fillRect (b.x, b.y+h-t, w, t, fg);
        break;
    case 6:
        tft.fillRect (b.x, b.y, t, h, fg);
        tft.fillRect (b.x+t, b.y, w-t, t, fg);
        tft.fillRect (b.x+t, b.y+ho2-to2, w-t, t, fg);
        tft.fillRect (b.x+w-t, b.y+ho2+to2, t, ho2-to2-t, fg);
        tft.fillRect (b.x+t, b.y+h-t, w-t, t, fg);
        break;
    case 7:
        tft.fillRect (b.x, b.y, w, t, fg);
        tft.fillRect (b.x+w-t, b.y+t, t, h-t, fg);
        break;
    case 8:
        tft.fillRect (b.x, b.y, t, h, fg);
        tft.fillRect (b.x+w-t, b.y, t, h, fg);
        tft.fillRect (b.x+t, b.y, w-2*t, t, fg);
        tft.fillRect (b.x+t, b.y+ho2-to2, w-2*t, t, fg);
        tft.fillRect (b.x+t, b.y+h-t, w-2*t, t, fg);
        break;
    case 9:
        tft.fillRect (b.x, b.y, t, ho2+to2, fg);
        tft.fillRect (b.x+w-t, b.y, t, h, fg);
        tft.fillRect (b.x+t, b.y, w-2*t, t, fg);
        tft.fillRect (b.x+t, b.y+ho2-to2, w-2*t, t, fg);
        break;
    default:
        fatalError("drawDigit %d", digit);
        break;
    }
}

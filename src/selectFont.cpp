/* interface that allows direct usage of output files created by fontconvert tool without wrangling.
 */

#include "HamClock.h"

void selectFontStyle (FontWeight w, FontSize s)
{
    if (s == SMALL_FONT) {
        if (w == BOLD_FONT)
            tft.setFont(&Germano_Bold16pt7b);
        else
            tft.setFont(&Germano_Regular16pt7b);
    } else if (s == LARGE_FONT) {
        tft.setFont(&Germano_Bold30pt7b);
    } else /* FAST_FONT */ {
        tft.setFont(NULL);
    }
}

/* get current font without having to know internal names
 */
void getFontStyle (FontWeight *wp, FontSize *sp)
{
    const GFXfont *f = tft.getFont();

    if (f == &Germano_Bold16pt7b) {
        *wp = BOLD_FONT;
        *sp = SMALL_FONT;
    } else if (f == &Germano_Regular16pt7b) {
        *wp = LIGHT_FONT;
        *sp = SMALL_FONT;
    } else if (f == &Germano_Bold30pt7b) {
        *wp = BOLD_FONT;
        *sp = LARGE_FONT;
    } else {
        *wp = LIGHT_FONT;
        *sp = FAST_FONT;
    }
}

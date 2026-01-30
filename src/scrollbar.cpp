/* generic vertical scroll bar
 *
 *                    ----                         
 *                 0 |    |                       ^
 *                    ----                        |
 * top_vis = 1 ->  1 |    |          ^            |
 *                    ----           |            |
 *                 2 |    |      max_vis = 3      |     
 *                    ----           |        n_data = 6
 *                 3 |    |          v            |
 *                    ----                        |
 *                 4 |    |                       |
 *                    ----                        |
 *                 5 |    |                       v
 *                    ----                         
 */

#include "HamClock.h"

/* draw from scratch
 */
void ScrollBar::draw(void)
{
    // skip if we can show all
    if (max_vis >= n_data)
        return;

    // init container
    fillSBox (*sc_bp, RA8875_BLACK);
    drawSBox (*sc_bp, RA8875_WHITE);

    // common arrow info
    const uint16_t tip_x = sc_bp->x + sc_bp->w/2;
    const uint16_t base_x = sc_bp->x + ARROW_GAP;
    const uint16_t base_far_x = sc_bp->x + sc_bp->w - ARROW_GAP;

    // draw up arrow
    uint16_t base_y = up_b.y + up_b.h;
    uint16_t tip_y = up_b.y;
    if (okToScrollUp())
        tft.fillTriangle (tip_x, tip_y,   base_x, base_y,  base_far_x, base_y, fg);
    else
        tft.drawTriangle (tip_x, tip_y,   base_x, base_y,  base_far_x, base_y, fg);

    // draw down arrow
    base_y = dw_b.y;
    tip_y = dw_b.y + dw_b.h - 1;
    if (okToScrollDown())
        tft.fillTriangle (tip_x, tip_y,   base_x, base_y,  base_far_x, base_y, fg);
    else
        tft.drawTriangle (tip_x, tip_y,   base_x, base_y,  base_far_x, base_y, fg);

    // draw thumb inside trough
    uint16_t tr_y = up_b.y + up_b.h + THUMB_GAP;                                // trough y
    uint16_t tr_h = dw_b.y - THUMB_GAP - tr_y;                                  // trough height
    uint16_t th_x = sc_bp->x + THUMB_GAP;
    uint16_t th_y = tr_y + top_vis * tr_h / n_data;
    uint16_t th_w = sc_bp->w - 2*THUMB_GAP;
    uint16_t th_h = n_data > max_vis ? max_vis * tr_h / n_data : tr_h;
    tft.fillRect (th_x, th_y, th_w, th_h, RA8875_WHITE);

}

/* define maximum visible data, total data, container.
 */
void ScrollBar::init (int mv, int nd, SBox &b)
{
    // capture configuration
    max_vis = mv;
    n_data = nd;
    sc_bp = &b;

    // start at top
    top_vis = 0;

    // assign surfaces

    up_b.x = sc_bp->x;
    up_b.y = sc_bp->y;
    up_b.h = 3*sc_bp->w/2;
    up_b.w = sc_bp->w;

    dw_b.x = sc_bp->x;
    dw_b.h = up_b.h;
    dw_b.y = sc_bp->y + sc_bp->h - dw_b.h;
    dw_b.w = sc_bp->w;

    trough_b.x = sc_bp->x;
    trough_b.y = sc_bp->y + up_b.h;
    trough_b.w = sc_bp->w;
    trough_b.h = sc_bp->h - up_b.h - dw_b.w;

    draw();
}


/* update position from either keyboard or touch input.
 * return whether any change.
 */
bool ScrollBar::checkTouch (char kb, SCoord &s)
{
    bool ours = false;

    // check keyboard
    switch (kb) {
    case CHAR_NONE:
        // still might be a touch
        break;
    case CHAR_UP:
        if (okToScrollUp())
            top_vis -= 1;
        ours = true;
        break;
    case CHAR_DOWN:
        if (okToScrollDown())
            top_vis += 1;
        ours = true;
        break;
    }

    // check touch
    if (!ours) {
        if (inBox (s, up_b)) {
            if (okToScrollUp())
                top_vis -= 1;
            ours = true;
        } else if (inBox (s, dw_b)) {
            if (okToScrollDown())
                top_vis += 1;
            ours = true;
        }
    }

    // update if change
    if (ours)
        draw();

    // resuilt
    return (ours);
}

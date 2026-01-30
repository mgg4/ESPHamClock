/* a few extra drawing commands
 */

#include "HamClock.h"

/* raster scan each triangle sweeping from first point. 
 * N.B. assumes convex shape
 */
void fillPolygon (const SCoord poly[], int n_poly, uint16_t color)
{
    if (n_poly < 1)
        return;
    if (n_poly == 1)
        tft.drawPixel (poly[0].x, poly[0].y, color);
    else if (n_poly == 2)
        tft.drawLine (poly[0].x, poly[0].y, poly[1].x, poly[1].y, color);
    else {
        for (int i = 1; i < n_poly-1; i++)
            tft.fillTriangle (poly[0].x, poly[0].y, poly[i].x, poly[i].y, poly[i+1].x, poly[i+1].y, color);
    }
}

/* connect the given points, including closing last back to first.
 */
void drawPolygon (const SCoord poly[], int n_poly, uint16_t color)
{
    if (n_poly < 1)
        return;
    if (n_poly == 1)
        tft.drawPixel (poly[0].x, poly[0].y, color);
    else if (n_poly == 2)
        tft.drawLine (poly[0].x, poly[0].y, poly[1].x, poly[1].y, color);
    else {
        for (int i = 0; i < n_poly-1; i++)
            tft.drawLine (poly[i].x, poly[i].y, poly[i+1].x, poly[i+1].y, color);
        tft.drawLine (poly[n_poly-1].x, poly[n_poly-1].y, poly[0].x, poly[0].y, color);
    }
}


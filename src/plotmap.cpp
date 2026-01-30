/* a generic mechanism to plot a server file or x/y table on map_b
 */

#include "HamClock.h"

// layout
#define TOPB            46                      // top border
#define BOTB            40                      // bottom border
#define LEFTB           40                      // left border
#define RIGHTB          15                      // right border
#define TICKL           5                       // tick mark length
#define PLOTW           (map_b.w-LEFTB-RIGHTB)  // plot width
#define PLOTH           (map_b.h-TOPB-BOTB)     // plot height
#define AXISXL          (map_b.x + LEFTB)       // axis x left
#define AXISXR          (AXISXL + PLOTW)        // axis x right
#define AXISYT          (map_b.y + TOPB)        // axis y top
#define AXISYB          (AXISYT + PLOTH)        // axis y bottom
#define XLBLY           (AXISYB+20)             // x axis label y
#define TTLY            (map_b.y + 30)          // title y
#define ERR_DWELL       2000                    // time to leave up err msg, msec

// colors
#define AXISC           BRGRAY                  // axis color
#define GRIDC           GRAY                    // grid color
#define LABELC          RA8875_WHITE            // label color
#define TITLEC          RA8875_WHITE            // title color
#define DATAC           RA8875_GREEN            // data color

// plot decoration
#define NXTICKS         20                      // nominal number of x tickmarks
#define NYTICKS         10                      // nominal number of y tickmarks
#define DX2GX(x)  (AXISXL + (float)PLOTW*((x)-xticks[0])/(xticks[n_xticks-1]-xticks[0])) // data to graphics x
#define DY2GY(y)  (AXISYB - (float)PLOTH*((y)-yticks[0])/(yticks[n_yticks-1]-yticks[0])) // data to graphics y


/* plot the 2d data on the earth map.
 */
void plotMapData (const char title[], const char x_label[], float x_data[], float y_data[], int n_data)
{
    // skip if not at least 2 points
    if (n_data < 2) {
        Serial.printf ("PMAP: only %d data points, require 2\n", n_data);
        return;
    }

    // erase
    fillSBox (map_b, RA8875_BLACK);

    // find ranges
    float min_x = x_data[0], max_x = x_data[0];
    float min_y = y_data[0], max_y = y_data[0];
    for (int i = 1; i < n_data; i++) {
        if (x_data[i] < min_x)
            min_x = x_data[i];
        if (x_data[i] > max_x)
            max_x = x_data[i];
        if (y_data[i] < min_y)
            min_y = y_data[i];
        if (y_data[i] > max_y)
            max_y = y_data[i];
    }

    // find tickmarks
    float xticks[NXTICKS+2], yticks[NYTICKS+2];
    int n_xticks = tickmarks (min_x, max_x, NXTICKS, xticks);
    int n_yticks = tickmarks (min_y, max_y, NYTICKS, yticks);

    // title
    selectFontStyle (LIGHT_FONT, SMALL_FONT);
    tft.setTextColor (TITLEC);
    uint16_t tw = getTextWidth(title);
    tft.setCursor (map_b.x + (map_b.w - tw)/2, TTLY);
    tft.print (title);

    // draw axes
    tft.drawLine (AXISXL, AXISYT, AXISXL, AXISYB, AXISC);
    tft.drawLine (AXISXL, AXISYB, AXISXR, AXISYB, AXISC);

    // draw x label
    selectFontStyle (LIGHT_FONT, FAST_FONT);
    uint16_t xw = getTextWidth(x_label);
    tft.setCursor (map_b.x + (map_b.w - xw)/2, XLBLY);
    tft.print (x_label);

    // draw grids
    tft.setTextColor (LABELC);
    for (int i = 0; i < n_xticks; i++) {
        char lbl[20];
        uint16_t x = DX2GX(xticks[i]);
        tft.drawLine (x, AXISYT, x, AXISYB+TICKL, GRIDC);
        snprintf (lbl, sizeof(lbl), "%.4g", xticks[i]);
        tft.setCursor (x-getTextWidth(lbl)/2, AXISYB+TICKL+4);  // center lbl
        tft.print (lbl);
    }
    for (int i = 0; i < n_yticks; i++) {
        uint16_t y = DY2GY(yticks[i]);
        tft.drawLine (AXISXL-TICKL, y, AXISXR, y, GRIDC);
        tft.setCursor (AXISXL-LEFTB+1, y-4);
        tft.printf("%g", yticks[i]);
    }

    // finally the data -- looks nicer at raw resolition
    uint16_t prev_x = 0, prev_y = 0;
    for (int i = 0; i < n_data; i++) {
        uint16_t x = DX2GX(x_data[i]) * tft.SCALESZ;
        uint16_t y = DY2GY(y_data[i]) * tft.SCALESZ;
        if (i > 0)
            tft.drawLineRaw (prev_x, prev_y, x, y, 2, DATAC);
        prev_x = x;
        prev_y = y;
    }

    // create resume button box
    SBox resume_b;
    resume_b.w = 100;
    resume_b.x = map_b.x + map_b.w - resume_b.w - LEFTB;
    resume_b.h = 40;
    resume_b.y = map_b.y + 4;
    const char button_name[] = "Resume";
    selectFontStyle (LIGHT_FONT, SMALL_FONT);
    drawStringInBox (button_name, resume_b, false, RA8875_GREEN);

    // see it all now
    tft.drawPR();

    // report info for tap times until time out or do anything
    UserInput ui = {
        map_b,
        UI_UFuncNone,
        UF_UNUSED,
        30000,
        UF_CLOCKSOK,
        {0, 0}, TT_NONE, '\0', false, false
    };
    (void) waitForUser(ui);

    // ack
    drawStringInBox (button_name, resume_b, true, RA8875_GREEN);
    tft.drawPR();

    // restore
    initEarthMap();
}

/* read and plot the given server file.
 */
void plotServerFile (const char *filename, const char *title, const char x_label[])
{
    // base of filename
    const char *file_slash = strrchr (filename, '/');
    const char *file_base = file_slash ? file_slash + 1 : filename;

    // read data
    float *x_data = NULL;
    float *y_data = NULL;
    int n_data = 0;
    WiFiClient map_client;
    bool ok = false;

    Serial.println (filename);
    if (map_client.connect (backend_host, backend_port)) {
        updateClocks(false);

        // query web page
        httpHCGET (map_client, backend_host, filename);

        // skip response header
        if (!httpSkipHeader (map_client)) {
            mapMsg (2000, "%s: Header is short", file_base);
            goto out;
        }

        // read lines, adding to x_data[] and y_data[]
        char line[100];
        while (getTCPLine (map_client, line, sizeof(line), NULL)) {

            // crack
            float x, y;
            if (sscanf (line, "%f %f", &x, &y) != 2) {
                Serial.printf ("PMAP: bad line: %s\n", line);
                mapMsg (2000, "%s: Data is corrupted", file_base);
                goto out;
            }

            // grow
            float *new_x = (float *) realloc (x_data, (n_data+1) * sizeof(float));
            float *new_y = (float *) realloc (y_data, (n_data+1) * sizeof(float));
            if (!new_x || !new_y) {
                mapMsg (2000, "%s: Insufficient memory", file_base);
                goto out;
            }

            // add
            x_data = new_x;
            y_data = new_y;
            x_data[n_data] = x;
            y_data[n_data] = y;
            n_data += 1;
        }

        Serial.printf ("PMAP: read %d points\n", n_data);

        // require at least a few points
        if (n_data < 10) {
            mapMsg (2000, "%s: File is short", file_base);
            goto out;
        }

        // ok!
        ok = true;
    }

out:

    if (ok)
        plotMapData (title, x_label, x_data, y_data, n_data);

    // clean up, any error is already reported
    free (x_data);
    free (y_data);
    map_client.stop();
}

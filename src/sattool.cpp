/* satellite planning tool
 */


#include "HamClock.h"


// elevation plot parameters and handy helpers
#define ST_TB           60                                      // top plot border
#define ST_LB           60                                      // left plot border
#define ST_RB           20                                      // right plot border
#define ST_BB           50                                      // bottom plot border
#define ST_NI           10                                      // next-both-up table indent
#define ST_MT           5                                       // up marker line thickness
#define ST_TL           2                                       // tick length
#define ST_PH           (map_b.h - ST_TB - ST_BB)               // plot height
#define ST_PW           (map_b.w - ST_LB - ST_RB)               // plot width
#define ST_X0           (map_b.x + ST_LB)                       // x coord of plot left
#define ST_DUR          (24*3600)                               // plot duration, seconds
#define ST_DT           (ST_DUR/200)                            // plot step size, seconds
#define ST_US           30                                      // micro step refined time, seconds
#define ST_TO           (30*1000)                               // time out, millis
#define ST_FC           RGB565(65,200,65)                       // fill color 
#define ST_TT           7                                       // timeline marker thickness
#define ST_E2Y(E)       ((uint16_t)(map_b.y+ST_TB + ST_PH*(M_PI_2F-(E))/M_PIF + 0.5F)) // elev to y coord
#define ST_T2X(T)       ((uint16_t)(ST_X0 + ST_PW*((T)-t0)/ST_DUR))     // time_t to x coord
#define ST_X2T(X)       ((time_t)(t0 + ST_DUR*((X)-ST_X0)/ST_PW))       // x coord to time_t

// handy az/el in rads, computed from SatNow which is in degs
typedef struct {
    float az, el;       // rads
} SatAzEl;

/* fill in de and dx sat sky positions at t0.
 * See getSatCir() for specifics.
 */
static void getObsCir (Observer *de_obsp, Observer *dx_obsp, time_t t0, SatAzEl &de_azel, SatAzEl &dx_azel)
{
    SatNow snow;
    if (!getSatCir (de_obsp, t0, snow))
        fatalError ("SatTool failed to get sat DE info");
    de_azel.az = deg2rad(snow.az);
    de_azel.el = deg2rad(snow.el);
    if (!getSatCir (dx_obsp, t0, snow))
        fatalError ("SatTool failed to get sat DX info");
    dx_azel.az = deg2rad(snow.az);
    dx_azel.el = deg2rad(snow.el);
}

/* draw everything in the plot except the elevation plots, Resume button and the "Next Up" table.
 * t0 is nowWO()
 */
static void drawSTSetup (time_t t0)
{
        // get sat name, if up
        SatNow snow;
        if (!getSatNow (snow))
            fatalError ("SatTool failed to get sat name");

        // grid lines color
        const uint16_t dark = RGB565(50,50,50);

        // title
        char title[NV_SATNAME_LEN + 100];
        snprintf (title, sizeof(title), "%s Elevation at DE and DX", snow.name);
        selectFontStyle (LIGHT_FONT, SMALL_FONT);
        uint16_t tw = getTextWidth(title);
        tft.setCursor (map_b.x + (map_b.w-tw)/2, map_b.y + 30);
        tft.setTextColor (RA8875_WHITE);
        tft.print (title);

        // x and y axes
        tft.drawLine (ST_X0, ST_E2Y(-M_PI_2F), ST_X0, ST_E2Y(M_PI_2F), BRGRAY);
        tft.drawLine (ST_X0, ST_E2Y(-M_PI_2F), ST_X0 + ST_PW, ST_E2Y(-M_PI_2F), BRGRAY);

        // center line
        tft.drawLine (ST_X0, ST_E2Y(0), ST_X0+ST_PW, ST_E2Y(0), GRAY);

        // horizontal grid lines
        for (int i = -80; i <= 90; i += 10)
            tft.drawLine (ST_X0+ST_TL, ST_E2Y(deg2rad(i)), ST_X0 + ST_PW, ST_E2Y(deg2rad(i)), dark);
        tft.drawLine (ST_X0+ST_TL, ST_E2Y(0), ST_X0 + ST_PW, ST_E2Y(0), GRAY);

        // y labels
        selectFontStyle (LIGHT_FONT, FAST_FONT);
        tft.setTextColor(BRGRAY);
        tft.setCursor (ST_X0 - 20, ST_E2Y(M_PI_2F) - 4);
        tft.print ("+90");
        tft.setCursor (ST_X0 - 10, ST_E2Y(0) - 4);
        tft.print ("0");
        tft.setCursor (ST_X0 - 20, ST_E2Y(-M_PI_2F) - 4);
        tft.print ("-90");
        tft.setCursor(ST_X0-17, ST_E2Y(deg2rad(50)));
        tft.print("Up");
        tft.setCursor(ST_X0-29, ST_E2Y(deg2rad(-45)));
        tft.print("Down");
        const char estr[] = "Elevation";
        const int estr_l = strlen(estr);
        for (int i = 0; i < estr_l; i++) {
            tft.setCursor(ST_X0-42, ST_E2Y(deg2rad(45-10*i)));
            tft.print(estr[i]);
        }

        // y tick marks
        for (int i = -80; i <= 90; i += 10)
            tft.drawLine (ST_X0, ST_E2Y(deg2rad(i)), ST_X0+ST_TL, ST_E2Y(deg2rad(i)), BRGRAY);

        // time zone labels
        uint16_t de_y = ST_E2Y(-M_PI_2F) + 6;
        uint16_t dx_y = ST_E2Y(-M_PI_2F) + ST_BB/2 - 4;
        uint16_t utc_y = ST_E2Y(-M_PI_2F) + ST_BB - 6-8;
        tft.setTextColor (DE_COLOR);
        tft.setCursor (ST_X0-53, de_y);
        tft.print ("DE hour");
        tft.setTextColor (DX_COLOR);
        tft.setCursor (ST_X0-53, dx_y);
        tft.print ("DX");
        tft.setTextColor (RA8875_WHITE);
        tft.setCursor (ST_X0-53, utc_y);
        tft.print ("UTC");

        // x axis time line and vertical grid lines, mark each even hour.
        // N.B. check every 15 minutes for oddball time zones (looking at you Australia)
        int detz = getTZ (de_tz);
        int dxtz = getTZ (dx_tz);
        tft.drawLine (ST_X0, dx_y-3, ST_X0+ST_PW, dx_y-3, BRGRAY);
        tft.drawLine (ST_X0, utc_y-3, ST_X0+ST_PW, utc_y-3, BRGRAY);
        int prev_de_hr = hour (t0 + detz);
        int prev_dx_hr = hour (t0 + dxtz);
        int prev_utc_hr = hour (t0);
        for (time_t t = 900*(t0/900+1); t < t0 + ST_DUR; t += 900) {

            // get x coord of this time
            uint16_t x = ST_T2X(t);

            // get times in each zone
            int de_hr = hour (t + detz);
            int dx_hr = hour (t + dxtz);
            int utc_hr = hour (t);

            // plot each time zone every 2 hours
            if (prev_de_hr != de_hr && (de_hr%2)==0) {
                tft.drawLine (x, ST_E2Y(-M_PI_2F), x, ST_E2Y(M_PI_2F), dark);
                tft.drawLine (x, ST_E2Y(-M_PI_2F), x, ST_E2Y(-M_PI_2F)-ST_TL, RA8875_WHITE);
                tft.setTextColor (DE_COLOR);
                tft.setCursor (x-(de_hr<10?3:6), de_y);         // center X or XX
                tft.print (de_hr);
            }
            if (prev_dx_hr != dx_hr && (dx_hr%2)==0) {
                tft.drawLine (x, dx_y-3, x, dx_y-3-ST_TL, RA8875_WHITE);
                tft.setTextColor (DX_COLOR);
                tft.setCursor (x-(dx_hr<10?3:6), dx_y);
                tft.print (dx_hr);
            }
            if (prev_utc_hr != utc_hr && (utc_hr%2)==0) {
                tft.drawLine (x, utc_y-3, x, utc_y-3-ST_TL, RA8875_WHITE);
                tft.setTextColor (RA8875_WHITE);
                tft.setCursor (x-(utc_hr<10?3:6), utc_y);
                tft.print (utc_hr);
            }

            // retain for next loop
            prev_de_hr = de_hr;
            prev_dx_hr = dx_hr;
            prev_utc_hr = utc_hr;
        }
}

/* draw both elevation plots.
 * return rough start and end times +- ST_DT of first period in which moon is up for both, with complications:
 *   start == t0 means plot period started both-up;
 *   end == 0 means both-up never ended within plot duration;
 *   both above means always both-up;
 *   start == 0 means never both-up, end has no meaning
 * t0 is nowWO()
 */
static void drawSTElPlot (time_t t0, time_t &t_start, time_t &t_end)
{
        // define the two observers
        Observer de_obs (de_ll.lat_d, de_ll.lng_d, 0);
        Observer dx_obs (dx_ll.lat_d, dx_ll.lng_d, 0);

        // reset start/end so we can set with first occurance
        t_start = t_end = 0;

        // previous location in order to build line segments and find when both just up or down
        uint16_t prev_x = 0, prev_de_y = 0, prev_dx_y = 0;
        bool prev_both_up = false;

        // handy
        const uint16_t x_step = ST_T2X(ST_DT) - ST_T2X(0);    // time step x change
        const uint16_t elm90y = ST_E2Y(deg2rad(-90));         // y of -90 el

        // work across plot
        for (time_t t = t0; t <= t0 + ST_DUR; t += ST_DT) {

            // find each circumstance at time t
            SatAzEl de_azel, dx_azel;
            getObsCir (&de_obs, &dx_obs, t, de_azel, dx_azel);
            uint16_t de_y = ST_E2Y(de_azel.el);
            uint16_t dx_y = ST_E2Y(dx_azel.el);
            uint16_t x = ST_T2X(t);

            // check both_up_now
            bool both_up_now = de_azel.el > 0 && dx_azel.el > 0;

            // emphasize when both up
            if (!prev_both_up && both_up_now) {
                // approximate this starting half step left of x .. beware left edge
                uint16_t left_x = x - x_step/2;
                if (left_x < ST_X0)
                    left_x = ST_X0;
                tft.fillRect (left_x, elm90y-ST_TT, x_step/2 + 1, ST_TT, ST_FC);
            } else if (prev_both_up && both_up_now) {
                // mark entire step
                tft.fillRect (prev_x, elm90y-ST_TT, x_step + 1, ST_TT, ST_FC);
            } else if (prev_both_up && !both_up_now) {
                // approximate this stopping half step right of prev_x .. beware right edge
                uint16_t width = x_step/2;
                if (x + width > ST_X0 + ST_PW)
                    width = ST_X0 + ST_PW - x;
                tft.fillRect (prev_x, elm90y-ST_TT, width, ST_TT, ST_FC);
            }

            // continue line segment connected to previous location
            if (t > t0) {
                tft.drawLine (prev_x, prev_de_y, x, de_y, DE_COLOR);
                tft.drawLine (prev_x, prev_dx_y, x, dx_y, DX_COLOR);
            }

            // note when first both up or down
            if (both_up_now) {
                if (t_start == 0)
                    t_start = t;
            } else if (prev_both_up) {
                if (t_end == 0)
                    t_end = t;
            }

            // save for next iteration
            prev_x = x;
            prev_de_y = de_y;
            prev_dx_y = dx_y;
            prev_both_up = both_up_now;
        }

        Serial.printf ("SatTool:: rough start %02d:%02d end %02d:%02d\n",
                                hour(t_start), minute(t_start),
                                hour(t_end), minute(t_end));
}

/* given plot start time and approximate times for both-up start and end, refine and draw table.
 * N.B. see drawSTElPlot comments for special cases.
 */
static void drawSTBothUpTable (time_t t0, time_t t_start, time_t t_end)
{
        // define the two observers
        Observer de_obs (de_ll.lat_d, de_ll.lng_d, 0);
        Observer dx_obs (dx_ll.lat_d, dx_ll.lng_d, 0);

        bool always_both_up = t_start == t0 && !t_end;
        bool never_both_up = t_start == 0;
        bool finite_both_up = !always_both_up && !never_both_up;
        char buf[50];

        // search around times in finer steps to refine to nearest ST_US
        time_t better_start = 0, better_end = 0;
        if (finite_both_up) {

            // find better start unless now
            if (t_start > t0) {
                bool both_up_now = true;
                for (better_start = t_start - ST_US; both_up_now; better_start -= ST_US) {
                    SatAzEl de_azel, dx_azel;
                    getObsCir (&de_obs, &dx_obs, better_start, de_azel, dx_azel);
                    both_up_now = de_azel.el > 0 && dx_azel.el > 0;
                }
                better_start += 2*ST_US;            // return to last known both_up_now
            } else {
                better_start = t0;
            }

            // find better end
            bool both_up_now = false;
            for (better_end = t_end - ST_US; !both_up_now; better_end -= ST_US) {
                SatAzEl de_azel, dx_azel;
                getObsCir (&de_obs, &dx_obs, better_end, de_azel, dx_azel);
                both_up_now = de_azel.el > 0 && dx_azel.el > 0;
            }
            better_end += 2*ST_US;              // return to last known !both_up_now

            Serial.printf ("SatTool:: better start %02d:%02d end %02d:%02d\n",
                                hour(better_start), minute(better_start),
                                hour(better_end), minute(better_end));
        }

        // table title
        selectFontStyle (LIGHT_FONT, FAST_FONT);
        tft.setTextColor (RA8875_WHITE);
        tft.setCursor (map_b.x+ST_NI, map_b.y+5);
        if (always_both_up) {
            tft.print ("Both always up");
            return;
        }
        if (never_both_up) {
            tft.print ("Never both up");
            return;
        }
        int dt = better_end - better_start;
        snprintf (buf, sizeof(buf), "Next both up %02dh%02d", dt/3600, (dt%3600)/60);
        tft.print (buf);


        // DE row
        int detz = getTZ (de_tz);
        if (better_start == t0)  {
            snprintf (buf, sizeof(buf), "DE    now    %02d:%02d",
                    hour(better_end+detz), minute(better_end+detz));
        } else {
            snprintf (buf, sizeof(buf), "DE   %02d:%02d   %02d:%02d",
                    hour(better_start+detz), minute(better_start+detz),
                    hour(better_end+detz), minute(better_end+detz));
        }
        tft.setTextColor (DE_COLOR);
        tft.setCursor (map_b.x+ST_NI, map_b.y+15);
        tft.print (buf);

        // DX row
        int dxtz = getTZ (dx_tz);
        if (better_start == t0)  {
            snprintf (buf, sizeof(buf), "DX    now    %02d:%02d",
                    hour(better_end+dxtz), minute(better_end+dxtz));
        } else {
            snprintf (buf, sizeof(buf), "DX   %02d:%02d   %02d:%02d",
                    hour(better_start+dxtz), minute(better_start+dxtz),
                    hour(better_end+dxtz), minute(better_end+dxtz));
        }
        tft.setTextColor (DX_COLOR);
        tft.setCursor (map_b.x+ST_NI, map_b.y+25);
        tft.print (buf);

        // UTC rows
        if (better_start == t0)  {
            snprintf (buf, sizeof(buf), "UTC   now    %02d:%02d",
                    hour(better_end), minute(better_end));
        } else {
            snprintf (buf, sizeof(buf), "UTC  %02d:%02d   %02d:%02d",
                    hour(better_start), minute(better_start),
                    hour(better_end), minute(better_end));
        }
        tft.setTextColor (RA8875_WHITE);
        tft.setCursor (map_b.x+ST_NI, map_b.y+35);
        tft.print (buf);
}

/* draw popup in the given box for time t
 */
static void drawSTPopup (const time_t t, const SBox &popup_b)
{

        // define the two observers
        Observer de_obs (de_ll.lat_d, de_ll.lng_d, 0);
        Observer dx_obs (dx_ll.lat_d, dx_ll.lng_d, 0);

        // find each circumstance at time t
        SatAzEl de_azel, dx_azel;
        getObsCir (&de_obs, &dx_obs, t, de_azel, dx_azel);

        // prep popup rectangle
        fillSBox (popup_b, RA8875_BLACK);
        drawSBox (popup_b, RA8875_WHITE);

        // draw column headings
        tft.setTextColor (RA8875_WHITE);
        selectFontStyle (LIGHT_FONT, FAST_FONT);
        tft.setCursor (popup_b.x+4, popup_b.y+2);
        tft.print ("     Time   Az   El");

        // draw time, el and az at each location
        char buf[100];

        int detz = getTZ (de_tz);
        snprintf (buf, sizeof(buf), "DE  %02d:%02d  %3.0f %4.0f", hour(t+detz),
                minute(t+detz), rad2deg(de_azel.az), rad2deg(de_azel.el));
        tft.setCursor (popup_b.x+4, popup_b.y+14);
        tft.setTextColor(DE_COLOR);
        tft.print (buf);

        int dxtz = getTZ (dx_tz);
        snprintf (buf, sizeof(buf), "DX  %02d:%02d  %3.0f %4.0f", hour(t+dxtz),
                minute(t+dxtz), rad2deg(dx_azel.az), rad2deg(dx_azel.el));
        tft.setCursor (popup_b.x+4, popup_b.y+24);
        tft.setTextColor(DX_COLOR);
        tft.print (buf);

        snprintf (buf, sizeof(buf), "UTC %02d:%02d", hour(t), minute(t));
        tft.setCursor (popup_b.x+4, popup_b.y+34);
        tft.setTextColor(RA8875_WHITE);
        tft.print (buf);

        // now
        tft.drawPR();
}

/* plot satellite elevation vs time on map_b. time goes forward a few days. label in DE DX local and UTC.
 */
void drawSatTool()
{
        // start now
        time_t t0 = nowWO();

        // erase
        fillSBox (map_b, RA8875_BLACK);

        // draw boilerplate
        drawSTSetup (t0);

        // draw elevation plot, find first period when both up
        time_t t_start, t_end;
        drawSTElPlot (t0, t_start, t_end);

        // refine and draw both-up table
        drawSTBothUpTable (t0, t_start, t_end);

        // create resume button box
        SBox resume_b;
        resume_b.w = 100;
        resume_b.x = map_b.x + map_b.w - resume_b.w - ST_RB;
        resume_b.h = 40;
        resume_b.y = map_b.y + 4;
        const char button_name[] = "Resume";
        selectFontStyle (LIGHT_FONT, SMALL_FONT);
        drawStringInBox (button_name, resume_b, false, RA8875_GREEN);

        // see it all now
        tft.drawPR();

        // popup history for erasing
        bool popup_is_up = false;
        bool popup_was_up = false;
        SBox popup_b = {0,0,0,0};

        // report info for tap times until time out or tap Resume button
        UserInput ui = {
            map_b,
            UI_UFuncNone,
            UF_UNUSED,
            ST_TO,
            UF_CLOCKSOK,
            {0, 0}, TT_NONE, '\0', false, false
        };

        while (waitForUser(ui)) {

            // done if return, esc or tap Resume button or tap outside box
            if (ui.kb_char == CHAR_CR || ui.kb_char == CHAR_NL || ui.kb_char == CHAR_ESC
                        || inBox (ui.tap, resume_b) || (ui.kb_char == CHAR_NONE && !inBox (ui.tap, map_b)))
                break;

            // first erase previous popup, if any
            if (popup_is_up) {
                fillSBox (popup_b, RA8875_BLACK);
                drawSTSetup (t0);
                drawSTElPlot (t0, t_start, t_end);
                popup_is_up = false;
                popup_was_up = true;
            }

            // show new popup if tap within the plot area but outside previous popup
            if (ui.tap.x > ST_X0 && ui.tap.x < ST_X0 + ST_PW && ui.tap.y > ST_E2Y(M_PI_2F)
                                && ui.tap.y < ST_E2Y(-M_PI_2F)
                                && (!popup_was_up || !inBox (ui.tap, popup_b))) {


                // popup at s
                popup_b.x = ui.tap.x;
                popup_b.y = ui.tap.y;
                popup_b.w = 122;
                popup_b.h = 45;

                // insure entirely over plot
                if (popup_b.x + popup_b.w > ST_X0 + ST_PW)
                    popup_b.x = ST_X0 + ST_PW - popup_b.w;
                if (popup_b.y + popup_b.h > ST_E2Y(-M_PI_2F) - ST_MT)
                    popup_b.y = ST_E2Y(-M_PI_2F) - ST_MT - popup_b.h;

                // draw popup
                drawSTPopup (ST_X2T(ui.tap.x), popup_b);

                // update popup state
                popup_was_up = popup_is_up;
                popup_is_up = true;
            }
        }

        // ack
        selectFontStyle (LIGHT_FONT, SMALL_FONT);
        drawStringInBox (button_name, resume_b, true, RA8875_GREEN);
        tft.drawPR();
}

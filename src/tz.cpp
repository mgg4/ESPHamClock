/* manage getting time zone at lat/lng.
 * uses the same data storeage as wx.cpp because wx and tz data both arrive together from open weather map.
 */


#include "HamClock.h"



/* look up timezone from local grid for the approx location.
 */
int getFastTZ (const LatLong &ll)
{
    const WXInfo *wip = findWXFast (ll);
    if (wip)
        return (wip->timezone);
    Serial.printf ("TZ: getFastTZ not found %g %g\n", ll.lat_d, ll.lng_d);
    return (0);
}

/* given a LatLong return the smallest deviation from a whole hour among the surrounding timezones.
 */
int getFastTZStep (const LatLong &ll)
{
    // scan 5x5 neighborhood around ll
    int min_step = 3600;                                // look for steps smaller than one hour
    for (int d_lat = -2; d_lat <= 2; d_lat += 1) {
        LatLong ll_step;
        ll_step.lat_d = ll.lat_d + d_lat;
        if (ll_step.lat_d < -89 || ll_step.lat_d > 89)
            continue;
        for (int d_lng = -2; d_lng <= 2; d_lng += 1) {
            ll_step.lng_d = ll.lng_d + d_lng;
            ll_step.normalize();                        // accounts for lng wrap
            int tz_chg = abs(getFastTZ (ll_step)) % 3600;
            if (tz_chg > 0 && tz_chg < min_step)
                min_step = tz_chg;
        }
    }

    return (min_step);
}

/* always call this to get the time zone UTC offset, never use tz_secs directly.
 * N.B. tz must be de_tz or dx_tz
 * N.B. beware nested calls, eg, via sendUserAgent()
 */
int getTZ (TZInfo &tz)
{
    bool is_de;
    if (&tz == &de_tz) {
        is_de = true;
    } else if (&tz == &dx_tz) {
        is_de = false;
    } else {
         fatalError ("getTZ unknown query");
         return (0);    // lint
    }

    // N.B. avoid recursive network calls via net retry scheculing
    static bool here_again;
    if (here_again)
        Serial.printf ("getTZ: detected recursion\n");

    if (tz.auto_tz && !here_again) {
        Message ynot;
        here_again = true;
        const WXInfo *wip = findTZCache (tz.ll, is_de, ynot);
        here_again = false;
        if (wip)
            tz.tz_secs = wip->timezone;
        else
            Serial.printf ("TZ: %s getTZ err: %s\n", is_de ? "DE" : "DX", ynot.get());
    }

    return (tz.tz_secs);
}

/* always call this to set the time zone to a static offset, never set tz_secs directly.
 * this allows setting both the time and tz mode to manual.
 * e.g. setTZSecs (de_tz, 3600);
 */
void setTZSecs (TZInfo &tz, int secs)
{
    tz.auto_tz = false;
    tz.tz_secs = secs;
}

/* arrange for tz to automatically update its time zone offset
 */
void setTZAuto (TZInfo &tz)
{
    tz.auto_tz = true;
}

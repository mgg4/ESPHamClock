/* manage WiFi field strength plot
 */

#include "HamClock.h"


// WiFi plot history state
#define WP_MAXAGE       (24*3600)               // max age to save for plotting, seconds
typedef struct {
    time_t t;                                   // sample time
    int rssi;                                   // sample value
} WPSample;
static WPSample *wp_table;                      // malloced table, ordered by increasing t
static int n_wp_table;                          // n entries in wp_table
static const char *title;                       // title in dbm or percentage


/* read wifi signal strength and whether it is dBm or percentage, add to wp_table.
 * return whether ok.
 */
bool readWiFiRSSI(int &rssi, bool &dbm)
{
    // read and validate. N.B. beware very early call with no time yet
    time_t t0 = myNow();
    if (!t0)
        return (false);
    if (!WiFi.RSSI (rssi, dbm))
        return (false);
    if (dbm && rssi > 10)                       // 10 dBm is crazy hi
        return (false);

    // remove old entries by sliding down -- remember wp_table[0] is oldest (smallest)
    for (int i = 0; i < n_wp_table; i++) {
        WPSample &wp = wp_table[i];
        time_t age = t0 - wp.t;                 // seconds old
        if (age < WP_MAXAGE) {
            // found oldest age within WP_MAXAGE: remove all older .. don't bother to realloc here
            memmove (&wp_table[0], &wp_table[i], (n_wp_table - i) * sizeof(WPSample));
            n_wp_table -= i;
            break;
        }
    }

    // append
    wp_table = (WPSample *) realloc (wp_table, (n_wp_table + 1) * sizeof(WPSample));
    if (!wp_table)
        fatalError ("no memory for %d wifi history", n_wp_table+1);
    WPSample &wp = wp_table[n_wp_table++];
    wp.t = t0;
    wp.rssi = rssi;

    // include appropriate units in title
    title = dbm ? "WiFi dBm" : "WiFi Signal Percentage";

    return (true);
}

/* plot wifi history
 */
void plotWiFiHistory()
{
    // wait for a few table entries
    if (n_wp_table < 2)
        return;

    // prep malloced scales -- N.B. free() !!
    float *x_data = NULL;                       // ago
    float *y_data = NULL;                       // dBm or % depending on is_dbm
    int n_data = 0;

    // find scale from oldest entry in wp_table[0]
    const time_t t0 = myNow();
    const bool scale_minutes = (t0 - wp_table[0].t) < 3600;
    const char *x_label = scale_minutes ? "Age, minutes" : "Age, hours";

    // form scales for table
    for (int i = 0; i < n_wp_table; i++) {

        WPSample &wp = wp_table[i];

        // expand
        x_data = (float *) realloc (x_data, (n_data + 1) * sizeof(float));
        y_data = (float *) realloc (y_data, (n_data + 1) * sizeof(float));
        if (!x_data || !y_data)
            fatalError ("no memory to plot %d wifi", n_data+1);

        // set nice x units
        time_t age = t0 - wp.t;                 // seconds old
        if (scale_minutes)
            x_data[n_data] = -age/60.0F;        // minutes ago
        else
            x_data[n_data] = -age/3600.0F;      // hours ago

        // y
        y_data[n_data] = wp.rssi;
        n_data++;
    }


    // plot when find at least a few sensible plottable points
    if (n_data >= 2)
        plotMapData (title, x_label, x_data, y_data, n_data);

    // done with axis arrays
    free (x_data);
    free (y_data);
}

/* Get lat/long and time from NMEA sentences.
 */

#include <termios.h>

#include "HamClock.h"

/* since now() can ask any time for a new sync, knowing when the second started allows it to sleep
 * to the start of the next whole second. It is assumed the NMEA sentences come in groups, the first
 * of which starts MSGGAP_DT after a whole second and the whole group takes much less than a second.
 * We also assume that the message describes the time of the _previous_ PPS moment, not the next.
 * This value was measured on an Adafruit Ultimate GPS module.
 */
#define MSGGAP_DT       380                                     // period from PPS to first serial msg, millis


// protected shared values set by thread, read by main program
static volatile time_t nmea_time;                               // UNIX epoch reported by RMC message
static volatile float nmea_lat, nmea_lng;                       // lat long, degrees +N +E
static volatile uint32_t nmea_msec0;                            // millis() at nmea_time
static volatile bool nmea_ok;                                   // nmea_* are ready
static pthread_mutex_t nmea_lock = PTHREAD_MUTEX_INITIALIZER;   // atomic access control for nmea_*


/* crack a lat or lng field into degrees
 */
static bool crackLL (char *field, float &ll)
{
    // should never include sign
    if (strchr (field, '-'))
        return (false);

    // get mins first starting at most 2 back from decimal point
    char *dpt = strchr (field, '.');
    if (!dpt)
        return (false);
    char *min_start = dpt >= field+2 ? dpt-2 : field;
    float min = atof (min_start);

    // degs is anything preceding minutes
    float deg = 0;
    if (dpt > field+2) {
        dpt[-2] = '\0';
        deg = atof (field);
    }
    ll = deg + min/60.0F;
    return (true);
}


/* crack RMC message, setting neam_* shared variables if ok.
 *   ex: $GPRMC,203522.00,A,5109.0262308,N,11401.8407342,W,0.004,133.4,130522,0.0,E,D*2B
 * N.B. GP can be GN if more than one constellation was used
 *   https://docs.novatel.com/OEM7/Content/Logs/GPRMC.htm
 * leading zeroes are not guaranteed:
 *   https://www.hisse-et-oh.com/store/medias/sailing/609/267/fdc/original/609267fdc1c6454d83482852.pdf
 *   Where a numeric latitude or longitude is given, the two digits immediately to the left of the decimal
 *   point are whole minutes, to the right are decimals of minutes, and the remaining digits to the left of
 *   the whole minutes are whole degrees.
 *   Eg. 4533.35 is 45 degrees and 33.35 minutes. ".35" of a minute is exactly 21 seconds.
 *   Eg. 16708.033 is 167 degrees and 8.033 minutes. ".033" of a minute is about 2 seconds.
 * N.B we assume nmea_lock is already acquired
 * return whether nmea_time nmea_lat nmea_lng have be updated.
 */
static bool crackRMC (const char *rmc)
{
    // guilty until proven innocent
    bool ok = false;

    // make a temporary mutable copy for parsing
    StackMalloc rmc_copy(strlen(rmc)+1);
    char *msg = (char *)rmc_copy.getMem();
    strcpy (msg, rmc);

    // build these from fields
    unsigned sum = 0;
    float lat = 0, lng = 0;
    int dy, mn, yr;
    int hh, mm, ss;

    // confirm starts with $
    if (rmc[0] != '$') {
        Serial.printf ("NMEA: message does not begin with $\n");
        goto out;
    }

    // confirm the checksum
    for (const char *cp = rmc+1; *cp != '\0'; cp++) {
        if (*cp == '*') {
            unsigned chk = strtol (cp+1, NULL, 16);
            if (sum != chk) {
                Serial.printf ("NMEA: bad checksum: %02X != %02X\n", chk, sum);
                goto out;
            }
            break;
        }
        sum ^= (unsigned char)*cp;
    }

    // crack each field
    for (int i = 0; i < 13; i++) {

        // next field extends to next comma or *
        char *field = strsep (&msg, ",*");
        if (!field) {
            Serial.printf ("NMEA: missing field comma\n");
            goto out;
        }
        if (debugLevel (DEBUG_NMEA, 2))
            Serial.printf ("NMEA: field %d %s\n", i, field);

        // crack depending on field
        switch (i) {
        case 0:         // header
            break;
        case 1:         // UTC hhmmss.ss 144326.00
            // N.B. if try to round up seconds > .5 then day itself might also need advancing, eg 23:59:59.9
            if (sscanf (field, "%2d%2d%2d", &hh, &mm, &ss) != 3) {
                Serial.printf ("NMEA: bogus UTC: %s\n", field);
                goto out;
            }
            break;
        case 2:         // Position status (A = data valid, V = data invalid)
            if (*field != 'A') {
                Serial.printf ("NMEA: data marked invalid: %c\n", *field);
                goto out;
            }
            break;
        case 3:         // Latitude DDmm.mm 5107.0017737
            if (!crackLL (field, lat)) {
                Serial.printf ("NMEA: bogus latitude: %s\n", field);
                goto out;
            }
            break;
        case 4:         // Latitude direction: (N = North, S = South)
            if (*field == 'S')
                lat = -lat;
            else if (*field != 'N') {
                Serial.printf ("NMEA: bogus latitude direction: %c\n", *field);
                goto out;
            }
            break;
        case 5:         // Longitude (DDDmm.mm) 11402.3291611
            if (!crackLL (field, lng)) {
                Serial.printf ("NMEA: bogus longitude: %s\n", field);
                goto out;
            }
            break;
        case 6:         // Longitude direction: (E = East, W = West)
            if (*field == 'W')
                lng = -lng;
            else if (*field != 'E') {
                Serial.printf ("NMEA: bogus longitude direction: %c\n", *field);
                goto out;
            }
            break;
        case 7:         // Speed over ground, knots
            break;
        case 8:         // Track made good, degrees True
            break;
        case 9:         // Date: dd/mm/yy 210307
            if (sscanf (field, "%2d%2d%2d", &dy, &mn, &yr) != 3) {
                Serial.printf ("NMEA: bogus date: %s\n", field);
                goto out;
            }
            break;
        case 10:        // Magnetic variation, degrees
            break;
        case 11:        // Magnetic variation direction E/W
            break;
        case 12:        // Positioning system mode indicator
            break;
        default:
            fatalError ("bogus crackRMC field count %d\n", i);
            break;
        }
    }

    // ok!
    ok = true;

out:
    if (ok) {

        // convert to our format, time takes a bit of work
        struct tm tms;
        memset (&tms, 0, sizeof(tms));
        tms.tm_year = yr + 2000 - 1900;         // wants year - 1900
        tms.tm_mon = mn - 1;;                   // wants 0..11
        tms.tm_mday = dy;
        tms.tm_hour = hh;
        tms.tm_min = mm;
        tms.tm_sec = ss;
        setenv ("TZ", "", 1);                   // UTC
        tzset();
        time_t unix = mktime (&tms);

        // post new data
        nmea_time = unix;
        nmea_lat = lat;
        nmea_lng = lng;

    } else {

        // post error state
        Serial.printf ("NMEA: bad line: %s\n", rmc);
    }

    return (ok);
}

/* prepare the given tty fd
 */
static bool setNMEAtty (int fd, Message &ynot)
{
    // set up for line input
    struct termios t;
    memset (&t, 0, sizeof(t));
    t.c_iflag = IGNBRK | IGNPAR | ISTRIP | IGNCR;
    t.c_oflag = 0;
    t.c_cflag = CS8 | CREAD | CLOCAL;           // 8N1
    t.c_lflag = ICANON;

    // set speed
    const char *baud_str = getNMEABaud();
    int B;
    if (strcmp (baud_str, "4800") == 0)
        B = B4800;
    else if (strcmp (baud_str, "9600") == 0)
        B = B9600;
    else if (strcmp (baud_str, "38400") == 0)
        B = B38400;
    else {
        ynot.printf ("bad baud %s", baud_str);
        return(false);
    }
    if (cfsetspeed (&t, B) < 0) {
        ynot.printf ("cfsetspeed %s: %s", baud_str, strerror(errno));
        return(false);
    }

    // engage
    if (tcsetattr (fd, TCSANOW, &t) < 0) {
        ynot.printf ("tcsetattr %s", strerror(errno));
        return(false);
    }

    // fresh
    if (tcflush (fd, TCIOFLUSH) < 0) {
        ynot.printf ("tcflush %s", strerror(errno));
        return(false);
    }

    if (debugLevel (DEBUG_NMEA, 1))
        Serial.printf ("NMEA: %s set to %s\n", getNMEAFile(), baud_str);

    return (true);
}

/* forever thread to read NMEA messages and update nmea_*.
 * we presume the device name has already been checked by checkNMEAFilename.
 * N.B.: we call fatalError a lot -- better to keep retrying?
 */
static void *theNMEAThread (void *unused)
{
    // forver
    (void) unused;
    pthread_detach(pthread_self());

    // open and prep
    char path[1000];
    const char *fn = getNMEAFile();
    if (!expandENV (fn, path, sizeof(path)))
        fatalError ("NMEA: %s env expansion failed", fn);
    int fd = open (path, O_RDONLY);
    if (fd < 0)
        fatalError ("NMEA device %s: %s", path, strerror(errno));

    // do a little more if it appears to be a tty
    unsigned tty_cps = 0;                                       // chars/sec if tty
    if (isatty (fd)) {
        const char *baud_str = getNMEABaud();
        tty_cps = atoi(baud_str)/10;                            // assumes 8N1, ie, 10 bits/char
        Serial.printf ("NMEA: tty %u char/sec\n", tty_cps);
        Message ynot;
        if (!setNMEAtty (fd, ynot)) {
            close (fd);
            fatalError ("NMEA device %s: %s", path, ynot.get());
        }
    }

    // handy FILE
    char msg[200];
    FILE *fp = fdopen (fd, "r");

    // read forever, capture RMC and start of message collection
    uint32_t msec0 = 0;                                         // millis of start of second when known
    uint32_t m0 = millis();                                     // measure fgets() time
    while (fgets (msg, sizeof(msg), fp)) {

        // measure fgets time
        uint32_t m1 = millis();
        uint32_t fgets_ms = m1 - m0;

        // count message length and rm nl
        size_t msg_l = strlen (msg);
        if (msg[msg_l-1] == '\n')
            msg[msg_l-1] = '\0';

        // capture whole second when see long fgets times
        if (tty_cps) {                                          // tty can correct start based on baud
            if (fgets_ms > MSGGAP_DT) {                         // long time implies this is first of group
                unsigned msg_dt = (1000U*msg_l)/tty_cps;        // time to send the last message, millis
                msec0 = m1 - msg_dt - MSGGAP_DT;                // go back by msg time and gap time
            }
            if (debugLevel (DEBUG_NMEA, 1)) {
                unsigned cps = fgets_ms > 0 ? (1000U*msg_l)/fgets_ms : 0;
                Serial.printf ("NMEA: fgets %4u ms, %4u cps: %s\n", fgets_ms, cps, msg);
            }
        } else {
            if (fgets_ms > MSGGAP_DT)                           // long time implies this is first of group
                msec0 = m1 - MSGGAP_DT;                         // can only guess correction
            if (debugLevel (DEBUG_NMEA, 1))
                Serial.printf ("NMEA: fgets %4u ms: %s\n", fgets_ms, msg);
        }

        // crack RMC, post if ok and offset has been set
        if (memcmp (&msg[3], "RMC", 3) == 0) {
            pthread_mutex_lock (&nmea_lock);
            if (crackRMC (msg) && msec0) {
                nmea_ok = true;
                nmea_msec0 = msec0;
            } else 
                nmea_ok = false;
            pthread_mutex_unlock (&nmea_lock);
        }

        // restart fgets timer
        m0 = millis();                                          // measure fgets() time
    }

    // get what happened then close
    bool err = ferror(fp);
    bool eof = feof(fp);
    fclose (fp);

    // the end
    if (err)
        fatalError ("NMEA %s: %s\n", path, strerror(errno));
    else if (eof)
        fatalError ("NMEA %s: disconnected\n", path);
    else
        fatalError ("NMEA %s: unknown error\n", path);

    // lint
    return (NULL);
}

/* make sure the NMEA listener thread is running, harmless if already running.
 */
static void checkNMEAThread (void)
{
    // out fast if already running, but we only try once
    static bool thread_ok;
    if (thread_ok)
        return;
    thread_ok = true;

    pthread_t tid;
    int e = pthread_create (&tid, NULL, theNMEAThread, NULL);
    if (e)
        fatalError ("NMEA: pthread_create %s", strerror(e));

    // make sure it's running successfully
    bool running_ok = false;
    for (int i = 0; i < 5 && !running_ok; i++) {
        pthread_mutex_lock (&nmea_lock);
        running_ok = nmea_ok;
        pthread_mutex_unlock (&nmea_lock);
        if (!running_ok) {
            Serial.printf ("NMEA: waiting for valid sentence\n");
            wdDelay(1000);
        }
    }
    if (!running_ok)
        fatalError ("NMEA: no valid sentences found\n");
}

/* return whether the given file seems suitable for NMEA.
 * if trouble return false with short reason
 */
bool checkNMEAFilename (const char *fn, Message &ynot)
{
    // expand env
    char path[1000];
    if (!expandENV (fn, path, sizeof(path))) {
        ynot.printf ("%s env expansion failed", fn);
        return(false);
    }

    // try to open
    int fd = open (path, O_RDONLY|O_NONBLOCK);

    // done if can't even open
    if (fd < 0) {
        ynot.printf ("%s: %s", path, strerror(errno));
        return(false);
    }

    // do a little more if it appears to be a tty
    if (isatty (fd)) {
        if (!setNMEAtty (fd, ynot)) {
            close (fd);
            return (false);
        }
    }

    // ok!
    close (fd);
    return (true);
}

/* get UTC else 0
 */
time_t getNMEAUTC (void)
{
    checkNMEAThread();

    time_t t = 0;

    pthread_mutex_lock (&nmea_lock);

    if (nmea_ok) {

        // start with RMC time
        t = nmea_time;

        // advance to next whole second
        int ms_late = millis() - nmea_msec0;
        int s_late = ms_late/1000;
        t += s_late;
        ms_late -= 1000*s_late;
        if (ms_late > 0) {
            int us_sleep = 1000000 - 1000*ms_late;
            usleep (us_sleep);
            if (debugLevel (DEBUG_NMEA, 1))
                Serial.printf ("NMEA: sync sleep %d ms\n", us_sleep/1000);
            t += 1;
        }
    }

    pthread_mutex_unlock (&nmea_lock);

    return (t);
}

/* get ll else false
 */
bool getNMEALatLong (LatLong &ll)
{
    checkNMEAThread();

    bool ok = false;
    pthread_mutex_lock (&nmea_lock);
    if (nmea_ok) {
        ll.lat_d = nmea_lat;
        ll.lng_d = nmea_lng;
        ok = true;
    }
    pthread_mutex_unlock (&nmea_lock);
    if (ok)
        ll.normalize();
    return (ok);
}

/* occasionaly refresh DE from NMEA if enabled and we moved a little.
 */
void updateNMEALoc()
{
        // out fast if not configured
        if (!useNMEALoc())
            return;

        // not crazy often
        static uint32_t to_t;
        if (!timesUp (&to_t, FOLLOW_DT))
            return;

        // get loc
        LatLong ll;
        if (!getNMEALatLong(ll))
            return;

        // dist from DE
        float miles = ERAD_M*de_ll.GSD(ll);

        // engage if large enough, consider 6 char grid is 5'x2.5' or about 6x3 mi at equator
        if (miles > FOLLOW_MIND)
            newDE (ll, NULL);
}

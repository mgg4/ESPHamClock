/* manage list of cities.
 */

#include "HamClock.h"


// name of server file containing cities and local cache name
static const char cities_page[] = "/cities2.txt"; // changed in 2.81
static const char cities_fn[] = "cities2.txt";

// refresh period
#define CITIES_DT       (7L*24*3600)            // max cache age, secs
#define CITIES_SZ       1000                    // min believable size
#define CRETRY_DT       (60)                    // retry preiod on error, secs

// malloced sorted kdtree
static KD3Node *city_malloc;                    // malloced array
static KD3Node *city_root;                      // tree root somewhere within the array
static int n_cities;                            // number in use

// pixel width of longest city
static int max_city_len;


/* read list of cities, create kdtree.
 */
static void readCities()
{
        // insure reset
        if (city_malloc) {
            freeKD3NodeTree (city_malloc, n_cities);
            city_root = NULL;
            city_malloc = NULL;
            n_cities = 0;
        }

        // open file from cache
        FILE *fp = openCachedFile (cities_fn, cities_page, CITIES_DT, CITIES_SZ);
        if (!fp)
            return;

        // read each city and build temp lists of location and name
        char **names = NULL;                    // temp malloced list of persistent malloced names
        LatLong *lls = NULL;                    // temp malloced list of locations
        int n_malloced = 0;                     // number malloced in each list
        char line[200];
        max_city_len = 0;

        while (fgets (line, sizeof(line), fp)) {

            // crack
            char name[101];                     // N.B. match length-1 in sscanf
            float lat, lng;
            if (sscanf (line, "%f, %f, \"%100[^\"]\"", &lat, &lng, name) != 3)
                continue;

            // grow lists if full
            if (n_cities + 1 > n_malloced) {
                n_malloced += 100;
                names = (char **) realloc (names, n_malloced * sizeof(char *));
                lls = (LatLong *) realloc (lls, n_malloced * sizeof(LatLong));
                if (!names || !lls)
                    fatalError ("alloc cities: %d", n_malloced);
            }

            // add to lists
            names[n_cities] = strdup (name);
            LatLong &new_ll = lls[n_cities];
            new_ll.lat_d = lat;
            new_ll.lng_d = lng;
            new_ll.normalize();

            // capture longest name
            int name_l = strlen (name);
            if (name_l > max_city_len)
                max_city_len = name_l;

            // good
            n_cities++;

        }

        Serial.printf ("Cities: found %d\n", n_cities);

        // finished with file
        fclose (fp);

        // build tree -- N.B. can not build as we read because realloc could move left/right pointers
        city_malloc = (KD3Node *) calloc (n_cities, sizeof(KD3Node));
        if (!city_malloc && n_cities > 0)
            fatalError ("alloc cities tree: %d", n_cities);
        for (int i = 0; i < n_cities; i++) {
            KD3Node *kp = &city_malloc[i];
            ll2KD3Node (lls[i], kp);
            kp->data = (void*) names[i];
        }

        // finished with temporary lists -- names themselves live forever
        free (names);
        free (lls);

        // transform array into proper KD3 tree
        city_root = mkKD3NodeTree (city_malloc, n_cities, 0);
}

/* return name of nearest city and location but no farther than MAX_CSR_DIST from the given ll, else NULL.
 * also report back longest city length for drawing purposes unless NULL.
 */
const char *getNearestCity (const LatLong &ll, LatLong &city_ll, int *max_cl)
{
        // refresh
        static time_t next_update;
        if (myNow() > next_update) {
            readCities();
            if (!city_root) {
                next_update = myNow() + CRETRY_DT;
                Serial.printf ("%s failed, next update in %d\n", cities_fn, CRETRY_DT);
            } else
                next_update = myNow() + CITIES_DT;
        }
        if (!city_root) {
            Serial.printf ("still no %s after refresh attempt", cities_fn);
            return (NULL);
        }

        // pass back max length if interested
        if (max_cl)
            *max_cl = max_city_len;

        // search
        KD3Node seach_city;
        ll2KD3Node (ll, &seach_city);
        const KD3Node *best_city = NULL;
        float best_dist = 0;
        int n_visited = 0;
        nearestKD3Node (city_root, &seach_city, 0, &best_city, &best_dist, &n_visited);
        // printf ("**** visted %d\n", n_visited);

        // report results if successful
        best_dist = nearestKD3Dist2Miles (best_dist);   // convert to miles
        if (best_dist < MAX_CSR_DIST) {
            KD3Node2ll (*best_city, &city_ll);
            return ((char*)(best_city->data));
        } else {
            return (NULL);
        }

}


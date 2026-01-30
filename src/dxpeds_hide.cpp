/* manage the DXPeds hidden list
 *
 * N.B. the file format is consistent with fetchDXPeds.pl
 */

#include "HamClock.h"


// the cache is loaded on first use, then the file is updated whenever it changes.
#define MAX_PEDS_LINE   256                             // generous max length of any one line
static const char hide_fn[] = "dxpeditions-hide.txt";   // file containing hidden peds lines
static char **hidden_cache;                             // malloced list of malloced lines
static int n_hidden_cache;                              // n lines in cache


/* remove the given cache index from memory
 */
static void rmCacheIndex (int i)
{
    // remove from list, don't realloc to avoid the case where length goes to zero
    memmove (&hidden_cache[i], &hidden_cache[i+1], (--n_hidden_cache - i) * sizeof(char*));
}

/* add the given line to the memory cache
 */
static void addCacheLine (const char *line)
{
    hidden_cache = (char **) realloc (hidden_cache, (n_hidden_cache+1) * sizeof(char*));
    if (!hidden_cache)
        fatalError ("no memory for %d dxpeds hidden cache", n_hidden_cache+1);
    hidden_cache[n_hidden_cache++] = strdup (line);
}

/* return whether the given cache line is still actve
 */
static bool cacheLineIsActive (const char *line)
{
    long start_t, end_t;
    if (sscanf (line, "%ld,%ld", &start_t, &end_t) != 2)
        fatalError ("bogus DXPeds cache line: %s", line);
    return (end_t > myNow());
}

/* format the given DXPedEntry the same way as fetchDXPeds.pl.
 */
static void formatCacheLine (const DXPedEntry &p, char *line, size_t line_l)
{
    snprintf (line, line_l, "%ld,%ld,%s,%s,%s", (long)p.start_t, (long)p.end_t, p.loc, p.call, p.url);
}

/* load memory cache from file, ok if absent but fatal if permission problem.
 */
static void loadHiddenCache(void)
{
    // reset memory cache
    for (int i = 0; i < n_hidden_cache; i++)
        free (hidden_cache[i]);
    free (hidden_cache);
    hidden_cache = NULL;

    // open file, no worries unless due to permission
    FILE *fp = fopenOurs (hide_fn, "r");
    if (!fp) {
        if (errno == EACCES)
            fatalError ("%s: %s", hide_fn, strerror(errno));
        return;
    }

    // load cache from current list
    char line[MAX_PEDS_LINE];
    while (fgets (line, sizeof(line), fp)) {
        chompString (line);
        if (cacheLineIsActive(line))
            addCacheLine (line);
    }

    // finished with file
    fclose (fp);

    if (debugLevel (DEBUG_DXPEDS, 1))
        Serial.printf ("DXP: loaded %d hidden\n", n_hidden_cache);
}

/* save cache, removing passed along the way
 */
static void saveHiddenCache (void)
{
    // create file -- any failure is fatal
    FILE *fp = fopenOurs (hide_fn, "w");
    if (!fp)
        fatalError ("%s: %s", hide_fn, strerror(errno));

    // write cache, removing any expired along the way
    for (int i = 0; i < n_hidden_cache; i++) {
        if (cacheLineIsActive (hidden_cache[i]))
            fprintf (fp, "%s\n", hidden_cache[i]);
        else {
            rmCacheIndex(i);
            i -= 1;                             // visit i again
        }
    }

    // check for IO trouble before closing
    bool trouble = feof(fp) || ferror(fp);
    fclose (fp);
    if (trouble)
        fatalError ("%s: %s", hide_fn, strerror(errno));

    if (debugLevel (DEBUG_DXPEDS, 1))
        Serial.printf ("DXP: saved %d hidden\n", n_hidden_cache);
}

/* return whether the given dxpedition line is to be hidden
 */
bool isDXPedsHidden (const char *line)
{
    // load if first time
    if (!hidden_cache)
        loadHiddenCache();

    // check against list
    for (int i = 0; i < n_hidden_cache; i++)
        if (strcmp (line, hidden_cache[i]) == 0)
            return (true);
    return (false);
}

/* return whether the given dxpedition is to be hidden
 */
bool isDXPedsHidden (const DXPedEntry *dxp)
{
    // load if first time
    if (!hidden_cache)
        loadHiddenCache();

    char line[MAX_PEDS_LINE];
    formatCacheLine (*dxp, line, sizeof(line));

    return (isDXPedsHidden (line));
}

/* return current number of hidden peds.
 */
int nDXPedsHidden (void)
{
    // load if first time
    if (!hidden_cache)
        loadHiddenCache();

    return (n_hidden_cache);
}

/* add the given ped to hiddle list, both in memory and the file
 */
void addDXPedsHidden (const DXPedEntry *dxp)
{
    // load if first time
    if (!hidden_cache)
        loadHiddenCache();

    char line[MAX_PEDS_LINE];
    formatCacheLine (*dxp, line, sizeof(line));
    addCacheLine(line);
    saveHiddenCache();
}

/* remove the given ped from the hidden list, both in memory and the file
 */
void rmDXPedsHidden (const DXPedEntry *dxp)
{
    // load if first time
    if (!hidden_cache)
        loadHiddenCache();

    char line[MAX_PEDS_LINE];
    formatCacheLine (*dxp, line, sizeof(line));
    for (int i = 0; i < n_hidden_cache; i++) {
        if (strcmp (line, hidden_cache[i]) == 0) {
            rmCacheIndex (i);
            saveHiddenCache();
            break;
        }
    }
}

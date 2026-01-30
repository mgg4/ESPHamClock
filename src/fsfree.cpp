/* file system utility functions.
 */


#include "HamClock.h"


// checkFSFull() configuration
#define CHECKFULL_MS    (600*1000UL)            // check interval, millis
#define MINFS_FREE      (100*1000000LL)         // min free bytes
#define FATAL_FREE      (1*1000000LL)           // fatally full free bytes
#define CLEAN_NAME      "bmp"                   // cache file names to remove
#define CLEAN_AGE       (1*3600*24)             // cache age to remove, secs


#include <sys/statvfs.h>

/* pass back bytes capacity and used of the dir containing our working directory.
 * return whether successful.
 */
bool getFSSize (DSZ_t &cap, DSZ_t &used)
{
    const char *dir = our_dir.c_str();
    struct statvfs buf;
    memset (&buf, 0, sizeof(buf));

    if (statvfs (dir, &buf) < 0) {
        Serial.printf ("FS: Can not get info for FS containing %s: %s\n", dir, strerror(errno));
        return (false);
    }

    // macOS says statvfs may not return any valid information ?!
    cap = (DSZ_t)buf.f_bsize * (DSZ_t)buf.f_blocks;
    if (cap == 0) {
        Serial.printf ("FS: bogus statvfs block size 0\n");
        return (false);
    }

    used = cap - (DSZ_t)buf.f_bsize * (DSZ_t)buf.f_bfree;
    return (true);
}

/* try to remove a diag file, return whether successful
 */
static bool rmDiagFile (void)
{
    // try in reverse order, but never back to the current one
    for (int i = N_DIAG_FILES; --i >= 1; ) {
        std::string dp = our_dir + diag_files[i];
        const char *fn = dp.c_str();
        if (unlink (fn) == 0) {
            Serial.printf ("FS: removed %s\n", fn);
            return (true);
        }
    }
    return (false);

}

/* return if file system is dangerously low, cleaning up if it helps.
 * also pass back flag whether disk really is full
 */
bool checkFSFull (bool &really)
{
    static uint32_t check_ms;

    if (timesUp (&check_ms, CHECKFULL_MS)) {
        DSZ_t cap, used;
        while (getFSSize (cap, used) && cap - used < MINFS_FREE
                                                && (rmDiagFile() || cleanCache (CLEAN_NAME, CLEAN_AGE)))
            continue;
        if (getFSSize (cap, used) && cap - used < MINFS_FREE) {
            Serial.printf ("FS: disk %llu bytes free = %llu%% full\n", cap - used, 100*used/cap);
            really = cap - used < FATAL_FREE;
            return (true);
        }
    }

    really = false;
    return (false);
}


/* qsort-style compare two FS_Info by name
 */
static int FSInfoNameQsort (const void *p1, const void *p2)
{
        return (strcmp (((FS_Info *)p1)->name, ((FS_Info *)p2)->name));
}


/* produce a listing of the map storage directory.
 * return malloced array and malloced name -- caller must free() -- else NULL if not available.
 */
FS_Info *getConfigDirInfo (int *n_info, char **fs_name, DSZ_t *fs_size, DSZ_t *fs_used)
{
        // get basic fs info
        DSZ_t cap, used;
        if (!getFSSize (cap, used))
            return (NULL);

        // pass back basic info
        const char *wd = our_dir.c_str();
        *fs_name = strdup (wd);
        *fs_size = cap;
        *fs_used = used;

        // build each entry
        FS_Info *fs_array = NULL;
        int n_fs = 0;
        DIR *dirp = opendir (wd);
        if (!dirp)
            fatalError ("can not open %s: %s", wd, strerror(errno));

        struct dirent *dp;
        while ((dp = readdir(dirp)) != NULL) {

            // extend array
            fs_array = (FS_Info *) realloc (fs_array, (n_fs+1)*sizeof(FS_Info));
            if (!fs_array)
                fatalError ("alloc file name %d failed", n_fs);
            FS_Info *fip = &fs_array[n_fs++];

            // store name
            quietStrncpy (fip->name, dp->d_name, sizeof(fip->name));

            // get full name for stat()
            char full[2000];
            snprintf (full, sizeof(full), "%s/%s", wd, dp->d_name);
            struct stat sbuf;
            if (stat (full, &sbuf) < 0) {
                Serial.printf ("FS: %s: %s\n", full, strerror(errno));
                continue;
            }

            // store UNIX time
            time_t t = fip->t0 = sbuf.st_mtime;

            // store as handy date string too
            int yr = year(t);
            int mo = month(t);
            int dy = day(t);
            int hr = hour(t);
            int mn = minute(t);
            int sc = second(t);
            snprintf (fip->date, sizeof(fip->date), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                                yr, mo, dy, hr, mn, sc);

            // store file length
            fip->len = sbuf.st_size;
        }

        closedir (dirp);

        // nice sorted order
        qsort (fs_array, n_fs, sizeof(FS_Info), FSInfoNameQsort);

        // ok
        *n_info = n_fs;
        return (fs_array);
}

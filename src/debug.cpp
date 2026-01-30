/* set and get debug levels via RESTful web commands.
 */

#include "HamClock.h"


// system name and current level
typedef struct {
    const char *name;                   // fifo name
    int level;                          // current level
} DebugLevel;


#define X(a,b) {b, 0},                  // expands DEBUG_SUBSYS to each name string and initial level
static DebugLevel db_level[DEBUG_SUBSYS_N] = {
    DEBUG_SUBSYS
};
#undef X

/* set the given debug subsystem to the given level.
 * name can be short
 */
bool setDebugLevel (const char *name, int level)
{
    // only require match up to name length
    const size_t n_l = strlen (name);

    // find match, beware ambiquities
    DebugLevel *dlp = NULL;
    for (int i = 0; i < DEBUG_SUBSYS_N; i++) {
        if (strncasecmp (name, db_level[i].name, n_l) == 0) {
            if (dlp) {
                Serial.printf ("DEBUG: %s is ambiguous\n", name);
                return (false);
            }
            dlp = &db_level[i];
        }
    }

    if (dlp) {
        dlp->level = level;
        Serial.printf ("DEBUG: set %s=%d\n", dlp->name, dlp->level);
        return (true);
    }

    Serial.printf ("DEBUG: set unknown %s\n", name);
    return (false);
}

void getDebugs (const char *names[DEBUG_SUBSYS_N], int levels[DEBUG_SUBSYS_N])
{
    for (int i = 0; i < DEBUG_SUBSYS_N; i++) {
        names[i] = db_level[i].name;
        levels[i] = db_level[i].level;
    }
}


void prDebugLevels (WiFiClient &client, int indent)
{
    char line[100];

    snprintf (line, sizeof(line), "%-*s%s=%d\n", indent, "Debugs", db_level[0].name, db_level[0].level);
    client.print (line);
    for (int i = 1; i < DEBUG_SUBSYS_N; i++) {
        snprintf (line, sizeof(line), "%*s%s=%d\n", indent, "", db_level[i].name, db_level[i].level);
        client.print (line);
    }
}


/* return whether to activate the given debug subsys at the given level of detail.
 */
bool debugLevel (DebugSubsys s, int level)
{
    return (db_level[s].level >= level);
}

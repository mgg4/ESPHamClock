/* implement a watch list of prefixes and bands.
 * the WatchList class compiles a watch list spec then can determine whether a DXSpot qualifies.
 */



#include "HamClock.h"


// watch list keywords to mean "match if spot is NOT in ADIF file DXCC or prefix"
char NOTADIFDXCC_KW[] = "NADXCC";
char NOTADIFPREF_KW[] = "NAPref";
char NOTADIFBAND_KW[] = "NABand";
char NOTADIFGRID_KW[] = "NAGrid";


/* class to manage watch lists
 */
class WatchList {

    private:

        // specified frequency spread
        typedef struct {
            float min_kHz, max_kHz;
        } FreqRange;

        // handy
        typedef char *Prefix;

        // all info contained in one specification, ie, a component separated by comma
        typedef struct {
            FreqRange *freqs;           // malloced list of each freq range
            int n_freqs;                // freqs[n]
            Prefix *prefs;              // malloced list of each prefix
            int n_prefs;                // prefs[n]
            bool noadif_dxcc;           // true if spot DXCC is not be in ADIF file
            bool noadif_band;           // true if spot band is not be in ADIF file
            bool noadif_grid;           // true if spot grid is not be in ADIF file
            bool noadif_pref;           // true if spot prefix is not be in ADIF file
        } OneSpec;

        OneSpec *specs;                 // malloced array of specifications
        int n_specs;                    // specs[n]
        bool finished_spec;             // flag set when spec is closed after parsing


        /* add the given explicit frequency range.
         * N.B. no error checking
         */
        void addFreqRange (float min_kHz, float max_kHz)
        {
            if (debugLevel (DEBUG_WL, 1))
                Serial.printf ("WLIST: setting range %g - %g kHz\n", min_kHz, max_kHz);

            // start new spec if flagged earlier or we are first
            checkNewSpec();

            // grow freq list to current spec
            OneSpec &s = specs[n_specs-1];                      // current spec
            s.freqs = (FreqRange *) realloc (s.freqs, (s.n_freqs + 1) * sizeof(FreqRange));
            FreqRange &f = s.freqs[s.n_freqs++];
            f.min_kHz = min_kHz;
            f.max_kHz = max_kHz;
        }

        /* add the given string prefix
         * N.B. no error checking
         */
        void addPrefix (const char *p)
        {
            if (debugLevel (DEBUG_WL, 1))
                Serial.printf ("WLIST: setting prefix %s\n", p);

            // start new spec if flagged earlier or we are first
            checkNewSpec();

            // grow prefix list and add malloced copy to current spec
            OneSpec &s = specs[n_specs-1];                      // current spec
            s.prefs = (Prefix *) realloc (s.prefs, (s.n_prefs + 1) * sizeof(Prefix));
            s.prefs[s.n_prefs++] = strdup (p);
        }

        /* add all freq ranges for the given subband, return count.
         * N.B. units will be "m" when want whole band.
         * N.B. already qualified units so 
         */
        int addSubBand (HamBandSetting h, const char *units)
        {
            // findBandEdges wants NULL mode for whole band
            const char *mode = strcasecmp (units, "m") == 0 ? NULL : units;

            #define _MAX_SUBB 10
            float min_kHz[_MAX_SUBB], max_kHz[_MAX_SUBB];
            int n_e = findBandEdges (h, mode, min_kHz, max_kHz, _MAX_SUBB);
            for (int i = 0; i < n_e; i++)
                addFreqRange (min_kHz[i], max_kHz[i]);
            return (n_e);
        }

        /* upon entry all we know is token contains '-'.
         * must be in format n1-n2{subband,m,mhz} else error.
         */
        bool checkBandRange (const char *token, Message &ynot)
        {
            // should find 2 numbers separated by - followed by units
            float f1, f2;
            char units[10];
            int n_s = sscanf (token, "%g-%g%9s", &f1, &f2, units);
            if (n_s < 2) {
                ynot.set ("busted range");
                return (false);
            }
            if (n_s == 2) {
                ynot.set ("no units");
                return (false);
            }

            // kindly accommodate f1 > f2
            if (f1 > f2) {
                float tmp_f = f1;
                f1 = f2;
                f2 = tmp_f;
            }

            // if MHz just add directly
            if (strcasecmp (units, "MHz") == 0) {
                // sanity check
                float f1_kHz = f1*1e3F;                         // MHz to kHz
                float f2_kHz = f2*1e3F;
                if (findHamBand (f1_kHz) == HAMBAND_NONE || findHamBand (f2_kHz) == HAMBAND_NONE) {
                    ynot.printf ("limits? %g-%g", f1, f2);
                    return (false);
                }
                addFreqRange (f1_kHz, f2_kHz);
                return (true);
            }

            // other than MHz check for valid units, including "m"
            if (strcasecmp (units, "m") && !isValidSubBand(units)) {
                ynot.printf ("subband? %s", units);
                return (false);
            }

            // otherwise numbers must be integer meters
            int f1_int = (int)f1;
            int f2_int = (int)f2;
            if (f1_int != f1) {
                ynot.printf ("fraction? %g", f1);
                return (false);
            }
            if (f2_int != f2) {
                ynot.printf ("fraction? %g", f2);
                return (false);
            }

            // find each ham band selection
            int h1 = (int)findHamBand (f1_int);                 // type int means meters
            if (h1 == HAMBAND_NONE) {
                ynot.printf ("band? %g", f1);
                return (false);
            }
            int h2 = (int)findHamBand (f2_int);                 // type int means meters
            if (h2 == HAMBAND_NONE) {
                ynot.printf ("band? %g", f2);
                return (false);
            }

            // kindly accommodate h1 > h2
            if (h1 > h2) {
                int tmp_h = h1;
                h1 = h2;
                h2 = tmp_h;
            }

            // advance h2 to accommodate such weirdness as 80-80m
            h2++;

            // scan [h1, h2)
            int n_ranges = 0;
            for (int i = h1; i < h2; i++)
                n_ranges += addSubBand ((HamBandSetting)i, units);
            if (n_ranges == 0) {
                ynot.set ("no matches");
                return (false);
            }

            // ok !
            return (true);
        }

        /* determine if token is a single band spec (ie, no '-') else a generic prefix.
         * return whether it looks reasonable either way.
         */
        bool checkPrefix (const char *token, Message &ynot)
        {
            // first we scan for any weird chars other than / at the end
            for (const char *tp = token; *tp; tp++) {
                if (!isalnum(*tp) && strcmp (tp, "/")) {
                    ynot.printf ("char? %c", *tp);
                    return (false);
                }
            }

            // check for leading number which MIGHT be a band (doesn't have to be)
            char *modeptr;
            int n = strtol (token, &modeptr, 10);
            if (*modeptr == '\0') {
                // pure number
                ynot.printf ("number? %s", token);
                return (false);
            }
            if (modeptr > token) {
                // leading number is followed by something, is it a valid band?
                HamBandSetting h = findHamBand(n);
                if (h == HAMBAND_NONE) {
                    // not valid band not a prefix either if over 9
                    if (n >= 10) {
                        ynot.printf ("band? %d", n);
                        return (false);
                    }
                } else {
                    // number is a valid band, now check if what followed is valid mode
                    if (strcasecmp (modeptr, "m") == 0 || isValidSubBand(modeptr)) {
                        // valid mode too so we're commited to this not being a generic prefix
                        if (addSubBand (h, modeptr) == 0) {
                            ynot.set ("no match");
                            return (false);
                        }
                        return (true);
                    }
                }
            }

            // that just leaves a generic prefix. Keith doesn't think it's worth checking in cty so .. ok!

            addPrefix (token);

            return (true);
        }

        /* add spec if first or finished previous
         */
        void checkNewSpec(void)
        {
            if (finished_spec || specs == NULL) {
                specs = (OneSpec *) realloc (specs, (n_specs + 1) * sizeof(OneSpec));
                OneSpec *sp = &specs[n_specs++];
                memset (sp, 0, sizeof(*sp));
                finished_spec = false;
            }
        }

        /* indicate the current specification is complete, prepare to begin another.
         */
        void finishedSpec(void)
        {
            if (debugLevel (DEBUG_WL, 2) && !finished_spec)
                Serial.printf ("WLIST: spec complete\n");
            finished_spec = true;
        }

        /* reclaim and reset all storage references
         */
        void resetStorage()
        {
            if (specs) {
                for (int i = 0; i < n_specs; i++) {
                    OneSpec &s = specs[i];
                    if (s.freqs) {
                        free (s.freqs);
                        s.freqs = NULL;
                        s.n_freqs = 0;
                    }
                    if (s.prefs) {
                        for (int j = 0; j < s.n_prefs; j++)
                            free (s.prefs[j]);
                        free (s.prefs);
                        s.prefs = NULL;
                        s.n_prefs = 0;
                    }
                }
                free (specs);
                specs = NULL;
                n_specs = 0;
            }
        }


    public:

        /* constructor: init state
         */
        WatchList (void)
        {
            specs = NULL;
            n_specs = 0;
            finished_spec = false;
        }

        /* destructor: release storage
         */
        ~WatchList (void)
        {
            resetStorage();
        }

        /* print the watch list data structure components
         * N.B. wl_id == WLID_N means anon
         */
        void print (WatchListId wl_id)
        {
            // name and state
            const char *name = wl_id == WLID_N ? "anon" : getWatchListName(wl_id);
            char state[WLA_MAXLEN];
            if (wl_id == WLID_N)
                strcpy (state, ":");
            else
                (void) getWatchListState (wl_id, state);
            Serial.printf ("WLIST %s %s %d specs:\n", name, state, n_specs);

            // each spec
            for (int i = 0; i < n_specs; i++) {

                Serial.printf ("  Spec %d:\n", i+1);

                OneSpec &s = specs[i];

                Serial.printf ("    %d Prefixes:", s.n_prefs);
                for (int j = 0; j < s.n_prefs; j++)
                    printf (" %s%s", j > 0 ? "or " : "", s.prefs[j]);
                printf ("\n");

                Serial.printf ("    %d Freq ranges:", s.n_freqs);
                for (int j = 0; j < s.n_freqs; j++)
                    printf (" %g-%g", s.freqs[j].min_kHz, s.freqs[j].max_kHz);
                printf ("\n");

                char buf[200];
                size_t bl = snprintf (buf, sizeof(buf), "    ADIF:");
                if (s.noadif_dxcc)
                    bl += snprintf (buf+bl, sizeof(buf)-bl, " %s", NOTADIFDXCC_KW);
                if (s.noadif_band)
                    bl += snprintf (buf+bl, sizeof(buf)-bl, " %s", NOTADIFBAND_KW);
                if (s.noadif_pref)
                    bl += snprintf (buf+bl, sizeof(buf)-bl, " %s", NOTADIFPREF_KW);
                if (s.noadif_grid)
                    bl += snprintf (buf+bl, sizeof(buf)-bl, " %s", NOTADIFGRID_KW);
                Serial.printf ("%s\n", buf);
            }
        }

        /* determine whether the given spot is allowed.
         * check each spec:
         *   if spec contains any frequencies spot freq must lie within at least one.
         *   if spec contains any prefixes spot call must match at least one.
         *   if spec contains any NO_ADIF there must be no matching entries in ADIF.
         */
        bool onList (const DXSpot &spot)
        {
            // always work with the dx portion if split
            char home_call[NV_CALLSIGN_LEN];
            char dx_call[NV_CALLSIGN_LEN];
            splitCallSign (spot.tx_call, home_call, dx_call);

            // decide whether this is really a proper split call (just containing / is not proof)?
            bool two_part = strcasecmp (home_call, dx_call) != 0;

            // search for any spec that is a match
            bool spot_matches = false;
            for (int i = 0; i < n_specs && !spot_matches; i++) {
                OneSpec &s = specs[i];

                // match any prefix or none?
                bool match_pref = false;
                if (s.n_prefs) {
                    for (int j = 0; j < s.n_prefs; j++) {
                        // only match against the dx portion of a two-part call
                        const char *pref = s.prefs[j];
                        const char *pref_slash = strchr (pref, '/');
                        size_t pref_len = pref_slash ? pref_slash - pref : strlen(pref);    // sans / if any
                        bool dx_match = strncasecmp (dx_call, pref, pref_len) == 0;
                        bool home_match = strncasecmp (home_call, pref, pref_len) == 0;
                        if ((pref_slash && two_part && dx_match) || (!pref_slash && home_match)) {
                            match_pref = true;
                            break;
                        }
                    }
                } else
                    match_pref = true;          // always true if no prefix tests


                // match any freq range or none?
                bool match_freq = false;
                if (s.n_freqs) {
                    for (int j = 0; j < s.n_freqs; j++) {
                        if (s.freqs[j].min_kHz <= spot.kHz && spot.kHz <= s.freqs[j].max_kHz) {
                            match_freq = true;
                            break;
                        }
                    }
                } else
                    match_freq = true;          // always true if no freq tests

                // match all no-ADIF requirements?
                bool match_noadif = (s.noadif_dxcc || s.noadif_grid || s.noadif_pref || s.noadif_band)
                            ? !onADIFList (spot, s.noadif_dxcc, s.noadif_grid, s.noadif_pref, s.noadif_band)
                            : true;

                // good?
                if (match_pref && match_freq && match_noadif)
                    spot_matches = true;

                if (debugLevel (DEBUG_WL, 2)) {
                    Serial.printf ("WLIST: %s (%s %s) %g: prefix?%c freq?%c ADIF?%c: %s\n",
                        spot.tx_call, home_call, dx_call, spot.kHz,
                        match_pref ? 'Y' : 'N',
                        match_freq ? 'Y' : 'N',
                        match_noadif ? 'Y' : 'N',
                        spot_matches ? "match!" : "no match");
                }
            }

            return (spot_matches);
        }

        /* try to compile the given watch list specifications.
         * if trouble return false with short excuse in ynot[].
         */
        bool compile (const char *wl_specs, Message &ynot)
        {
            // fresh start
            resetStorage();

            // delimiters, implicitly including EOS
            static char delims[] = " ,";

            // walk the spec looking for each token and its delimiter.
            // N.B. rely on loop body to break at EOS
            size_t tok_len;
            for (const char *wl_walk = wl_specs; true; wl_walk += tok_len) {

                // next token extends to next delim
                tok_len = strcspn (wl_walk, delims);

                // skip if empty, but note EOS or when ',' indicates this section is finished
                if (tok_len == 0) {
                    char delim = wl_walk[0];
                    if (delim == '\0') {
                        // no more
                        finishedSpec();
                        break;
                    }
                    if (delim == ',')
                        // finished this spec then start next
                        finishedSpec();
                    tok_len = 1;                                        // skip delim
                    continue;
                }

                // handy token as a separate trimmed upper-case string
                char token[50];
                snprintf (token, sizeof(token), "%.*s", (int)tok_len, wl_walk);
                strtoupper (token);

                // ADIF?
                if (strcasecmp (token, NOTADIFDXCC_KW) == 0) {
                    if (!checkADIFFilename (getADIFilename(), ynot))
                        return (false);
                    checkNewSpec();                                     // insure at least 1
                    OneSpec &s = specs[n_specs-1];                      // current spec
                    s.noadif_dxcc = true;
                    continue;
                }
                if (strcasecmp (token, NOTADIFBAND_KW) == 0) {
                    if (!checkADIFFilename (getADIFilename(), ynot))
                        return (false);
                    checkNewSpec();                                     // insure at least 1
                    OneSpec &s = specs[n_specs-1];                      // current spec
                    s.noadif_band = true;
                    continue;
                }
                if (strcasecmp (token, NOTADIFPREF_KW) == 0) {
                    if (!checkADIFFilename (getADIFilename(), ynot))
                        return (false);
                    checkNewSpec();                                     // insure at least 1
                    OneSpec &s = specs[n_specs-1];                      // current spec
                    s.noadif_pref = true;
                    continue;
                }
                if (strcasecmp (token, NOTADIFGRID_KW) == 0) {
                    if (!checkADIFFilename (getADIFilename(), ynot))
                        return (false);
                    checkNewSpec();                                     // insure at least 1
                    OneSpec &s = specs[n_specs-1];                      // current spec
                    s.noadif_grid = true;
                    continue;
                }

                // if contains '-' then freq range?
                if (strchr (token, '-')) {
                    if (!checkBandRange (token, ynot))
                        return (false);
                    continue;
                }

                // something else?
                if (!checkPrefix (token, ynot))
                    return (false);
            }

            // disallow empty
            if (n_specs == 0) {
                ynot.set ("empty");
                return (false);
            }

            // yah!
            return (true);
        }

};


/* return whether the given string contains any of the ADIF watch list keywords
 */
static bool anyWLADIFKW (const char *s)
{
     return (strcistr (s, NOTADIFDXCC_KW) || strcistr (s, NOTADIFGRID_KW)
          || strcistr (s, NOTADIFBAND_KW) || strcistr (s, NOTADIFPREF_KW));

}

/* one per list
 */
static WatchList wlists[WLID_N];


/* return whether wl_id is safe to use
 */
bool wlIdOk (WatchListId wl_id)
{
    return (wl_id >= 0 && wl_id < WLID_N);
}


/* decide how, or whether, to display the given DXSpot with respect to the given watch list
 * N.B. call freshenADIFFile() before calling this to avoid recursion when it checks it's own WL.
 */
WatchListShow checkWatchListSpot (WatchListId wl_id, const DXSpot &dxsp)
{
    if (!wlIdOk(wl_id))
        fatalError ("checkWatchList bogus %d", (int)wl_id);

    switch (getWatchListState (wl_id, NULL)) {
    case WLA_OFF:
        return (WLS_NORM);              // spot always qualfies so no need to check
    case WLA_FLAG:
        return (wlists[wl_id].onList(dxsp) ? WLS_HILITE : WLS_NORM);
    case WLA_ONLY:
        return (wlists[wl_id].onList(dxsp) ? WLS_NORM : WLS_NO);
    case WLA_NOT:
        return (wlists[wl_id].onList(dxsp) ? WLS_NO : WLS_NORM);
    case WLA_N:
        break;
    }

    // lint
    return (WLS_NORM);
}

/* compile the given string on the given watch list.
 * return false with brief reason if trouble.
 */
bool compileWatchList (WatchListId wl_id, const char *wl_str, Message &ynot)
{
    if (!wlIdOk(wl_id))
        fatalError ("compileWatchList bogus is %d", (int)wl_id);

    // avoid recusive references
    if (wl_id == WLID_ADIF && anyWLADIFKW (wl_str)) {
        ynot.set ("ADIF recursion");
        return (false);
    }

    Serial.printf ("WLIST %s: compiling %s\n", getWatchListName(wl_id), wl_str);
    bool ok = wlists[wl_id].compile (wl_str, ynot);

    if (ok)
        wlists[wl_id].print (wl_id);
    else
        Serial.printf ("WLIST %s: %s\n", getWatchListName(wl_id), ynot.get());

    return (ok);
}

/* like compileWatchList but uses a temporary anonymous WatchList just to capture any compile errors.
 * N.B. _menu_text->label contains the WL state name, ->text contains the watchlist
 * N.B. just feign success if WLA_OFF.
 */
static bool compileTestWatchList (struct _menu_text *tfp, Message &ynot)
{
    // just say yes if state is Off
    if (lookupWatchListState(tfp->label) == WLA_OFF)
        return (true);

    // temporary compiler
    WatchList anon_wl;
    
    Serial.printf ("WLIST anon: compiling %s\n", tfp->text);
    bool ok = anon_wl.compile (tfp->text, ynot);

    Serial.printf ("WLIST anon: compiled %s\n", tfp->text);
    if (ok)
        anon_wl.print (WLID_N);
    else
        Serial.printf ("WLIST anon: state %s compiled %s: %s\n", tfp->label, tfp->text, ynot.get());

    return (ok);
}

/* like compileTestWatchList but for use only for compiling WL_ADIF.
 */
static bool compileTestADIFWatchList (struct _menu_text *tfp, Message &ynot)
{
    // just say yes if state is Off
    if (lookupWatchListState(tfp->label) == WLA_OFF)
        return (true);

    // avoid recusive references
    if (anyWLADIFKW (tfp->text)) {
        ynot.set ("ADIF recursion");
        return (false);
    }

    // temporary compiler
    WatchList anon_wl;
    
    Serial.printf ("WLIST anon: compiling %s\n", tfp->text);
    bool ok = anon_wl.compile (tfp->text, ynot);

    if (ok)
        anon_wl.print (WLID_N);
    else
        Serial.printf ("WLIST anon: state %s compiled %s: %s\n", tfp->label, tfp->text, ynot.get());

    return (ok);
}


/* handy consolidation of setting up a MENU_TEXT for editing watch lists.
 * N.B. caller must free mi.text
 */
void setupWLMenuText (WatchListId wl_id, MenuText &mt, char wl_state[WLA_MAXLEN])
{
    memset (&mt, 0, sizeof(mt));                                // easy defaults
    getWatchListState (wl_id, wl_state);                        // get current state name
    getWatchList (wl_id, &mt.text, &mt.t_mem);                  // N.B. must free mt.text
    mt.label = wl_state;                                        // mutable label memory
    mt.l_mem = WLA_MAXLEN;                                      // max label len
    mt.text_fp = wl_id == WLID_ADIF ? compileTestADIFWatchList : compileTestWatchList; // watchlist test
    mt.label_fp = rotateWatchListState;                         // label cycler
    mt.to_upper = true;                                         // all uc
    mt.c_pos = mt.w_pos = 0;                                    // start at left
}


/* remove all extraneous blanks and commas IN PLACE from the given watch list specification.
 * this means all leading, trailing and consecutive blanks or commas.
 * return s (because good stuff has been shifted to the beginning).
 */
char *wlCompress (char *s)
{
    #define COMMA    ','
    #define BLANK    ' '
    #define EOS      '\0'
    #define SAVE(c)  *s_to++ = (c)

    typedef enum {
        WLC_SEEKLD,             // seeking left delimiter
        WLC_SEEKRD,             // seeking right delimiter
        WLC_INTOKEN,            // in token chars
    } WLCState;
    WLCState wls = WLC_SEEKLD;

    bool saw_comma = false;
    char *s_from = s;
    char *s_to = s;

    for (char c = *s_from; c != EOS; c = *++s_from) {
        switch (wls) {
        case WLC_SEEKLD:
            if (c != BLANK && c != COMMA) {
                SAVE(c);
                wls = WLC_INTOKEN;
            }
            break;
        case WLC_SEEKRD:
            if (c == COMMA) {
                saw_comma = true;
            } else if (c != BLANK) {
                if (saw_comma) {
                    SAVE(COMMA);
                    saw_comma = false;
                } else
                    SAVE(BLANK);
                SAVE(c);
                wls = WLC_INTOKEN;
            }
            break;
        case WLC_INTOKEN:
            if (c == COMMA) {
                saw_comma = true;
                wls = WLC_SEEKRD;
            } else if (c == BLANK) {
                saw_comma = false;
                wls = WLC_SEEKRD;
            } else {
                SAVE(c);
            }
            break;
        }
    }
    *s_to = EOS;

    return (s);

    #undef COMMA
    #undef BLANK
    #undef EOS
    #undef SAVE
}

/* misc string utils
 */

#include "HamClock.h"


/* string hash, commonly referred to as djb2
 * see eg http://www.cse.yorku.ca/~oz/hash.html
 */
uint32_t stringHash (const char *str)
{
    uint32_t hash = 5381;
    int c;

    while ((c = *str++) != '\0')
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

/* convert any upper case letter in str to lower case IN PLACE
 */
char *strtolower (char *str)
{
    char *str0 = str;
    for (char c = *str; c != '\0'; c = *++str)
        *str = tolower(c);
    return (str0);
}

/* convert any lower case letter in str to upper case IN PLACE
 */
char *strtoupper (char *str)
{
    char *str0 = str;
    for (char c = *str; c != '\0'; c = *++str)
        *str = toupper(c);
    return (str0);
}

/* my own portable implementation of strcasestr
 */
const char *strcistr (const char *haystack, const char *needle)
{
    // empty needle is always considered a match to the entire haystack
    if (needle[0] == '\0')
        return (haystack);

    for (; *haystack; haystack++) {
        const char *h = haystack;
        for (const char *n = needle; ; n++, h++) {
            if (*n == '\0')
                return (haystack);
            if (toupper(*h) != toupper(*n))
                break;
        }
    }
    return (NULL);
}

/* split str into as many as max_tokens whitespace-delimited tokens.
 * tokens[i] points to the i'th token within str, which has been null-terminated IN PLACE.
 * return number of tokens.
 * N.B any additional tokens are silently lost.
 */
int strtokens (char *str, char *tokens[], int max_tokens)
{
    int n_tokens = 0;
    char *string = str;
    char *token;

    while (n_tokens < max_tokens && (token = strtok (string, " \t")) != NULL) {
        tokens[n_tokens++] = token;
        string = NULL;
    }

    return (n_tokens);
}

/* remove leading and trailing white space IN PLACE; return str.
 */
char *strTrimEnds (char *str)
{
    if (!str)
        return (NULL);

    // find first non-blank going left-to-right
    char *fnb = str;
    while (isspace(*fnb))
        fnb++;

    // find last non-blank going right-to-left. N.B. will end up < str if all blanks
    char *lnb = &str[strlen(str)-1];
    while (lnb >= str && isspace(*lnb))
        --lnb;

    // printf ("%d %d\n", (int)(fnb-str), (int)(lnb-str));

    // check if str was nothing but blanks
    if (lnb < fnb) {
        *str = '\0';
        return (str);
    }

    // compute n non-blank
    size_t nnb = lnb-fnb+1;

    // copy [fnb,lnb] back to str
    memmove (str, fnb, nnb);

    // add EOS
    str[nnb] = '\0';

    return (str);
}

/* remove all leading, trailing and embedded duplicate white space IN PLACE; return str.
 */
char *strTrimAll (char *str)
{
    if (!str)
        return (NULL);

    char *to = str;
    char *from = str;
    while (isspace(*from))
        from++;
    for ( ; *from; from++)
        if (!isspace (*from) || (from[1] && !isspace(from[1])))
            *to++ = *from;
    *to = '\0';
    return (str);
}


/* return whether the given string contains at least one character as defined by the ctype.h test function.
 */
static bool strHasCType (const char *s, int (fp)(int))
{
    for (; *s; s++)
        if ((*fp) (*s))
            return (true);
    return (false);
}

/* return whether the given string contains at least one alpha character
 */
bool strHasAlpha (const char *s)
{
    return (strHasCType (s, isalpha));
}

/* return whether the given string contains at least one digit.
 */
bool strHasDigit (const char *s)
{
    return (strHasCType (s, isdigit));
}

/* return whether the given string contains at least one punct
 */
bool strHasPunct (const char *s)
{
    return (strHasCType (s, ispunct));
}

/* return whether the given string contains at least one space
 */
bool strHasSpace (const char *s)
{
    return (strHasCType (s, isspace));
}

/* strncpy that avoids g++ fussing when from is longer than to
 */
void quietStrncpy (char *to, const char *from, int len)
{
    snprintf (to, len, "%.*s", len-1, from);
}

/* qsort-style comparison of two strings in ascending order (a b c d : a will be first in result)
 */
int qsAString (const void *v1, const void *v2)
{
    return (strcmp (*(char**)v1, *(char**)v2));
}

/* qsort-style comparison of two strings in descending order (a b c d : a will be last in result)
 */
int qsDString (const void *v1, const void *v2)
{
    return (strcmp (*(char**)v2, *(char**)v1));
}


/* return the bounding box of the given string in the current font.
 */
void getTextBounds (const char str[], uint16_t *wp, uint16_t *hp)
{
    int16_t x, y;
    tft.getTextBounds (str, 100, 100, &x, &y, wp, hp);
}

/* return width in pixels of the given string in the current font
 */
uint16_t getTextWidth (const char str[])
{
    uint16_t w, h;
    getTextBounds (str, &w, &h);
    return (w);
}

/* remove trailing nl and/or cr IN PLACE
 */
void chompString (char *str)
{
    char *cr = strchr (str, '\r');
    if (cr)
        *cr = '\0';
    char *nl = strchr (str, '\n');
    if (nl)
        *nl = '\0';
}

/* shorten str IN PLACE as needed to be less than maxw pixels wide.
 * return final width in pixels.
 */
uint16_t maxStringW (char *str, uint16_t maxw)
{
    uint8_t strl = strlen (str);
    uint16_t bw = 0;

    while (strl > 0 && (bw = getTextWidth(str)) >= maxw)
        str[--strl] = '\0';

    return (bw);
}

/* copy from_str to to_str changing all from_char to to_char.
 * unlike strncpy(3) to_str is always terminated with EOS, ie copying stops after from_str EOS or (to_len-1).
 * the two strings should not overlap but they may be the same.
 */
void strncpySubChar (char to_str[], const char from_str[], char to_char, char from_char, int to_len)
{
    if (to_len > 0) {
        while (--to_len > 0) {
            char c = *from_str++;
            *to_str++ = (c == from_char) ? to_char : c;
            if (c == '\0')
                return;
        }
        *to_str = '\0';
    }
}


/* copy fn to with_env expanding any $ENV.
 * return whether all $ENV found and result fits.
 */
bool expandENV (const char *fn, char *with_env, size_t max_len)
{
    size_t with_len = 0;

    for (const char *fn_walk = fn; *fn_walk != '\0'; fn_walk++) {

        // check for embedded terms else just copy

        if (*fn_walk == '$') {

            // temporarily copy caps/digits/_ to with_env starting just passed the $
            char *with_env_start = &with_env[with_len];         // start of env copy within with_env
            int env_len = 0;
            for (const char *env = fn_walk+1; isupper(*env) || isdigit(*env) || *env=='_'; env++) {
                if (with_len + env_len < max_len-1)             // leave room for final EOS
                    with_env_start[env_len++] = *env;
                else {
                    Serial.printf ("ENV: expanding %s exceeds max %ld\n", fn, (long)max_len);
                    return (false);
                }
            }

            // terminate and look up
            with_env_start[env_len] = '\0';
            char *env_value = getenv (with_env_start);
            if (!env_value) {
                Serial.printf ("ENV: %s within %s not found\n", with_env_start, fn);
                return (false);
            }

            // copy env_value into with_env for real starting at the $ but sans EOS
            size_t env_value_len = strlen (env_value);
            memcpy (with_env_start, env_value, env_value_len);

            // increment length by env value length and advance walk by env name length
            with_len += env_value_len;
            fn_walk += env_len;

        } else if (*fn_walk == '~') {

            // expand ~ as $HOME
            char *home = getenv ("HOME");
            if (!home) {
                Serial.printf ("ENV: no HOME to expand %s\n", fn);
                return (false);
            }

            // copy to with_env
            while (*home != '\0') {
                if (with_len < max_len - 1)
                    with_env[with_len++] = *home++;
                else {
                    Serial.printf ("ENV: expanding ~ in %s exceeds max %ld\n", fn, (long)max_len);
                    return (false);
                }
            }

        } else {

            if (with_len < max_len - 1)
                with_env[with_len++] = *fn_walk;
            else {
                Serial.printf ("ENV: expanded %s exceeds max %ld\n", fn, (long)max_len);
                return (false);
            }
        }
    }

    // add EOS
    with_env[with_len] = '\0';

    // ok
    return (true);
}

/* handy means to break time interval into HHhMM or MM:SS given dt in hours.
 * return each component and the appropriate separate, the expectation is the time
 * can then be printed using *printf (%02d%c%02d", a, sep, b);
 */
void formatSexa (float dt_hrs, int &a, char &sep, int &b)
{
    if (dt_hrs < 1) {
        // next event is less than 1 hour away, show time in MM:SS
        dt_hrs *= 60;                           // dt_hrs is now minutes
        sep = ':';
    } else {
        // next event is at least an hour away, show time in HH:MM
        sep = 'h';
    }

    // same hexa conversion either way
    a = (int)dt_hrs;
    b = (int)((dt_hrs-(int)dt_hrs)*60);
}

/* format a representation of the given age into line[] exactly cols chars long.
 * return line.
 */
char *formatAge (time_t age, char *line, int line_l, int cols)
{
    // eh?
    if (age < 0) {
        Serial.printf ("formatAge(%ld,%d) resetting negative age to zero\n", (long)age, cols);
        age = 0;
    }

    switch (cols) {

    case 1:

        // show a few symbols
        if (age < 10*60)
            snprintf (line, line_l, " ");
        else if (age < 60*60)
            snprintf (line, line_l, "m");
        else
            snprintf (line, line_l, "h");
        break;

    case 2:

        // show minutes up thru 59 else hrs up thru 9 then +
        if (age < 60*60-30)
            snprintf (line, line_l, "%2d", (int)(age/60));
        else if (age < 10*60*60-1800)
            snprintf (line, line_l, "%dh", (int)(age/(60*60)));
        else
            strcpy (line, "+");
        break;

    case 3:

        // show 2 digits then s, m, h, d, M, y
        if (age < 60) {
            snprintf (line, line_l, "%2lds", (long)age);
        } else if (age < (60*60)) {
            snprintf (line, line_l, "%2ldm", (long)age/60);
        } else if (age < 3600*24) {
            snprintf (line, line_l, "%2ldh", (long)age/3600);
        } else if (age < 100L*3600*24) {
            snprintf (line, line_l, "%2ldd", (long)age/(3600L*24));
        } else if (age < 365L*3600*24) {
            snprintf (line, line_l, "%2ldM", (long)age/(31L*3600*24));
        } else {
            snprintf (line, line_l, "%2ldy", (long)age/(365L*3600*24));
        }

        break;

    case 4:

        // show seconds thru years(!)
        if (age < 60) {
            snprintf (line, line_l, "%3lds", (long)age);
        } else if (age < (60*60)) {
            snprintf (line, line_l, "%3ldm", (long)age/60);
        } else if (age < (24*60*60-1800)) {
            float hours = age/3600.0F;
            if (hours < 9.95F)
                snprintf (line, line_l, "%3.1fh", hours);
            else
                snprintf (line, line_l, "%3.0fh", hours);
        } else if (age < 3600L*(24*365-12)) {
            float days = age/(3600*24.0F);
            if (days < 9.95F)
                snprintf (line, line_l, "%3.1fd", days);
            else
                snprintf (line, line_l, "%3.0fd", days);
        } else {
            float years = age/(3600*24*365.0F);
            if (years < 9.95F)
                snprintf (line, line_l, "%3.1fy", years);
            else
                snprintf (line, line_l, "%3.0fy", years);
        }
        break;

    default:
        fatalError("formatAge bad cols %d", cols);
        break;
    }

    return (line);
}

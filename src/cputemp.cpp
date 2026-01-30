/* a collection of various means to measure cpu temperature.
 * if you know of a way not listed please contact me.
 */

#include "HamClock.h"


// history record
#define TLOOKSSANE(t)   ((t) > 10 && (t) < 300)                 // whether t, in C, looks sane
#define SAMPLE_DT       30                                      // minimum sampling period, s
#define HISTORY_DT      (24*3600)                               // total history to save, s
#define N_CPUTEMPS      (HISTORY_DT/SAMPLE_DT)                  // n of records to keep
typedef struct {
    float t_C;                                                  // temp, C
    time_t t;                                                   // sample time, 0 if not yet taken
} CPUTempRecord;
static CPUTempRecord temps_cq[N_CPUTEMPS];                      // circular q, next goes at temps_head
static int temps_head;                                          // index to oldest record == next insert



#if defined(_IS_LINUX) || defined(_IS_APPLE_x86)

/* add t to temps_cq[] if at least SAMPLE_DT since previous
 */
static void logCPUTemp (float t)
{
    // skip samples until time is sensible
    time_t now = myNow();
    if (now < 1000000000)                                       // 2001-09-09T01:46:40
        return;

    // skip if too soon since previous
    CPUTempRecord &prev_tr = temps_cq[(temps_head+(N_CPUTEMPS-1))%N_CPUTEMPS];
    if (prev_tr.t > 0 && now < prev_tr.t + SAMPLE_DT)
        return;

    // add at head
    CPUTempRecord &new_tr = temps_cq[temps_head];
    new_tr.t_C = t;
    new_tr.t = now;

    // advance head, beware wrap
    if (++temps_head == N_CPUTEMPS)
        temps_head = 0;
}

#endif // defined(_IS_LINUX) || defined(_IS_APPLE_x86)





#if defined (_IS_LINUX)

/* just read from /sys
 */

/* retrieve cpu temp in C, return whether possible.
 */
bool getCPUTemp (float &t_C)
{
    static char sys_temp[] = "/sys/class/thermal/thermal_zone0/temp";
    static bool sys_failed;

    bool ok = false;

    if (!sys_failed) {
        FILE *fp = fopen (sys_temp, "r");
        if (fp) {
            char buf[50];
            while (!ok && fgets (buf, sizeof(buf), fp)) {
                char *endp;
                int v = (int) strtol (buf, &endp, 10);
                if (v && *endp == '\n') {
                    t_C = v * 1.0e-3F;
                    ok = TLOOKSSANE(t_C);
                    if (ok)
                        logCPUTemp(t_C);
                    else {
                        Serial.printf ("CPUTEMP: %s: preposterous value %g Cn", sys_temp, t_C);
                        sys_failed = true;
                    }
                }
            }
            fclose (fp);
        } else {
            Serial.printf ("CPUTEMP: %s: %s\n", sys_temp, strerror(errno));
            sys_failed = true;
        }
    }

    return (ok);
}



#elif defined (_IS_APPLE_x86)


/* no known method that works with Apple Silicon systems
 */


#ifdef _APPLE_FIXES_POWERMETRICS_TO_EXIT_WHEN_WRITES_TO_STDOUT_FAIL

/* can't use this because if hamclock exits then powermetrics continues to run forever because it does not
 * check for write errors to stdout
 */


/* fork/exec powermetrics forever, non-blocking read from its stdout for each reading
 */

/* retrieve cpu temp in C, return whether possible
 */
bool getCPUTemp (float &t_C)
{
    static char cmd[] = "/usr/bin/powermetrics";        // full path of program
    static bool cmd_failed;                             // set if attempt failed
    static FILE *cmd_fp;                                // read from cmd
    static int cmd_pid;                                 // child process
    bool ok = false;

    // out fast if aleady tried and failed
    if (cmd_failed)
        return (false);

    // run and connect to cmd on first call
    if (!cmd_fp) {

        /* fork/exec cmd[].
         * can't use popen() because we need to retain su status.
         * arrange to read its stdout on cmd_fp.
         */

        int p[2];
        if (pipe (p) < 0) {
            Serial.printf ("CPUTEMP: pipe(): %s\n", strerror(errno));
        } else {
            int cmd_pid = fork();
            if (cmd_pid < 0) {
                Serial.printf ("CPUTEMP: fork(): %s\n", strerror(errno));
                close (p[0]);
                close (p[1]);

            } else if (cmd_pid == 0) {
                // child -- arrange to send output of cmd to p[1]

                // close read end in child so parent is the only reader
                close (p[0]);

                // arrange stdio and close all others
                dup2 (open("/dev/null", O_RDONLY), 0);
                dup2 (p[1], 1);
                dup2 (p[1], 2);
                for (int i = 3; i < 100; i++)
                    close(i);
                execl (cmd, "powermetrics", "--samplers", "smc", "-i", "1000", "-b", "0", NULL);
                _exit(errno);           // pass back errno as exit status

            } else {
                // parent reads from cmd on p[0]

                // close write end in parent so cmd is the only writer
                close (p[1]);

                // set non-blocking
                if (fcntl (p[0], F_SETFL, fcntl (p[0], F_GETFL, 0) | O_NONBLOCK) < 0) {
                    close (p[0]);
                    Serial.printf ("CPUTEMP: fcntl(O_NONBLOCK): %s\n", strerror(errno));
                } else
                    cmd_fp = fdopen (p[0], "r");
            }
        }
    }

    // read temp from cmd, clean up if trouble 
    if (cmd_fp) {

        // drain cmd messages looking for temperature
        char buf[50];
        while (fgets (buf, sizeof(buf), cmd_fp)) {
            if (sscanf (buf, "CPU die temperature: %f", &t_C) == 1)
                ok = true;
        }

        // EAGAIN just means nothing more to read from non-blocking fd
        if ((ferror(cmd_fp) || feof(cmd_fp)) && (errno != EAGAIN && errno != EWOULDBLOCK)) {

            // wait for child
            int stat;
            if (waitpid (cmd_pid, &stat, 0) < 0)
                Serial.printf ("CPUTEMP: waitpid(%d): %s\n", cmd_pid, strerror(errno));
            else if (WIFEXITED(stat)) {
                // process exits with status errno
                Serial.printf ("CPUTEMP: %s: exec failed: %s\n", cmd, strerror(WEXITSTATUS(stat)));
            } else 
                Serial.printf ("CPUTEMP: %s: unexpected exit\n", cmd);

            // give up permanently
            cmd_failed = true;
            fclose (cmd_fp);            // also closes p[0]
        }
    }

    return (ok);
}


#else // ! _APPLE_FIXES_POWERMETRICS_TO_EXIT_WHEN_WRITES_TO_STDOUT_FAIL

// start a fresh instance of powermetrics for every read -- use this until apple fixes powermetrics


/* retrieve cpu temp in C, return whether possible
 */
bool getCPUTemp (float &t_C)
{
    static char cmd[] = "/usr/bin/powermetrics";
    static bool cmd_failed;

    bool ok = false;

    if (!cmd_failed) {

        // run cmd[], can't use popen because we need to retain su status

        int p[2];
        if (pipe (p) < 0)
            Serial.printf ("CPUTEMP: pipe(): %s\n", strerror(errno));
        else {
            int pid = fork();
            if (pid < 0) {
                Serial.printf ("CPUTEMP: fork(): %s\n", strerror(errno));
                close (p[0]);
                close (p[1]);

            } else if (pid == 0) {
                // child -- arrange to send output of cmd to p[1]

                // close read end in child so parent is the only reader
                close (p[0]);

                // arrange stdio and close all others
                dup2 (open("/dev/null", O_RDONLY), 0);
                dup2 (p[1], 1);
                dup2 (p[1], 2);
                for (int i = 3; i < 100; i++)
                    close(i);
                execl (cmd, "powermetrics", "--samplers", "smc", "-i", "1", "-n", "1", NULL);
                _exit(errno);           // pass back errno as exit status

            } else {
                // parent reads from cmd on p[0]

                // close write end in parent so cmd is the only writer
                close (p[1]);

                // handy stdio
                FILE *fp = fdopen (p[0], "r");
                if (fp) {
                    char buf[50];
                    while (!ok && fgets (buf, sizeof(buf), fp)) {
                        if (sscanf (buf, "CPU die temperature: %f", &t_C) == 1) {
                            ok = TLOOKSSANE(t_C);
                            if (ok)
                                logCPUTemp(t_C);
                            else {
                                Serial.printf ("CPUTEMP: %s: preposterous value %g Cn", cmd, t_C);
                                break;
                            }
                        }
                    }
                    fclose (fp);        // also closes p[0]
                } else
                    close (p[0]);

                // wait for child
                int stat;
                if (waitpid (pid, &stat, 0) < 0) {
                    Serial.printf ("CPUTEMP: %s waitpid(%d): %s\n", cmd, pid, strerror(errno));
                    ok = false;
                } else if (WIFEXITED(stat)) {
                    // process exits with status errno
                    int x = WEXITSTATUS(stat);
                    if (x != 0) {
                        Serial.printf ("CPUTEMP: %s: %s\n", cmd, strerror(x));
                        ok = false;
                    }
                } else {
                    Serial.printf ("CPUTEMP: %s: unexpected exit\n", cmd);
                    ok = false;
                }
            }
        }

        if (!ok) {
            Serial.printf ("CPUTEMP: giving up on %s\n", cmd);
            cmd_failed = true;

        }
    }

    return (ok);
}

#endif // _APPLE_FIXES_POWERMETRICS_TO_EXIT_WHEN_WRITES_TO_STDOUT_FAIL


#else // not supported

/* retrieve cpu temp in C, return whether possible
 */
bool getCPUTemp (float &t_C)
{
    (void) t_C;
    return (false);
}

#endif



/* plot cpu history since start_tm.
 * return whether there were any entries even older.
 */
static bool plotCPUTempSince (time_t start_tm)
{
    // split into x and y in time order starting from oldest at head, skipping empty enties
    float x[N_CPUTEMPS], y[N_CPUTEMPS];
    time_t now = myNow();
    int n_data = 0;
    bool found_older = false;
    for (int i = 0; i < N_CPUTEMPS; i++) {
        CPUTempRecord &tr = temps_cq[(temps_head+i) % N_CPUTEMPS];      // head is oldest
        if (tr.t > 0) {                                                 // 0's have never been used
            if (tr.t >= start_tm) {                                     // skip entries too old
                x[n_data] = tr.t - now;                                 // age, seconds until know scale
                y[n_data] = showTempC() ? tr.t_C : 1.8F*tr.t_C + 32;
                n_data++;
            }  else
                found_older = true;                                     // found valid entry older than start
        }
    }
    
    // bale if too few to plot
    if (n_data < 2)
        return (found_older);

    // rescale to nice time
    const char *scale_label;
    float scale_factor;
    float age = -x[0];                                                  // time span, now seconds > 0
    if (age < 60) {
        scale_label = "Age, seconds";
        scale_factor = 1;
    } else if (age < 3600) {
        scale_factor = 1.0F/60.0;
        scale_label = "Age, minutes";
    } else if (age < 24*3600) {
        scale_factor = 1.0F/3600.0;
        scale_label = "Age, hours";
    } else {
        scale_factor = 1.0F/SECSPERDAY;
        scale_label = "Age, days";
    }
    for (int i = 0; i < n_data; i++)
        x[i] *= scale_factor;

    // title
    char title[200];
    snprintf (title, sizeof(title), "CPU Temperature, %c", showTempC() ? 'C' : 'F');

    // go
    plotMapData (title, scale_label, x, y, n_data);

    // return whether there were any not plotted
    return (found_older);
}

/* plot cpu history
 */
void plotCPUTempHistory (void)
{
    // plot back no more than one hour then entire history if more farther back
    if (plotCPUTempSince (myNow() - 3600))
        (void) plotCPUTempSince (0);
}

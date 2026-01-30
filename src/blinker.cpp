/* manage perpetual threads dedicated to smooth LED blinking and digital polling.
 */

#include "HamClock.h"

// handy Hz to usecs
#define HZ_TO_US(hz)    (1000000/(hz))

// management info for each type of thread

typedef struct {
    int pin;                    // pin number
    int hz;                     // blink rate or one of BLINKER_*
    bool on_is_low;             // whether "on" means drive LOW
    bool running;               // set while supporting thread is running
    bool error;                 // thread failed, do not attempt restart
    bool disable;               // set to gracefully stop the thread
} ThreadBlinker;

typedef struct {
    int pin;                    // pin number
    bool running;               // set while supporting thread is running
    bool error;                 // thread failed, do not attempt restart
    bool disable;               // set to gracefully stop the thread
    bool value;                 // latest value
    bool prev_value;            // previous value, used to watch for edge
    bool latched;               // set when value is captured in latch_value; reset to repeat
    bool latched_value;         // value when latched held until unlatched
} MCPPoller;


// pool of thread io for each user pin, indexed by MCP23017Pins
static volatile ThreadBlinker blinkers[MCP_N_LINES];
static volatile MCPPoller pollers[MCP_N_LINES];



/* validate pin.
 */
static bool checkUserPin (int pin, Message &ynot)
{
    if (pin < 0 || pin >= MCP_N_LINES) {
        ynot.printf ("MCP pin must be %d..%d", 0, MCP_N_LINES-1);
        return (false);
    }
    return (true);
}



/**********************************************************************************************
 *
 * blinker writer
 *
 */


/* perpetual thread that blinks, or holds, an output according to shared hz value.
 */
static void * blinkerThread (void *vp)
{
    // get defining struct
    volatile ThreadBlinker &tb = *(ThreadBlinker *)vp;

    // get our loop delay
    int max_rate = mcp.maxPollRate();
    if (max_rate <= 0) {
        Serial.printf ("disabling pin %d blinker thread: max_rate = %d\n", tb.pin, max_rate);
        tb.error = true;
        return (NULL);
    }
    useconds_t delay_us = HZ_TO_US(max_rate);

    // ok
    tb.running = true;
            
    // init output pin to false
    mcp.pinMode (tb.pin, OUTPUT);
    mcp.digitalWrite (tb.pin, tb.on_is_low);

    // set our internal polling period and init counter there of.
    unsigned n_delay = 0;                                       // count of delay_us
    int prev_hz = BLINKER_UNKNOWN;                              // looking for changes avoids needless io
    int new_hz;                                                 // read tb.hz once per loop to avoid lock

    // forever until disabled
    while (!tb.disable) {

        usleep (delay_us);

        new_hz = tb.hz;

        if (new_hz == BLINKER_ON) {
            // constant on
            if (new_hz != prev_hz)
                mcp.digitalWrite (tb.pin, !tb.on_is_low);
            n_delay = 0;
        } else if (new_hz == BLINKER_OFF) {
            // constant off
            if (new_hz != prev_hz)
                mcp.digitalWrite (tb.pin, tb.on_is_low);
            n_delay = 0;
        } else {
            // blink
            unsigned rate_period_us = HZ_TO_US(new_hz)/2;       // 1 Hz is on and off in 1 s
            if (++n_delay*delay_us >= rate_period_us) {
                mcp.digitalWrite (tb.pin, !mcp.digitalRead (tb.pin));
                n_delay = 0;
            }
        }

        prev_hz = new_hz;
    }

    // finished
    tb.running = false;

    // return to passive input
    mcp.pinMode (tb.pin, INPUT_PULLUP);

    Serial.printf ("blinkerThread for pin %d exiting\n", tb.pin);
    return (NULL);
}

/* start a blinker thread for the given pin.
 * harmless to call multiple times with the same pin, but fatal if can't create a new thread.
 */
void startBinkerThread (int pin, bool on_is_low)
{
    Message ynot;
    if (!checkUserPin (pin, ynot))
        fatalError ("startBinkerThread: %s", ynot.get());
    volatile ThreadBlinker &tb = blinkers[pin];

    // start thread if not already and tried but failed
    if (!tb.running && !tb.error) {
        tb.pin = pin;
        tb.on_is_low = on_is_low;
        tb.hz = BLINKER_OFF;
        pthread_t tid;
        int e = pthread_create (&tid, NULL, blinkerThread, (void*)&tb); // make volatile again in thread
        if (e != 0)
            fatalError ("LED blinker thread for pin %d failed: %s", tb.pin, strerror(e));
        if (debugLevel (DEBUG_IO, 1))
            Serial.printf ("MCP: started blinker thread for pin %d\n", pin);
    }
}

/* set the given pin to the given desired rate
 */
void setBlinkerRate (int pin, int hz)
{
    Message ynot;
    if (!checkUserPin (pin, ynot))
        fatalError ("setBlinkerRate: %s", ynot.get());
    volatile ThreadBlinker &tb = blinkers[pin];

    if (debugLevel (DEBUG_IO, 1) && tb.hz != hz)
        printf ("setBlinkerRate pin %d  %d -> %d\n", pin, tb.hz, hz);

    tb.hz = hz;
}

/* tell a blinker thread to end
 */
void disableBlinker (int pin)
{
    Message ynot;
    if (!checkUserPin (pin, ynot))
        fatalError ("disableBlinker: %s", ynot.get());
    volatile ThreadBlinker &tb = blinkers[pin];

    // tell thread to quit, it was reset running
    tb.disable = true;
}

/**********************************************************************************************
 *
 * poller reader
 *
 */

/* thread that repeatedly reads a digital pin.
 * then client can query as needed with readMCPPoller without waiting inline.
 */
static void * pollerThread (void *vp)
{
    // get defining struct
    volatile MCPPoller &mp = *(MCPPoller *)vp;

    // get our loop delay
    int max_rate = mcp.maxPollRate();
    if (max_rate <= 0) {
        Serial.printf ("disabling pin %d poller thread: max_rate = %d\n", mp.pin, max_rate);
        mp.error = true;
        return (NULL);
    }
    useconds_t delay_us = HZ_TO_US(max_rate);

    // ok
    mp.running = true;

    // init input pin
    mcp.pinMode (mp.pin, INPUT_PULLUP);

    // forever until disabled
    while (!mp.disable) {
        mp.value = mcp.digitalRead (mp.pin) != 0;

        // capture change then hold until unlatched
        if (!mp.latched && mp.prev_value != mp.value) {
            // capture value before setting latched then we don't need a lock
            mp.latched_value = mp.value;
            mp.latched = true;
        }

        mp.prev_value = mp.value;

        usleep (delay_us);
    }

    // finished
    mp.running = false;

    Serial.printf ("wirepoller for pin %d exiting\n", mp.pin);
    return (NULL);
}

/* start a poller thread for the given input pin.
 * harmless to call multiple times with the same pin, but fatal if can't create a new thread.
 */
void startMCPPoller (int pin)
{
    Message ynot;
    if (!checkUserPin (pin, ynot))
        fatalError ("startMCPPoller: %s", ynot.get());
    volatile MCPPoller &mp = pollers[pin];

    // start thread if not already and tried but failed
    if (!mp.running && !mp.error) {
        mp.pin = pin;
        pthread_t tid;
        int e = pthread_create (&tid, NULL, pollerThread, (void*)&mp); // make volatile again in thread
        if (e != 0)
            fatalError ("LED poller thread for pin %d failed: %s", mp.pin, strerror(e));
        if (debugLevel (DEBUG_IO, 1))
            Serial.printf ("MCP: started poller thread for pin %d\n", pin);
    }
}

/* read the poller pin state from mp
 */
bool readMCPPoller (int pin)
{
    Message ynot;
    if (!checkUserPin (pin, ynot))
        fatalError ("readMCPPoller: %s", ynot.get());
    volatile MCPPoller &mp = pollers[pin];

    return (mp.running ? mp.value : true);              // hi default like Adafruit_MCP23X17::digitalRead
}

/* read the latched poller pin state from mp, if ready
 */
bool readLatchedMCPPoller (int pin, bool &value)
{
    Message ynot;
    if (!checkUserPin (pin, ynot))
        fatalError ("readLatchedMCPPoller: %s", ynot.get());
    volatile MCPPoller &mp = pollers[pin];

    if (mp.latched) {
        // value was captured before setting latched so we don't need a lock
        value = mp.latched_value;
        mp.latched = false;
        return (true);
    }
    return (false);
}

/* tell a poller thread to end
 */
void disableMCPPoller (int pin)
{
    Message ynot;
    if (!checkUserPin (pin, ynot))
        fatalError ("disableMCPPoller: %s", ynot.get());
    volatile MCPPoller &mp = pollers[pin];

    // tell thread to quit, it was reset running
    mp.disable = true;
}




/**********************************************************************************************
 *
 * user control via RESTful interface
 *
 */


/* set the given pin to output and blink rate to hz.
 * pin is WRT MCP23017Pins; hz may be any or BLINKER_ON or BLINKER_OFF.
 * return whether achieved and short reason if not.
 */
bool setUserGPIO (int pin, int hz, Message &ynot)
{
    // validate pin
    if (!checkUserPin (pin, ynot))
        return (false);
    volatile ThreadBlinker &tb = blinkers[pin];

    // validate hz
    if (hz != BLINKER_ON && hz != BLINKER_OFF && (hz <= 0 || hz > mcp.maxPollRate()/2)) {
        ynot.printf ("Hz must be %d .. %d", 1, mcp.maxPollRate()/2);
        return (false);
    }

    // stop if already an input
    if (pollers[pin].running)
        disableMCPPoller (pin);

    // start if new then engage
    if (!tb.running)
        startBinkerThread (pin, false);             // on-is-hi
    setBlinkerRate (pin, hz);

    // ok
    return (true);
}

/* get current state of the given input pin, either now or latched on last change.
 * pin is WRT MCP23017Pins.
 * return whether achieved and short reason if not.
 */
bool getUserGPIO (int pin, bool latched, bool &state, Message &ynot)
{
    // validate pin
    if (!checkUserPin (pin, ynot))
        return (false);
    volatile MCPPoller &mp = pollers[pin];

    // stop if already an output
    if (blinkers[pin].running)
        disableBlinker (pin);

    // start if new
    if (!mp.running)
        startMCPPoller (pin);

    // read as desired
    if (latched) {
        if (!readLatchedMCPPoller (pin, state)) {
            ynot.set ("no change");
            return (false);
        }
    } else
        state = readMCPPoller (pin);

    // ok
    return (true);
}

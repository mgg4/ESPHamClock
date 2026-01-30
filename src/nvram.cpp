/* code to help orgranize reading and writing bytes to EEPROM using named locations.
 * To add another value, add to both the NV_Name enum in HamClock.h and nv_sizes[] below.
 * N.B. be sure they stay in sync.
 *
 * Storage starts at NV_BASE. Items are stored contiguously without gaps. Each item begins with NV_COOKIE
 * followed by the number of bytes listed in nv_sizes[]. The latter is a uint8_t thus no one size can be
 * longer than 255 bytes.
 *
 * As a special case, we store two sets of path colors at the very end, just before FLASH_SECTOR_SIZE.
 * Each consists of NV_COOKIE followed by N_CSPR triples of uint8_t r, g and b bytes.
 */

#include "HamClock.h"

#define NV_BASE         55      // base address, move anywhere else to effectively start fresh
#define NV_COOKIE       0x5A    // magic cookie to decide whether a value is valid

// starting address of each color collection
#define CSEL_TBLSZ      (N_CSPR*3)                              // table size, bytes
#define CSEL_SETB_ADDR  (FLASH_SECTOR_SIZE - (1+CSEL_TBLSZ))    // NV_COOKIE then second table 
#define CSEL_SETA_ADDR  (CSEL_SETB_ADDR - (1+CSEL_TBLSZ))       // NV_COOKIE then first table


/* number of bytes for each NV_Name.
 * N.B. must be in the same order.
 */
static const uint8_t nv_sizes[NV_N] = {

    // 0
    4,                          // NV_TOUCH_CAL_A
    4,                          // NV_TOUCH_CAL_B
    4,                          // NV_TOUCH_CAL_C
    4,                          // NV_TOUCH_CAL_D
    4,                          // NV_TOUCH_CAL_E

    // 5
    4,                          // NV_TOUCH_CAL_F
    4,                          // NV_TOUCH_CAL_DIV
    1,                          // NV_DXMAX_N
    1,                          // NV_DE_TIMEFMT
    4,                          // NV_DE_LAT

    // 10
    4,                          // NV_DE_LNG
    4,                          // NV_PANE0ROTSET
    1,                          // NV_PLOT_0
    4,                          // NV_DX_LAT
    4,                          // NV_DX_LNG

    // 15
    4,                          // NV_DX_GRID_OLD
    2,                          // NV_CALL_FG
    2,                          // NV_CALL_BG
    1,                          // NV_CALL_RAINBOW
    1,                          // NV_PSK_SHOWDIST

    // 20
    4,                          // NV_UTC_OFFSET
    1,                          // NV_PLOT_1
    1,                          // NV_PLOT_2
    1,                          // NV_BRB_ROTSET_OLD
    1,                          // NV_PLOT_3

    // 25
    1,                          // NV_RSS_ON
    2,                          // NV_BPWM_DIM
    2,                          // NV_PHOT_DIM
    2,                          // NV_BPWM_BRIGHT
    2,                          // NV_PHOT_BRIGHT

    // 30
    1,                          // NV_LP
    1,                          // NV_UNITS
    1,                          // NV_LKSCRN_ON
    1,                          // NV_MAPPROJ
    1,                          // NV_ROTATE_SCRN_OLD

    // 35
    NV_WIFI_SSID_LEN,           // NV_WIFI_SSID
    NV_WIFI_PW_OLD_LEN,         // NV_WIFI_PASSWD_OLD
    NV_CALLSIGN_LEN,            // NV_CALLSIGN
    NV_SATNAME_LEN,             // NV_SAT1NAME
    1,                          // NV_DE_SRSS

    // 40
    1,                          // NV_DX_SRSS
    1,                          // NV_GRIDSTYLE
    2,                          // NV_DPYON_OLD
    2,                          // NV_DPYOFF_OLD
    NV_DXHOST_LEN,              // NV_DXHOST

    // 45
    2,                          // NV_DXPORT
    1,                          // NV_SWHUE
    4,                          // NV_TEMPCORR76
    NV_GPSDHOST_OLD_LEN,        // NV_GPSDHOST_OLD
    4,                          // NV_KX3BAUD

    // 50
    2,                          // NV_BCPOWER
    4,                          // NV_CD_PERIOD
    4,                          // NV_PRESCORR76
    2,                          // NV_BR_IDLE
    1,                          // NV_BR_MIN

    // 55
    1,                          // NV_BR_MAX
    4,                          // NV_DE_TZ
    4,                          // NV_DX_TZ
    NV_COREMAPSTYLE_LEN,        // NV_COREMAPSTYLE
    1,                          // NV_USEDXCLUSTER

    // 60
    1,                          // NV_USEGPSD
    1,                          // NV_LOGUSAGE
    1,                          // NV_LBLSTYLE
    NV_WIFI_PW_LEN,             // NV_WIFI_PASSWD
    1,                          // NV_NTPSET

    // 65
    NV_NTPHOST_OLD_LEN,         // NV_NTPHOST_OLD
    1,                          // NV_GPIOOK
    2,                          // NV_SAT1COLOR
    2,                          // NV_SAT2COLOR
    2,                          // NV_X11FLAGS

    // 70
    2,                          // NV_BCFLAGS
    NV_DAILYONOFF_LEN,          // NV_DAILYONOFF
    4,                          // NV_TEMPCORR77
    4,                          // NV_PRESCORR77
    2,                          // NV_SHORTPATHCOLOR

    // 75
    2,                          // NV_LONGPATHCOLOR
    2,                          // NV_PLOTOPS_OLD
    1,                          // NV_NIGHT_ON
    NV_DE_GRID_LEN,             // NV_DE_GRID
    NV_DX_GRID_LEN,             // NV_DX_GRID

    // 80
    2,                          // NV_GRIDCOLOR
    2,                          // NV_CENTERLNG
    1,                          // NV_NAMES_ON
    4,                          // NV_PANE1ROTSET
    4,                          // NV_PANE2ROTSET

    // 85
    4,                          // NV_PANE3ROTSET
    1,                          // NV_AUX_TIME
    2,                          // NV_DAILYALARM
    1,                          // NV_BC_UTCTIMELINE
    1,                          // NV_RSS_INTERVAL

    // 90
    1,                          // NV_DATEMDY
    1,                          // NV_DATEDMYYMD
    1,                          // NV_ROTUSE
    NV_ROTHOST_LEN,             // NV_ROTHOST
    2,                          // NV_ROTPORT

    // 95
    1,                          // NV_RIGUSE
    NV_RIGHOST_LEN,             // NV_RIGHOST
    2,                          // NV_RIGPORT
    NV_DXLOGIN_LEN,             // NV_DXLOGIN
    1,                          // NV_FLRIGUSE

    // 100
    NV_FLRIGHOST_LEN,           // NV_FLRIGHOST
    2,                          // NV_FLRIGPORT
    NV_DXCLCMD_OLD_LEN,         // NV_DXCMD0_OLD
    NV_DXCLCMD_OLD_LEN,         // NV_DXCMD1_OLD
    NV_DXCLCMD_OLD_LEN,         // NV_DXCMD2_OLD

    // 105
    NV_DXCLCMD_OLD_LEN,         // NV_DXCMD3_OLD
    1,                          // NV_DXCMDUSED_OLD
    1,                          // NV_PSK_MODEBITS
    4,                          // NV_PSK_BANDS
    2,                          // NV_160M_COLOR

    // 110
    2,                          // NV_80M_COLOR
    2,                          // NV_60M_COLOR
    2,                          // NV_40M_COLOR
    2,                          // NV_30M_COLOR
    2,                          // NV_20M_COLOR

    // 115
    2,                          // NV_17M_COLOR
    2,                          // NV_15M_COLOR
    2,                          // NV_12M_COLOR
    2,                          // NV_10M_COLOR
    2,                          // NV_6M_COLOR

    // 120
    2,                          // NV_2M_COLOR
    4,                          // NV_CSELDASHED
    1,                          // NV_BEAR_MAG
    1,                          // NV_WSJT_SETSDX_OLD
    1,                          // NV_WSJT_DX

    // 125
    2,                          // NV_PSK_MAXAGE
    1,                          // NV_WEEKMON
    1,                          // NV_BCMODE
    1,                          // NV_SDO
    1,                          // NV_SDOROT

    // 130
    1,                          // NV_ONTASPOTA_OLD
    1,                          // NV_ONTASSOTA_OLD
    2,                          // NV_BRB_ROTSET
    2,                          // NV_ROTCOLOR
    1,                          // NV_CONTESTS

    // 135
    4,                          // NV_BCTOA
    NV_ADIFFN_OLD_LEN,          // NV_ADIFFN_OLD
    NV_I2CFN_LEN,               // NV_I2CFN
    1,                          // NV_I2CON
    4,                          // NV_DXMAX_T

    // 140
    NV_POTAWLIST1_OLD_LEN,      // NV_POTAWLIST1_OLD
    1,                          // NV_SCROLLDIR
    1,                          // NV_SCROLLLEN_OLD
    NV_DXCLCMD_OLD_LEN,         // NV_DXCMD4_OLD
    NV_DXCLCMD_OLD_LEN,         // NV_DXCMD5_OLD

    // 145
    NV_DXCLCMD_OLD_LEN,         // NV_DXCMD6_OLD
    NV_DXCLCMD_OLD_LEN,         // NV_DXCMD7_OLD
    NV_DXCLCMD_OLD_LEN,         // NV_DXCMD8_OLD
    NV_DXCLCMD_OLD_LEN,         // NV_DXCMD9_OLD
    NV_DXCLCMD_OLD_LEN,         // NV_DXCMD10_OLD

    // 150
    NV_DXCLCMD_OLD_LEN,         // NV_DXCMD11_OLD
    2,                          // NV_DXCMDMASK
    1,                          // NV_DXWLISTMASK
    1,                          // NV_RANKSW_OLD
    1,                          // NV_NEWDXDEWX

    // 155
    1,                          // NV_WEBFS
    1,                          // NV_ZOOM
    2,                          // NV_PANX
    2,                          // NV_PANY
    1,                          // NV_POTAWLISTMASK_OLD

    // 160
    NV_SOTAWLIST1_OLD_LEN,      // NV_SOTAWLIST1_OLD
    4,                          // NV_ONCEALARM
    1,                          // NV_ONCEALARMMASK
    1,                          // NV_PANEROTP
    1,                          // NV_SHOWPIP

    // 165
    1,                          // NV_MAPROTP
    2,                          // NV_MAPROTSET
    1,                          // NV_GRAYDPY
    1,                          // NV_SOTAWLISTMASK_OLD
    1,                          // NV_ADIFWLISTMASK

    // 170
    NV_DXWLIST_LEN,             // NV_DXWLIST
    NV_ADIFWLIST_LEN,           // NV_ADIFWLIST
    1,                          // NV_ADIFSORT
    4,                          // NV_ADIFBANDS_OLD
    NV_POTAWLIST_OLD_LEN,       // NV_POTAWLIST_OLD

    // 175
    NV_SOTAWLIST_OLD_LEN,       // NV_SOTAWLIST_OLD
    NV_ADIFFN_LEN,              // NV_ADIFFN
    NV_NTPHOST_LEN,             // NV_NTPHOST
    NV_GPSDHOST_LEN,            // NV_GPSDHOST
    NV_NMEAFILE_LEN,            // NV_NMEAFILE

    // 180
    1,                          // NV_USENMEA
    2,                          // NV_NMEABAUD
    1,                          // NV_BCTOABAND
    1,                          // NV_BCRELBAND
    1,                          // NV_AUTOMAP

    // 185
    1,                          // NV_DXCAGE
    2,                          // NV_ONAIR_FG
    2,                          // NV_ONAIR_BG
    1,                          // NV_ONAIR_RAINBOW
    NV_DXCLCMD_LEN,             // NV_DXCMD0

    // 190
    NV_DXCLCMD_LEN,             // NV_DXCMD1
    NV_DXCLCMD_LEN,             // NV_DXCMD2
    NV_DXCLCMD_LEN,             // NV_DXCMD3
    NV_DXCLCMD_LEN,             // NV_DXCMD4
    NV_DXCLCMD_LEN,             // NV_DXCMD5

    // 195
    NV_DXCLCMD_LEN,             // NV_DXCMD6
    NV_DXCLCMD_LEN,             // NV_DXCMD7
    NV_DXCLCMD_LEN,             // NV_DXCMD8
    NV_DXCLCMD_LEN,             // NV_DXCMD9
    NV_DXCLCMD_LEN,             // NV_DXCMD10

    // 200
    NV_DXCLCMD_LEN,             // NV_DXCMD11
    NV_ONAIR_LEN,               // NV_ONAIR_MSG
    1,                          // NV_SETRADIO
    NV_ONTAWLIST_LEN,           // NV_ONTAWLIST
    1,                          // NV_ONTAWLISTMASK

    // 205
    1,                          // NV_ONTASORTBY
    NV_ONTAORG_LEN,             // NV_ONTAORG
    NV_TITLE_LEN,               // NV_TITLE
    2,                          // NV_TITLE_FG
    2,                          // NV_TITLE_BG

    // 210
    1,                          // NV_TITLE_RAINBOW
    1,                          // NV_ONTA_MAXAGE
    1,                          // NV_QRZID
    1,                          // NV_CALLPREF
    4,                          // NV_CSELONOFF

    // 215
    4,                          // NV_CSELTHIN
    4,                          // NV_CSELDASHED_A
    4,                          // NV_CSELONOFF_A
    4,                          // NV_CSELTHIN_A
    4,                          // NV_CSELDASHED_B

    // 220
    4,                          // NV_CSELONOFF_B
    4,                          // NV_CSELTHIN_B
    1,                          // NV_ONTABIO
    1,                          // NV_DXCBIO
    2,                          // NV_X11GEOM_X

    // 225
    2,                          // NV_X11GEOM_Y
    2,                          // NV_X11GEOM_W
    2,                          // NV_X11GEOM_H
    2,                          // NV_DEWXCHOICE
    2,                          // NV_DXWXCHOICE

    // 230
    1,                          // NV_UDPSETSDX
    4,                          // NV_SPCWXCHOICE
    1,                          // NV_DXPEDS
    1,                          // NV_UDPSPOTS
    1,                          // NV_AUTOUPGRADE

    // 235
    1,                          // NV_MAXTLEAGE
    2,                          // NV_MINLBLDIST
    NV_SATNAME_LEN,             // NV_SAT2NAME
    1,                          // NV_PSK_SHOWPATH
    1,                          // NV_SAT1FLAGS

    // 240
    1,                          // NV_SAT2FLAGS

};




/*******************************************************************
 *
 * internal implementation
 * 
 *******************************************************************/

/* table of each NV_Name starting address, ie, its NV_COOKIE.
 * built once at runtime for O(1) access thenceforth.
 */
static uint16_t nv_addrs[NV_N];


/* called to init EEPROM. ignore after first call.
 */
static void initEEPROM()
{
    // ignore if called before
    static bool before;
    if (before)
        return;
    before = true;

    // init EEPROM
    uint16_t ee_used, ee_size;
    reportEESize (ee_used, ee_size);
    if (ee_used > ee_size)
        fatalError ("EEPROM too large: %u > %u", ee_used, ee_size);
    EEPROM.begin(ee_size);      
    Serial.printf ("EEPROM %d size %u + %u = %u, max %u\n", NV_N, NV_BASE, ee_used-NV_BASE, ee_used, ee_size);

    // build nv_addrs[] cache
    uint16_t e_addr = NV_BASE;
    for (int i = 0; i < NV_N; i++) {
        nv_addrs[i] = e_addr;
        e_addr += 1 + nv_sizes[i];              // + 1 for cookie
    }


    if (debugLevel (DEBUG_NVRAM, 2)) {
        printf ("NV: initial settings:\n");
        uint16_t offset = 0;
        for (int i = 0; i < NV_N; i++) {
            const uint8_t sz = nv_sizes[i];
            uint16_t start = NV_BASE+offset;
            printf ("NV: %3d 0%03X %3d %02X ", i, start, sz, EEPROM.read(start));
            start += 1;                     // skip cookie
            switch (sz) {
            case 1: {
                uint8_t i1 = EEPROM.read(start);
                printf ("1: %11d = 0x%02X\n", i1, i1);
                }
                break;
            case 2: {
                uint16_t i2 = EEPROM.read(start) + 256*EEPROM.read(start+1);
                printf ("2: %11d = 0x%04X\n", i2, i2);
                }
                break;
            case 4: {
                uint32_t i4 = EEPROM.read(start) + (1UL<<8)*EEPROM.read(start+1)*(1UL<<16)
                                + (1UL<<16)*EEPROM.read(start+2) + (1UL<<24)*EEPROM.read(start+3);
                float f4;
                memcpy (&f4, &i4, 4);
                printf ("4: %11d = 0x%08X = %g\n", i4, i4, f4);
                }
                break;
            default:
                // string -- stop after first EOS
                printf ("s: ");
                for (int j = 0; j < sz; j++) {
                    uint8_t c = EEPROM.read(start+j);
                    if (c == '\0')
                        break;
                    if (c < ' ' || c >= 0x7f)
                        printf (" %02X", c);
                    else
                        printf ("%c", (char)c);
                }
                printf ("\n");
                break;
            }
            offset += sz + 1;         // size + cookie
        }
    }
}


/* given NV_Name, return address of item's NV_COOKIE and its length
 */
static bool nvramStartAddr (NV_Name e, uint16_t *e_addr, uint8_t *e_len)
{
    if (e >= NV_N)
        return(false);
    *e_addr = nv_addrs[e];
    *e_len = nv_sizes[e];

    return (true);
}

/* write NV_COOKIE then the given array with the expected number of bytes in the given element location.
 * xbytes 0 denotes unknown for strings.
 */
static void nvramWriteBytes (NV_Name e, const uint8_t data[], uint8_t xbytes)
{
    initEEPROM();

    uint8_t e_len = 0;
    uint16_t e_addr = 0;
    if (!nvramStartAddr (e, &e_addr, &e_len))
        fatalError ("NVBUG! Write: bad id %d", e);
    if (xbytes && e_len != xbytes)
        fatalError ("NVBUG! Write: %d %d != %d bytes", e, e_len, xbytes);

    if (debugLevel (DEBUG_NVRAM, 1)) {
        printf ("NV: e= %3d write 1+%d to 0x%03X:", e, xbytes, e_addr);
        printf (" %02X +", NV_COOKIE);
        for (int i = 0; i < xbytes; i++) {
            uint8_t v = data[i];
            printf (" 0x%02X=%c", v, isprint(v) ? v : '?');
        }
        printf ("\n");
    }

    EEPROM.write (e_addr++, NV_COOKIE);

    for (int i = 0; i < e_len; i++)
        EEPROM.write (e_addr++, *data++);
    if (!EEPROM.commit())
        fatalError ("EEPROM.commit failed");
}

/* read NV_COOKIE then the given array for the given element with the given number of expected bytes.
 * xbytes 0 denotes unknown for strings.
 */
static bool nvramReadBytes (NV_Name e, uint8_t *buf, uint8_t xbytes)
{
    initEEPROM();

    uint8_t e_len = 0;
    uint16_t e_addr = 0;
    if (!nvramStartAddr (e, &e_addr, &e_len))
        fatalError ("NVBUG! Read: bad id %d", e);
    if (xbytes && e_len != xbytes)
        fatalError ("NVBUG! Read: %d %d != %d bytes", e, e_len, xbytes);

    if (EEPROM.read(e_addr++) != NV_COOKIE) {
        Serial.printf ("NV: no cookie for e= %d at 0x%03X\n", e, e_addr);
        return (false);
    }

    if (debugLevel (DEBUG_NVRAM, 1)) {
        // N.B. already incremented e_addr
        printf ("NV: e= %3d read 1+%d from 0x%03X:", e, xbytes, e_addr-1);
        printf (" %02X +", EEPROM.read(e_addr-1));
        for (int i = 0; i < xbytes; i++) {
            uint8_t v = EEPROM.read(e_addr+i);
            printf (" 0x%02X=%c", v, isprint(v) ? v : '?');
        }
        printf ("\n");
    }

    for (int i = 0; i < e_len; i++)
        *buf++ = EEPROM.read(e_addr++);
    return (true);
}



/*******************************************************************
 *
 * external interface
 *
 *******************************************************************/

void reportEESize (uint16_t &ee_used, uint16_t &ee_size)
{
    // start with the skipped space at front
    ee_used = NV_BASE;

    // add each NV_ variable
    const unsigned n = NARRAY(nv_sizes);
    for (unsigned i = 0; i < n; i++)
        ee_used += 1 + nv_sizes[i];     // +1 for cookie

    // add the color tables
    ee_used += 2*(1+CSEL_TBLSZ);        // include cookie

    // size is just a constnt
    ee_size = FLASH_SECTOR_SIZE;
}

/* write the given float value to the given NV_name
 */
void NVWriteFloat (NV_Name e, float f)
{
    nvramWriteBytes (e, (uint8_t*)&f, 4);
}

/* write the given uint32_t value to the given NV_name
 */
void NVWriteUInt32 (NV_Name e, uint32_t u)
{
    nvramWriteBytes (e, (uint8_t*)&u, 4);
}

/* write the given int32_t value to the given NV_name
 */
void NVWriteInt32 (NV_Name e, int32_t i)
{
    nvramWriteBytes (e, (uint8_t*)&i, 4);
}

/* write the given uint16_t value to the given NV_name
 */
void NVWriteUInt16 (NV_Name e, uint16_t u)
{
    nvramWriteBytes (e, (uint8_t*)&u, 2);
}

/* write the given int16_t value to the given NV_name
 */
void NVWriteInt16 (NV_Name e, int16_t i)
{
    nvramWriteBytes (e, (uint8_t*)&i, 2);
}

/* write the given uint8_t value to the given NV_name
 */
void NVWriteUInt8 (NV_Name e, uint8_t u)
{
    nvramWriteBytes (e, (uint8_t*)&u, 1);
}

/* write the given int8_t value to the given NV_name
 */
void NVWriteInt8 (NV_Name e, int8_t i)
{
    nvramWriteBytes (e, (uint8_t*)&i, 1);
}

/* write the given string value to the given NV_name
 */
void NVWriteString (NV_Name e, const char *str)
{
    nvramWriteBytes (e, (uint8_t*)str, 0);
}

/* write the given time zone to the given NV_name
 */
void NVWriteTZ (NV_Name e, const TZInfo &tzi)
{
    int32_t secs = tzi.auto_tz ? NVTZ_AUTO : tzi.tz_secs;
    NVWriteInt32 (e, secs);
}

/* read the given NV_Name float value, return whether found in NVRAM.
 */
bool NVReadFloat (NV_Name e, float *fp)
{
    return (nvramReadBytes (e, (uint8_t*)fp, 4));
}

/* read the given NV_Name uint32_t value, return whether found in NVRAM.
 */
bool NVReadUInt32 (NV_Name e, uint32_t *up)
{
    return (nvramReadBytes (e, (uint8_t*)up, 4));
}

/* read the given NV_Name int32_t value, return whether found in NVRAM.
 */
bool NVReadInt32 (NV_Name e, int32_t *ip)
{
    return (nvramReadBytes (e, (uint8_t*)ip, 4));
}

/* read the given NV_Name uint16_t value, return whether found in NVRAM.
 */
bool NVReadUInt16 (NV_Name e, uint16_t *up)
{
    return (nvramReadBytes (e, (uint8_t*)up, 2));
}

/* read the given NV_Name int16_t value, return whether found in NVRAM.
 */
bool NVReadInt16 (NV_Name e, int16_t *ip)
{
    return (nvramReadBytes (e, (uint8_t*)ip, 2));
}

/* read the given NV_Name uint8_t value, return whether found in NVRAM.
 */
bool NVReadUInt8 (NV_Name e, uint8_t *up)
{
    return (nvramReadBytes (e, (uint8_t*)up, 1));
}

/* read the given NV_Name int8_t value, return whether found in NVRAM.
 */
bool NVReadInt8 (NV_Name e, int8_t *ip)
{
    return (nvramReadBytes (e, (uint8_t*)ip, 1));
}

/* read the given NV_Name string value, return whether found in NVRAM.
 */
bool NVReadString (NV_Name e, char *buf)
{
    return (nvramReadBytes (e, (uint8_t*)buf, 0));
}

/* read the given NV_Name TZInfo, return whether found in NVRAM.
 */
bool NVReadTZ (NV_Name e, TZInfo &tzi)
{
    int32_t secs;
    if (!NVReadInt32 (e, &secs))
        return (false);
    if (secs == NVTZ_AUTO) {
        tzi.auto_tz = true;
    } else {
        tzi.auto_tz = false;
        tzi.tz_secs = secs;
    }
    return (true);
}

/* read CSEL color table i, return whether valid.
 */
bool NVReadColorTable (int tbl_AB, uint8_t r[N_CSPR], uint8_t g[N_CSPR], uint8_t b[N_CSPR])
{
    // deterine starting addr
    uint16_t e_addr;
    if (tbl_AB == 1)
        e_addr = CSEL_SETA_ADDR;
    else if (tbl_AB == 2)
        e_addr = CSEL_SETB_ADDR;
    else
        return (false);

    // prep
    initEEPROM();

    // check cookie
    if (EEPROM.read(e_addr++) != NV_COOKIE)
        return (false);

    // read each color
    for (uint8_t i = 0; i < N_CSPR; i++) {
        r[i] = EEPROM.read(e_addr++);
        g[i] = EEPROM.read(e_addr++);
        b[i] = EEPROM.read(e_addr++);
    }
    return (true);
}

/* write CSEL color table i
 */
void NVWriteColorTable (int tbl_AB, const uint8_t r[N_CSPR], const uint8_t g[N_CSPR], const uint8_t b[N_CSPR])
{
    // deterine starting addr
    uint16_t e_addr;
    if (tbl_AB == 1)
        e_addr = CSEL_SETA_ADDR;
    else if (tbl_AB == 2)
        e_addr = CSEL_SETB_ADDR;
    else
        return;

    // prep
    initEEPROM();

    // write cookie
    EEPROM.write (e_addr++, NV_COOKIE);

    // write each color tuple
    for (uint8_t i = 0; i < N_CSPR; i++) {
        EEPROM.write (e_addr++, r[i]);
        EEPROM.write (e_addr++, g[i]);
        EEPROM.write (e_addr++, b[i]);
    }

    if (!EEPROM.commit())
        fatalError ("EEPROM.commit colors failed");
}

/* retrieve NV_X11GEOM_* or build size if not defined or smaller.
 */
void NVReadX11Geom (int &x, int &y, int &w, int &h)
{
    int16_t geom_x, geom_y, geom_w, geom_h;
    if (NVReadInt16 (NV_X11GEOM_X, &geom_x) && NVReadInt16 (NV_X11GEOM_Y, &geom_y)
                        && NVReadInt16 (NV_X11GEOM_W, &geom_w) && NVReadInt16 (NV_X11GEOM_H, &geom_h)
                        && geom_w >= BUILD_W && geom_h >= BUILD_H) {
        x = geom_x;
        y = geom_y;
        w = geom_w;
        h = geom_h;
    } else {
        x = 0;
        y = 0;
        w = BUILD_W;
        h = BUILD_H;
    }
}

/* save NV_X11GEOM_*
 */
void NVWriteX11Geom (int x, int y, int w, int h)
{
    NVWriteInt16 (NV_X11GEOM_X, x);
    NVWriteInt16 (NV_X11GEOM_Y, y);
    NVWriteInt16 (NV_X11GEOM_W, w);
    NVWriteInt16 (NV_X11GEOM_H, h);
}


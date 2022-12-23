#include <pjlib.h>
#include <pjlib-util.h>
#include <pjnath.h>
#include <pjsip.h>
#include <pjsip_ua.h>
#include <pjsip_simple.h>
#include <pjsua-lib/pjsua.h>
#include <pjmedia.h>
#include <pjmedia-codec.h>
#include <pjmedia.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "app_ringtones.h"

#define THIS_FILE "Server"
#define DIAL_TONE_SERVER "111"
#define WAV_SERVER "222"
#define RINGBACK_TONE_SERVER "333"

#define UDP_PORT 5060
#define LOG_LEVEL 3

#define MAX_CALLS 20

#define CALL_DELAY_TIME_SEC 3
#define CALL_DELAY_TIME_MSEC 0
#define CALL_DURATION_TIME_SEC 10
#define CALL_DURATION_TIME_MSEC 0

typedef enum ring_mode
{
    NOT_SET,
    DIAL_TONE,
    WAV_AUDIO,
    RINGBACK_TONE
} ring_mode;

typedef struct call_data
{
    ring_mode ring_mode;
    pj_timer_entry answer_delay_timer;
    pj_timer_entry call_timeout_timer;
    pjsua_call_id call_id;
} call_data;

ringtone ringback_tone;
ringtone dial_tone;

// pjsua_conf_port_id ringback_tone_port_id = -1;
// pjmedia_port *ringback_tone_port;

// pjsua_conf_port_id dial_tone_port_id = -1;
// pjmedia_port *dial_tone_port;

pjsua_player_id player_id;
pj_pool_t *pool;

call_data *calls_data;
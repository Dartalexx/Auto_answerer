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
#include <ctype.h>

#define THIS_FILE	"Server"
#define DIAL_TONE_SERVER "111"
#define WAV_SERVER "222"
#define RINGBACK_TONE_SERVER "333"
#define WAV_RINGTONE "sound/answer.wav"
#define UDP_PORT 5060
#define LOG_LEVEL 4
#define SAMPLES_PER_FRAME 160
#define CLOCK_RATE 8000
#define CHANNEL_COUNT 2
#define BITS_PER_SAMPLE 16
#define ON_DURATION 1000
#define OFF_DURATION 4000
#define TONES_COUNT 1
#define MAX_CALLS 20
#define TONE_FREQUENCY 425
#define DIAL_TONE_ON_DURATION 4000
#define DIAL_TONE_OFF_DURATION 0
#define CALL_DELAY_TIME_SEC 3
#define CALL_DELAY_TIME_MSEC 0

pjsua_conf_port_id ringback_tone_port_id = -1;
pjmedia_port *ringback_tone_port;
pjsua_conf_port_id dial_tone_port_id = -1;
pjmedia_port *dial_tone_port;
pjsua_player_id player_id;
pj_pool_t *pool;

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
    pj_timer_entry duration_timer;
    pjsua_call_id call_id;
} call_data;

call_data *calls_data;
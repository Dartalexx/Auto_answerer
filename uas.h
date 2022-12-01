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
#include <sip_timer.h>
#include <ctype.h>

#define THIS_FILE	"Server"

#define SAMPLES_PER_FRAME 160
#define CLOCK_RATE 8000
#define CHANNEL_COUNT 2
#define BITS_PER_SAMPLE 16
#define ON_DURATION 1000
#define OFF_DURATION 4000
#define TONES_COUNT 1
#define MAX_CONFERENCE_COUNT 20
#define NDEBUG

pjsua_conf_port_id ringback_tone_port_id = -1;
pjmedia_port *ringback_tone_port;
pjsua_conf_port_id dial_tone_port_id = -1;
pjmedia_port *dial_tone_port;
pjsua_player_id player_id;
pj_pool_t *pool;

typedef enum ring_mode
{
    DIAL_TONE = 1, 
    WAV_AUDIO = 2,
    RINGBACK_TONE = 3
} ring_mode;

typedef struct call_data
{
    ring_mode ring_mode;
} call_data;
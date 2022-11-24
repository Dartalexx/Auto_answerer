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
#include <omp.h>

#define THIS_FILE	"Server"

#define SAMPLES_PER_FRAME 160
#define ON_DURATION 1000
#define OFF_DURATION 4000
#define TONES_COUNT 1
#define MAX_CONFERENCE_COUNT 20
#define NDEBUG

pjsua_conf_port_id ringback_port_id = -1;
pjmedia_port *ringback_port;
pjmedia_conf **conf;
pjsua_player_id player_id;
pj_pool_t *pool;

int mode;
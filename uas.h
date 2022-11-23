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

typedef struct call_manager
{
	int calls_number;
    pjsua_conf_port_id ports [20];
} call_manager;

pjsua_conf_port_id ringback_port_id = -1;
pjmedia_port *ringback_port;
pjmedia_conf **conf;
pjsua_player_id player_id;
pj_pool_t *pool;
pjmedia_tone_desc pause_tone[TONES_COUNT];
pjmedia_tone_desc ong_tone[TONES_COUNT];
pjmedia_port *file_port; /* File port */
unsigned int *file_slot;
int mode;
call_manager cm;

typedef struct call_data
{
  pj_pool_t           *pool;
  pjmedia_conf        *conf;
  pjmedia_port        *cport;
  pjmedia_port        *null;
  pjmedia_master_port *m;
  int                  call_slot;
} call_data;

call_data ** cd;
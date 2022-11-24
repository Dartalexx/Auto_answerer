#include "uas.h"

typedef struct call_data
{
  pj_pool_t           *pool;
  pjmedia_conf        *conf;
  pjmedia_port        *dn_slot;
  pjmedia_port        *up_slot;
  pjmedia_master_port *m;
  int                  call_slot;
  int dn_port;
  int up_port;
} call_data;

pjmedia_port *file_port; /* File port */
unsigned int *file_slot;

/* Display error and exit application */
static void error_exit(const char *title, pj_status_t status)
{
    pjsua_perror(THIS_FILE, title, status);
    pjsua_destroy();
    exit(1);
}

/*DO NOT USE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
static void ringback_start(pjsua_call_id call_id)
{
    if (ringback_port_id != PJSUA_INVALID_ID)
    {
        pjsua_conf_connect(ringback_port_id, pjsua_call_get_conf_port(call_id));
        pjsua_conf_adjust_rx_level(pjsua_call_get_conf_port(call_id), 0);
    }
}

static void ringback_stop(pjsua_call_id call_id)
{
    if (ringback_port_id != PJSUA_INVALID_ID)
    {
        pjsua_conf_disconnect(ringback_port_id, pjsua_call_get_conf_port(call_id));
    }
}

pj_status_t file_player()
{
    pj_status_t status;
    //pj_str_t filename;
    //filename.ptr = "sound/answer.wav";
    //filename.slen = 16;
    //status = pjsua_player_create(&filename, 0, &player_id);
    status = pjmedia_wav_player_port_create(pool, "sound/answer.wav", 0, 0, 0, &file_port);
    if (status != PJ_SUCCESS)
        return status;
    return PJ_SUCCESS;
}

pj_status_t generate_tone()
{
    /* Create ringback tones */
    pj_str_t name;
    unsigned i;
    pjmedia_tone_desc pause_tone[TONES_COUNT];
	pjmedia_tone_desc ong_tone[TONES_COUNT];
    pj_status_t status;
    /* Ringback tone (call is ringing) */
    status = pjmedia_tonegen_create(pool, 8000, 1, SAMPLES_PER_FRAME, 16, 0, &ringback_port);
    if (status != PJ_SUCCESS)
        return status;

    pj_bzero(&pause_tone, sizeof(pause_tone));
    pj_bzero(&ong_tone, sizeof(ong_tone));

    /*One second rings, 4 seconds waits*/
    pause_tone[0].freq1 = 425;
    pause_tone[0].on_msec = ON_DURATION;
    pause_tone[0].off_msec = OFF_DURATION;

    /*Ongoing ringtone*/
    ong_tone[0].freq1 = 425;
    ong_tone[0].on_msec = 4000;
    ong_tone[0].off_msec = 0;

    /*playing tones to port*/
    if (mode == 1)
        pjmedia_tonegen_play(ringback_port, TONES_COUNT, ong_tone, PJMEDIA_TONEGEN_LOOP);
    else
        pjmedia_tonegen_play(ringback_port, TONES_COUNT, pause_tone, PJMEDIA_TONEGEN_LOOP);
    /*adding tones port*/
    //status = pjsua_conf_add_port(pool, ringback_port, &ringback_port_id);
    if (status != PJ_SUCCESS)
        error_exit("Error while adding ringtone port to conference", status);

    return PJ_SUCCESS;
}

static void call_media_init(pjsua_call_id call_id)
{
    pj_status_t status;
    pj_pool_t * media_pool;
    call_data *cd;

    media_pool = pjsua_pool_create("mycall", 1000, 1000);
    cd = PJ_POOL_ZALLOC_T(media_pool, struct call_data);

    pjsua_call_set_user_data(call_id, (void *)cd);
    status = pjmedia_conf_create(pool, 3, 8000, 1, 160, 16, PJMEDIA_CONF_NO_DEVICE | PJMEDIA_CONF_NO_MIC, &cd->conf);
    if (status != PJ_SUCCESS)
        error_exit("Error during creating conference\n", status);
    cd->dn_slot = pjmedia_conf_get_master_port(cd->conf);
    status = pjmedia_null_port_create(pool, 8000, 1, 160, 16, &cd->up_slot);
    status = pjmedia_master_port_create(pool, cd->up_slot, cd->dn_slot, 0, &cd->m);
    status = pjmedia_master_port_start(cd->m);

    /*---------------------------Not needed---------------------------------------------------*/
    pjmedia_snd_port ** p_port = pj_pool_alloc(pool, sizeof(pjmedia_snd_port**));
    pjmedia_snd_port_create 	(pool, -1, -1, 8000, 1, 160, 16, 0, p_port); 	
    status = pjmedia_snd_port_connect(*p_port, cd->dn_slot);
}

static void on_stream_created(pjsua_call_id call_id, pjmedia_stream *strm,
                              unsigned stream_idx,
                              pjmedia_port **p_port)
{
    struct call_data *cd;
    cd = (struct call_data*) pjsua_call_get_user_data(call_id);
    pjmedia_conf_add_port(cd->conf, cd->pool, *p_port, NULL, &cd->call_slot);
    if (mode != 2)
        pjmedia_conf_add_port(cd->conf, pool, ringback_port, NULL, &ringback_port_id);
    else
        pjmedia_conf_add_port(cd->conf, pool, file_port, NULL, &ringback_port_id);
}

static void on_stream_destroyed(pjsua_call_id call_id, pjmedia_stream *strm, unsigned stream_idx)
{
    struct call_data *cd;
    cd = (struct call_data*) pjsua_call_get_user_data(call_id);
    pjmedia_conf_remove_port(cd->conf, cd->call_slot);
}

static void call_media_deinit(pjsua_call_id call_id)
{
    struct call_data *cd;
    cd = (struct call_data*) pjsua_call_get_user_data(call_id);

    if (!cd)
        return;
    pjmedia_master_port_stop(cd->m);
    pjmedia_master_port_destroy(cd->m, PJ_FALSE);

    pjmedia_conf_destroy(cd->conf);
    pjmedia_port_destroy(cd->up_slot);

 //-------------------------------------------------

    pjsua_call_set_user_data(call_id, NULL);
}


/* Callback called by the library upon receiving incoming call */
static void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
                             pjsip_rx_data *rdata)
{
    pjsua_call_info ci;
    PJ_UNUSED_ARG(acc_id);
    PJ_UNUSED_ARG(rdata);
    pjsua_call_get_info(call_id, &ci);

    /*Sending ringing code to caller*/
    pjsua_call_answer(call_id, 180, NULL, NULL);
    PJ_LOG(3, (THIS_FILE,
               "Incoming call for account %d!\n"
               "Media count: %d audio & %d video\n"
               "From: %.*s\n"
               "To: %.*s\n",
               acc_id,
               ci.rem_aud_cnt,
               ci.rem_vid_cnt,
               (int)ci.remote_info.slen,
               ci.remote_info.ptr,
               (int)ci.local_info.slen,
               ci.local_info.ptr));

    /*------------------------PUT HERE NORMAL TIMER-----------------------------*/
    // double start, check;
    // start = omp_get_wtime();
    // //Get current time
    // for (check = omp_get_wtime();check - start < 7;check = omp_get_wtime()){}
    pj_thread_sleep(3000);
    /*-------------------------Timer ends---------------------------------------*/
    call_media_init(call_id);
    pjsua_call_answer(call_id, 200, NULL, NULL);
}

/* Callback called by the library when call's state has changed */
static void on_call_state(pjsua_call_id call_id, pjsip_event *e)
{
    pjsua_call_info call_info;
    pjsip_msg *msg;
    int code;
    PJ_UNUSED_ARG(e);

    pjsua_call_get_info(call_id, &call_info);

    if (call_info.state == PJSIP_INV_STATE_DISCONNECTED)
    {
        // call_manager_remove_call(call_id);
    //     if (mode != 2)
    //     pjsua_conf_disconnect(ringback_port_id, pjsua_call_get_conf_port(call_id));
    // else
    //     pjsua_conf_disconnect(pjsua_player_get_conf_port(player_id), pjsua_call_get_conf_port(call_id));

        call_media_deinit(call_id);
        
        PJ_LOG(3, (THIS_FILE, "Call %d is DISCONNECTED [reason=%d (%.*s)]",
                   call_id,
                   call_info.last_status,
                   (int)call_info.last_status_text.slen,
                   call_info.last_status_text.ptr));
    }
    else
    {

        if (call_info.state == PJSIP_INV_STATE_CONFIRMED)
        {
        }
        if (call_info.state == PJSIP_INV_STATE_EARLY)
        {
            int code;
            pj_str_t reason;
            pjsip_msg *msg;

            /* This can only occur because of TX or RX message */
            pj_assert(e->type == PJSIP_EVENT_TSX_STATE);

            if (e->body.tsx_state.type == PJSIP_EVENT_RX_MSG)
            {
                msg = e->body.tsx_state.src.rdata->msg_info.msg;
            }
            else
            {
                msg = e->body.tsx_state.src.tdata->msg;
            }

            code = msg->line.status.code;
            reason = msg->line.status.reason;

            PJ_LOG(3, (THIS_FILE, "Call %d state changed to %.*s (%d %.*s)",
                       call_id, (int)call_info.state_text.slen,
                       call_info.state_text.ptr, code,
                       (int)reason.slen, reason.ptr));
        }
        else
        {
            PJ_LOG(3, (THIS_FILE, "Call %d state changed to %.*s",
                       call_id,
                       (int)call_info.state_text.slen,
                       call_info.state_text.ptr));
        }
    }
}

/* Callback called by the library when call's media state has changed */
static void on_call_media_state(pjsua_call_id call_id)
{
    struct call_data *cd;
    cd = (struct call_data*) pjsua_call_get_user_data(call_id);
    pjsua_call_info ci;
    pj_status_t status;
    pjsua_call_get_info(call_id, &ci);
    if (mode != 2)
        ringback_stop(call_id);
    /* When media is active, connect ringtone to caller.*/
    if (ci.media_status == PJSUA_CALL_MEDIA_ACTIVE)
    {
        // call_manager_add_call(call_id);
        if (mode != 2)
        pjmedia_conf_connect_port(cd->conf, ringback_port_id, cd->call_slot, 0);
    else
        pjmedia_conf_connect_port(cd->conf, player_id, cd->call_slot, 0);
    }
    else
    call_media_deinit(call_id);
}
/*-------------------------------------------------------------------------------------*/


/*-------------------------------------------------------------------------------------*/
/*Main app functions*/
pj_status_t create_udp_transport(pjsua_transport_id *t_id)
{
    pjsua_transport_config cfg;
    pj_status_t status;
    pjsua_transport_config_default(&cfg);
    cfg.port = 5060;
    status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &cfg, t_id);
    if (status != PJ_SUCCESS)
        error_exit("Error creating transport", status);
    return status;
}

pj_status_t app_init(pjsua_acc_id *acc_id)
{
    pjsua_config cfg;
    pjsua_transport_id t_id;
    pjsua_media_config media_config;
    pjsua_logging_config log_cfg;
    pj_status_t status;

    //call_manager_init();

    status = pjsua_create();
    if (status != PJ_SUCCESS)
        error_exit("Error in pjsua_create()", status);

    pool = pjsua_pool_create("ringtone", 8000, 1000);

    pjsua_config_default(&cfg);
    cfg.cb.on_incoming_call = &on_incoming_call;
    cfg.cb.on_call_media_state = &on_call_media_state;
    cfg.cb.on_call_state = &on_call_state;

    cfg.cb.on_stream_created = &on_stream_created;
    cfg.cb.on_stream_destroyed = &on_stream_destroyed;
    pjsua_logging_config_default(&log_cfg);
    log_cfg.console_level = 4;

    pjsua_media_config_default(&media_config);

    status = pjsua_init(&cfg, &log_cfg, &media_config);
    if (status != PJ_SUCCESS)
        error_exit("Error in pjsua_init()", status);

    /*Transport creation*/
    create_udp_transport(&t_id);

    status = pjsua_start();
    if (status != PJ_SUCCESS)
        error_exit("Error starting pjsua", status);

    /* Create local SIP account */
    status = pjsua_acc_add_local(t_id, PJ_TRUE, acc_id);
    if (status != PJ_SUCCESS)
        error_exit("Error adding local account", status);

    /*Disconnecting pjsua from main bridge*/
    pjsua_set_no_snd_dev();

    /*Generate ringbacks*/
    if (mode != 2)
        status = generate_tone();
    else
        status = file_player();
    if (status != PJ_SUCCESS)
        error_exit("Error in file_player()", status);
}

pj_status_t stop_pjsua(pjsua_acc_id *acc_id)
{
    //pjsua_conf_remove_port(ringback_port_id);

    pjsua_acc_del(*acc_id);
    pj_pool_release(pool);
    pjsua_destroy();
}

int main(int argc, char *argv[])
{
    pjsua_acc_id acc_id;

    /*Check the arguments(maybe add check on int here)*/
    if (argc != 2)
    {
        printf("Missing command line argument, will quit now..");
        return 1;
    }
    mode = atoi(argv[1]);
    if (mode == 0)
    {
        printf("Wrong command line argument, will quit now..");
        return 1;
    }

    /*Initialize pjsua app*/
    app_init(&acc_id);

    /* Wait until user press "q" to quit. */
    for (;;)
    {
        char option[10];
        puts("Press 'q' to stop server");
        if (fgets(option, sizeof(option), stdin) == NULL)
        {
            puts("EOF while reading stdin, will quit now..");
            break;
        }

        if (option[0] == 'q')
            break;
    }

    /*Clear the mess*/
    stop_pjsua(&acc_id);

    return 0;
}
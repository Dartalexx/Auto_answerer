#include "uas.h"

void app_destroy();

/* Display error and exit application */
static void error_exit(const char *title, pj_status_t status)
{
	pjsua_perror(THIS_FILE, title, status);
	app_destroy();
	exit(1);
}

/*Parse remote URI to choose ringtone mode*/
ring_mode get_ring_mode(pjsua_call_info call_info)
{
	ring_mode ring_mode = NOT_SET;
	pj_str_t uri;
	uri = call_info.remote_info;
	if (!uri.ptr)
		return ring_mode;
	for (int i = 0; i < uri.slen; i++)
		if (uri.ptr[i] == '@')
		{
			if (isdigit(uri.ptr[i + 1]))
			{
				ring_mode = RINGBACK_TONE;
				break;
			}
			if (isalpha(uri.ptr[i + 1]))
			{
				ring_mode = WAV_AUDIO;
				break;
			}
		}
	if (ring_mode == NOT_SET)
		ring_mode = DIAL_TONE;

	return ring_mode;
}

/*Create pjsua player with attached wav file*/
pj_status_t file_player_create(pj_str_t filename)
{
	pj_status_t status;

	status = pjsua_player_create(&filename, 0, &player_id);
	return status;
}

/*Generate 2 different ringtones and play one of them (depending on
choosen ring_mode) in loop*/
pj_status_t tone_generate()
{
	/* Create ringback tones */
	pj_str_t name;
	pjmedia_tone_desc ringback_tone[TONES_COUNT];
	pjmedia_tone_desc dial_tone[TONES_COUNT];
	pj_status_t status;

	status = pjmedia_tonegen_create(pool,
									CLOCK_RATE, CHANNEL_COUNT, SAMPLES_PER_FRAME, BITS_PER_SAMPLE, 0,
									&ringback_tone_port);
	if (status != PJ_SUCCESS)
		return status;

	status = pjmedia_tonegen_create(pool,
									CLOCK_RATE, CHANNEL_COUNT, SAMPLES_PER_FRAME, BITS_PER_SAMPLE, 0,
									&dial_tone_port);
	if (status != PJ_SUCCESS)
		return status;

	pj_bzero(&ringback_tone, sizeof(ringback_tone));
	pj_bzero(&dial_tone, sizeof(dial_tone));

	ringback_tone[0].freq1 = 425;
	ringback_tone[0].on_msec = ON_DURATION;
	ringback_tone[0].off_msec = OFF_DURATION;

	dial_tone[0].freq1 = 425;
	dial_tone[0].on_msec = 4000;
	dial_tone[0].off_msec = 0;

	status = pjmedia_tonegen_play(dial_tone_port, TONES_COUNT, dial_tone, PJMEDIA_TONEGEN_LOOP);
	if (status != PJ_SUCCESS)
		return status;
	status = pjmedia_tonegen_play(ringback_tone_port, TONES_COUNT, ringback_tone, PJMEDIA_TONEGEN_LOOP);
	if (status != PJ_SUCCESS)
		return status;

	status = pjsua_conf_add_port(pool, dial_tone_port, &dial_tone_port_id);
	if (status != PJ_SUCCESS)
		return status;
	status = pjsua_conf_add_port(pool, ringback_tone_port, &ringback_tone_port_id);
	if (status != PJ_SUCCESS)
		return status;
		
	return PJ_SUCCESS;
}

/* Callback called by the library upon receiving incoming call */
static void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
							 pjsip_rx_data *rdata)
{
	pjsua_call_info call_info;
	PJ_UNUSED_ARG(acc_id);
	PJ_UNUSED_ARG(rdata);
	ring_mode ring_mode;
	pj_status_t status;
	pjsua_call_get_info(call_id, &call_info);
	/* Choose ring mode depending on URI*/
	ring_mode = get_ring_mode(call_info);
	if (ring_mode == NOT_SET)
		error_exit("Can't get ring mode from URI", PJ_EUNKNOWN);

	/*Assign ring mode to certain call*/
	status = pjsua_call_set_user_data(call_id, &ring_mode);
	if (status != PJ_SUCCESS)
		error_exit("Can't set user data to call", PJ_EUNKNOWN);
	PJ_LOG(3, (THIS_FILE,
			   "Ring mode %d was chosen for call %d!\n", ring_mode, call_id));

	/*Answer the call*/
	status = pjsua_call_answer(call_id, 180, NULL, NULL);
	if (status != PJ_SUCCESS)
		error_exit("Can't answer call", PJ_EUNKNOWN);
	PJ_LOG(3, (THIS_FILE,
			   "Incoming call for account %d!\n"
			   "From: %.*s\n"
			   "To: %.*s\n",
			   acc_id, (int)call_info.remote_info.slen,
			   call_info.remote_info.ptr, (int)call_info.local_info.slen, call_info.local_info.ptr));

	pj_thread_sleep(3000);
	/* pjsip_sess_expires_hdr* pjsip_sess_expires_hdr_create 	( 	pj_pool_t *  	pool	) 	
	*/

	status = pjsua_call_answer(call_id, 200, NULL, NULL);
	if (status != PJ_SUCCESS)
		error_exit("Can't answer call", PJ_EUNKNOWN);
}

/* Callback called by the library when call's state has changed */
static void on_call_state(pjsua_call_id call_id, pjsip_event *e)
{
	pjsua_call_info call_info;
	pjsip_msg *msg;
	int code;
	PJ_UNUSED_ARG(e);

	if (call_info.state == PJSIP_INV_STATE_DISCONNECTED)
	{
		PJ_LOG(3, (THIS_FILE, "Call %d is DISCONNECTED [reason=%d (%.*s)]", call_id,
				   call_info.last_status, (int)call_info.last_status_text.slen,
				   call_info.last_status_text.ptr));
	}
	else
	{
		if (call_info.state == PJSIP_INV_STATE_EARLY)
		{
			int code;
			pj_str_t reason;
			pjsip_msg *msg;

			/*Get call status*/
			if (e->body.tsx_state.type == PJSIP_EVENT_RX_MSG)
				msg = e->body.tsx_state.src.rdata->msg_info.msg;
			else
				msg = e->body.tsx_state.src.tdata->msg;

			code = msg->line.status.code;
			reason = msg->line.status.reason;

			PJ_LOG(3, (THIS_FILE, "Call %d state changed to %.*s (%d %.*s)", call_id,
					   (int)call_info.state_text.slen, call_info.state_text.ptr, code,
					   (int)reason.slen, reason.ptr));
		}
		else
			PJ_LOG(3, (THIS_FILE, "Call %d state changed to %.*s", call_id,
					   (int)call_info.state_text.slen, call_info.state_text.ptr));
	}
}

/* Callback called by the library when call's media state has changed */
static void on_call_media_state(pjsua_call_id call_id)
{
	pjsua_call_info call_info;
	pj_status_t status;
	ring_mode ring_mode;

	ring_mode = *(int *)(pjsua_call_get_user_data(call_id));
	if ((ring_mode != DIAL_TONE) && (ring_mode != RINGBACK_TONE) && (ring_mode != WAV_AUDIO))
		error_exit("Can't get ring_mode from URI", PJ_EUNKNOWN);

	status = pjsua_call_get_info(call_id, &call_info);
	if (status != PJ_SUCCESS)
		error_exit("Can't get call info", status);

	/* When media is active, connect ringtone to caller.*/
	if (call_info.media_status == PJSUA_CALL_MEDIA_ACTIVE)
	{
		if (ring_mode == DIAL_TONE)
			status = pjsua_conf_connect(dial_tone_port_id,
										pjsua_call_get_conf_port(call_id));
		if (ring_mode == WAV_AUDIO)
			status = pjsua_conf_connect(pjsua_player_get_conf_port(player_id),
										pjsua_call_get_conf_port(call_id));
		if (ring_mode == RINGBACK_TONE)
			status = pjsua_conf_connect(ringback_tone_port_id,	
										pjsua_call_get_conf_port(call_id));
		
		if (status != PJ_SUCCESS)
			error_exit("Can't connect ringtone port to caller", status);
	}
}

/*Create pjsua UDP transport*/
pj_status_t create_udp_transport(pjsua_transport_id *t_id)
{
	pjsua_transport_config cfg;
	pj_status_t status;

	pjsua_transport_config_default(&cfg);
	cfg.port = 5060;

	status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &cfg, t_id);
	return status;
}

/*Initialize  pjsua app*/
pj_status_t app_init()
{
	pjsua_config cfg;
	pjsua_transport_id t_id;
	pjsua_acc_id acc_id;
	pjsua_media_config media_config;
	pjsua_logging_config log_cfg;
	pj_status_t status;
	pj_str_t filename;
	pjsip_timer_setting time_settings;

	status = pjsua_create();
	if (status != PJ_SUCCESS)
		return status;

	pool = pjsua_pool_create("ringtone", 8000, 1000);
	if (!pool)
		return status;

	pjsua_config_default(&cfg);
	cfg.cb.on_incoming_call = &on_incoming_call;
	cfg.cb.on_call_media_state = &on_call_media_state;
	cfg.cb.on_call_state = &on_call_state;

	pjsua_logging_config_default(&log_cfg);
	log_cfg.console_level = 4;

	pjsua_media_config_default(&media_config);

	status = pjsua_init(&cfg, &log_cfg, &media_config);
	if (status != PJ_SUCCESS)
		return status;

	create_udp_transport(&t_id);

	status = pjsua_start();
	if (status != PJ_SUCCESS)
		return status;

	status = pjsua_acc_add_local(t_id, PJ_TRUE, &acc_id);
	if (status != PJ_SUCCESS)
		return status;

	pjsip_timer_init_module(pjsua_get_pjsip_endpt());
	pjsip_timer_setting_default(&time_settings);

	status = tone_generate();
	if (status != PJ_SUCCESS)
		error_exit("Can't generate tones for ringtone, will quit now...",status);

	filename.ptr = "sound/answer.wav";
	filename.slen = 16;

	status = file_player_create(filename);
	if (status != PJ_SUCCESS)
		error_exit("Can't create file player, will quit now...",status);
	return PJ_SUCCESS;
}

/*Remove ringback ports, release pool and destroy pjsua*/
void app_destroy()
{
	if (ringback_tone_port)
	{
		pjsua_conf_remove_port(ringback_tone_port_id);
		pjmedia_port_destroy(ringback_tone_port);
	}
	if (dial_tone_port)
	{
		pjsua_conf_remove_port(dial_tone_port_id);
		pjmedia_port_destroy(dial_tone_port);
	}

	pj_pool_release(pool);
	pjsua_destroy();
}

int main(int argc, char *argv[])
{
	pj_status_t status;
	
	status = app_init();
	if (status != PJ_SUCCESS)
		error_exit("Can't start app, will quit now...", status);
	
	/* Wait until "q" is pressed, then quit. */
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

	app_destroy();
	return 0;
}
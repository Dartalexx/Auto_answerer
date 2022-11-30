#include "uas.h"

void app_destroy();

/* Display error and exit application */
static void error_exit(const char *title, pj_status_t status)
{
	pjsua_perror(THIS_FILE, title, status);
	app_destroy();
	exit(1);
}

/*Create pjsua player with attached wav file*/
void file_player()
{
	pj_status_t status;
	pj_str_t filename;

	filename.ptr = "sound/answer.wav";
	filename.slen = 16;

	status = pjsua_player_create(&filename, 0, &player_id);
	if (status != PJ_SUCCESS)
		error_exit("Can't create file player", status);
}

/*Generate 2 different ringtones and play one of them (depending on
choosen ring_mode) in loop*/
void generate_tone()
{
	/* Create ringback tones */
	pj_str_t name;
	unsigned i;
	pjmedia_tone_desc pause_tone[TONES_COUNT];
	pjmedia_tone_desc ong_tone[TONES_COUNT];
	pj_status_t status;

	status = pjmedia_tonegen_create(pool,
									8000, 1, SAMPLES_PER_FRAME, 16, 0,
									&pause_ringback_port);
	if (status != PJ_SUCCESS)
		error_exit("Can't create tone generator", status);

	status = pjmedia_tonegen_create(pool,
									8000, 1, SAMPLES_PER_FRAME, 16, 0,
									&ong_ringback_port);
	if (status != PJ_SUCCESS)
		error_exit("Can't create tone generator", status);

	pj_bzero(&pause_tone, sizeof(pause_tone));
	pj_bzero(&ong_tone, sizeof(ong_tone));

	/*Ringtone with 4 second pause*/
	pause_tone[0].freq1 = 425;
	pause_tone[0].on_msec = ON_DURATION;
	pause_tone[0].off_msec = OFF_DURATION;

	/*Ongoing ringtone*/
	ong_tone[0].freq1 = 425;
	ong_tone[0].on_msec = 4000;
	ong_tone[0].off_msec = 0;

	status = pjmedia_tonegen_play(ong_ringback_port, TONES_COUNT, ong_tone, PJMEDIA_TONEGEN_LOOP);
	if (status != PJ_SUCCESS)
		error_exit("Can't play ongoing ringtone", status);
	status = pjmedia_tonegen_play(pause_ringback_port, TONES_COUNT, pause_tone, PJMEDIA_TONEGEN_LOOP);
	if (status != PJ_SUCCESS)
		error_exit("Can't play pause ringtone", status);

	status = pjsua_conf_add_port(pool, ong_ringback_port, &ong_ringback_port_id);
	if (status != PJ_SUCCESS)
		error_exit("Can't add add ringtone port to conference", status);
	status = pjsua_conf_add_port(pool, pause_ringback_port, &pause_ringback_port_id);
	if (status != PJ_SUCCESS)
		error_exit("Can't add ringtone port to conference", status);

	return PJ_SUCCESS;
}

/* Callback called by the library upon receiving incoming call */
static void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
							 pjsip_rx_data *rdata)
{
	pjsua_call_info ci;
	PJ_UNUSED_ARG(acc_id);
	PJ_UNUSED_ARG(rdata);
	int ring_mode;

	pjsua_call_get_info(call_id, &ci);
	ring_mode = 0;

	pj_str_t uri = ci.remote_info;
	if (!uri.ptr)
		error_exit("Can't extract URI", 0);

	/* Choose ring mode depending on URI*/
	for (int i = 0; i < uri.slen; i++)
		if (uri.ptr[i] == '@')
		{
			if (isdigit(uri.ptr[i + 1]))
			{
				ring_mode = 3;
				break;
			}
			if (isalpha(uri.ptr[i + 1]))
			{
				ring_mode = 2;
				break;
			}
		}
	if (ring_mode == 0)
		ring_mode = 1;

	/*Assign ring mode to certain call*/
	pjsua_call_set_user_data(call_id, &ring_mode);
	PJ_LOG(3, (THIS_FILE,
			   "Ring mode %d was chosen for call %d!\n", ring_mode, call_id));

	/*Answer the call*/
	pjsua_call_answer(call_id, 180, NULL, NULL);
	PJ_LOG(3, (THIS_FILE,
			   "Incoming call for account %d!\n"
			   "From: %.*s\n"
			   "To: %.*s\n",
			   acc_id, (int)ci.remote_info.slen,
			   ci.remote_info.ptr, (int)ci.local_info.slen, ci.local_info.ptr));
	pj_thread_sleep(3000);
	pjsua_call_answer(call_id, 200, NULL, NULL);
}

/* Callback called by the library when call's state has changed */
static void on_call_state(pjsua_call_id call_id, pjsip_event *e)
{
	pjsua_call_info call_info;
	pjsip_msg *msg;
	int code;
	pj_status_t status;
	PJ_UNUSED_ARG(e);
	int ring_mode;

	ring_mode = *(int *)(pjsua_call_get_user_data(call_id));
	pjsua_call_get_info(call_id, &call_info);

	if (call_info.state == PJSIP_INV_STATE_DISCONNECTED)
	{
		/*Disconnect ringback or player port from caller*/
		if (ring_mode == 1)
			status = pjsua_conf_disconnect(ong_ringback_port_id, 
										   pjsua_call_get_conf_port(call_id));
		if (ring_mode == 2)
			status = pjsua_conf_disconnect(pjsua_player_get_conf_port(player_id), 
										   pjsua_call_get_conf_port(call_id));
		if (ring_mode == 3)
			status = pjsua_conf_disconnect(pause_ringback_port_id, 
										   pjsua_call_get_conf_port(call_id));
		if (status != PJ_SUCCESS)
			error_exit("Can't disconnect ringback port from caller", status);

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
	pjsua_call_info ci;
	pj_status_t status;
	int ring_mode;

	ring_mode = *(int *)(pjsua_call_get_user_data(call_id));

	status = pjsua_call_get_info(call_id, &ci);
	if (status != PJ_SUCCESS)
		error_exit("Can't get call info", status);

	if ((pause_ringback_port_id != PJSUA_INVALID_ID))
	{
		if (ring_mode == 1)
			status = pjsua_conf_disconnect(ong_ringback_port_id, 
										   pjsua_call_get_conf_port(call_id));
		if (ring_mode == 3)
			status = pjsua_conf_disconnect(pause_ringback_port_id, 
										   pjsua_call_get_conf_port(call_id));

		if (status != PJ_SUCCESS)
			error_exit("Can't disconnect ringback port from caller", status);
	}
	/* When media is active, connect ringtone to caller.*/
	if (ci.media_status == PJSUA_CALL_MEDIA_ACTIVE)
	{
		if (ring_mode == 1)
			status = pjsua_conf_connect(ong_ringback_port_id,
										pjsua_call_get_conf_port(call_id));
		if (ring_mode == 2)
			status = pjsua_conf_connect(pjsua_player_get_conf_port(player_id),
										pjsua_call_get_conf_port(call_id));
		if (ring_mode == 3)
			status = pjsua_conf_connect(pause_ringback_port_id,	
										pjsua_call_get_conf_port(call_id));
		
		if (status != PJ_SUCCESS)
			error_exit("Can't connect ringtone port to caller", status);
	}
}

/*Create pjsua UDP transport*/
void create_udp_transport(pjsua_transport_id *t_id)
{
	pjsua_transport_config cfg;
	pj_status_t status;

	pjsua_transport_config_default(&cfg);
	cfg.port = 5060;

	status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &cfg, t_id);
	if (status != PJ_SUCCESS)
		error_exit("Can't create transport", status);
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

	status = pjsua_create();
	if (status != PJ_SUCCESS)
		error_exit("Can't create pjsua", status);

	pool = pjsua_pool_create("ringtone", 8000, 1000);
	if (!pool)
		error_exit("Can't allocate memory to pool", 0);

	pjsua_config_default(&cfg);
	cfg.cb.on_incoming_call = &on_incoming_call;
	cfg.cb.on_call_media_state = &on_call_media_state;
	cfg.cb.on_call_state = &on_call_state;

	pjsua_logging_config_default(&log_cfg);
	log_cfg.console_level = 4;

	pjsua_media_config_default(&media_config);

	status = pjsua_init(&cfg, &log_cfg, &media_config);
	if (status != PJ_SUCCESS)
		error_exit("Can't initialize pjsua", status);

	create_udp_transport(&t_id);

	status = pjsua_start();
	if (status != PJ_SUCCESS)
		error_exit("Can't start pjsua", status);

	status = pjsua_acc_add_local(t_id, PJ_TRUE, acc_id);
	if (status != PJ_SUCCESS)
		error_exit("Can't add local account", status);

	generate_tone();
	
	file_player();
}

/*Remove ringback ports, release pool and destroy pjsua*/
void app_destroy()
{
	if (pause_ringback_port)
	{
		pjsua_conf_remove_port(pause_ringback_port_id);
		pjmedia_port_destroy(pause_ringback_port_id);
	}
	if (ong_ringback_port)
	{
		pjsua_conf_remove_port(ong_ringback_port_id);
		pjmedia_port_destroy(ong_ringback_port_id);
	}

	pj_pool_release(pool);
	pjsua_destroy();
}

int main(int argc, char *argv[])
{
	app_init();

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
#include "uas.h"

void app_destroy();

/* Display error and exit application */
static void error_exit(const char *title, pj_status_t status)
{
	pjsua_perror(THIS_FILE, title, status);
	app_destroy();
	exit(1);
}

/* Search through call_data for unused call slot and return it */
call_data *call_data_get_by_id (pjsua_call_id id)
{
	if (!app_data.calls_data)
		return NULL;
	for (int i = 0; i < MAX_CALLS; i++)
	{
		if (app_data.calls_data[i].call_id == id)
		{
			return &app_data.calls_data[i];
		}
	}
	return NULL;
}

/* Search through call_datas for free element */
call_data *call_data_get_free()
{
	return call_data_get_by_id(PJSUA_INVALID_ID);
}

/* Fill call data with zeros */
void call_data_release(call_data* cell_to_release)
{
	pj_bzero(cell_to_release, sizeof(call_data));
	cell_to_release->call_id = PJSUA_INVALID_ID;
}

/* Callback called by the library upon expiring answer delay timer */
static void answer_delay_callback(pj_timer_heap_t *timer_heap, pj_timer_entry *entry)
{
	pj_status_t status;
	pjsip_endpoint *endpt = pjsua_get_pjsip_endpt();
	int call_id = entry->id;
	PJ_UNUSED_ARG(timer_heap);
	call_data *current_call_data = call_data_get_by_id(call_id);
	pj_str_t disconnect_reason;
	status = pjsua_call_answer(call_id, PJSIP_SC_OK, NULL, NULL);
	if (status != PJ_SUCCESS)
	{
		PJ_LOG(3, (THIS_FILE,
			   "Can't answer call %d by timer %d, hanging up\n", 
			   call_id,
			   current_call_data->answer_delay_timer.id));
		disconnect_reason = pj_str("Can't answer call by answer delay timer callback");
		pjsua_call_hangup(call_id, PJSIP_SC_INTERNAL_SERVER_ERROR, &disconnect_reason, NULL);
		return;
	}
	PJ_LOG(4, (THIS_FILE,
			   "Timer %d worked successfully\n",
			   current_call_data->answer_delay_timer.id));
	current_call_data->answer_delay_timer.id = PJSUA_INVALID_ID;
}

/* Callback called by the library upon expiring duration timer */
static void call_timeout_callback(pj_timer_heap_t *timer_heap, pj_timer_entry *entry)
{
	pj_status_t status;
	pjsip_endpoint *endpt = pjsua_get_pjsip_endpt();
	int call_id = entry->id;
	call_data *current_call_data = call_data_get_by_id(call_id);
	pj_str_t disconnect_reason;
    PJ_UNUSED_ARG(timer_heap);

	if (call_id == PJSUA_INVALID_ID) 
	{
		PJ_LOG(1,(THIS_FILE, "Invalid call ID in timer callback"));
		return;
    }
	
	disconnect_reason = pj_str("Call duration has been exceeded");

	status = pjsua_call_hangup(call_id, PJSIP_SC_OK, NULL, NULL);
	if (status == PJSUA_INVALID_ID)
		PJ_LOG(3, (THIS_FILE, "Failed to disconnect the call %d",
					call_id));
	PJ_LOG(3,(THIS_FILE, "Duration (%d seconds) has been exceeded "
			 "for call %d, disconnecting the call",
			 CALL_DURATION_TIME_SEC, call_id));
	current_call_data->call_timeout_timer.id = PJSUA_INVALID_ID;
}

/* Parse rx message to choose ringtone mode */
ring_mode ring_mode_get(pjsip_rx_data *rdata)
{
	char *msg;
	char *start;
	int size;
	
	pj_str_t dial_server = pj_str(DIAL_TONE_SERVER);
	pj_str_t ringback_server = pj_str(RINGBACK_TONE_SERVER);
	pj_str_t wav_server = pj_str(WAV_SERVER);

	char *to = pj_pool_alloc(app_data.pool, sizeof(char) * 3);
	msg = rdata->msg_info.msg_buf;
	
	start = strstr(msg, "To:");
	for (int i = 0; i < 256; i++)
	{
		if ((int)*(start + i) == 32)
		{
			size = i;
			strncpy(to, start+size+1, size);
			break;
		}
	}

	if (pj_strcmp2(&dial_server, to) == 0)
		return DIAL_MODE;
	if (pj_strcmp2(&wav_server, to) == 0)
		return WAV_MODE;
	if (pj_strcmp2(&ringback_server, to) == 0)
		return RINGBACK_MODE;

	return NOT_SET;
}

/* Create pjsua player with attached wav file */
pj_status_t file_player_create(pj_str_t filename)
{
	pj_status_t status;
	status = pjsua_player_create(&filename, 0, &app_data.player_id);
	return status;
}

/* Generate ringtone and play it in loop */
pj_status_t tone_generate(ringtone* ringtone)
{
	/* Create ringback tones */
	pjmedia_tone_desc tone[ringtone->tones_count];
	pj_status_t status;

	status = pjmedia_tonegen_create(app_data.pool,
									ringtone->clock_rate, ringtone->channel_count,
									ringtone->samples_per_frame, ringtone->bits_per_sample, 0,
									&ringtone->port);
	if (status != PJ_SUCCESS)
		return status;

	pj_bzero(&tone, sizeof(tone));
	for (int i = 0; i < ringtone->tones_count; i++)
	{
		tone[0].freq1 = ringtone->tone_freq;
		tone[0].on_msec = ringtone->on_duration;
		tone[0].off_msec = ringtone->off_duration;
	}
	status = pjmedia_tonegen_play(ringtone->port, ringtone->tones_count, tone, PJMEDIA_TONEGEN_LOOP);
	if (status != PJ_SUCCESS)
		return status;

	status = pjsua_conf_add_port(app_data.pool, ringtone->port, &ringtone->port_id);
	if (status != PJ_SUCCESS)
		return status;
		
	return PJ_SUCCESS;
}

/* Generate 2 ringtones and play them in loop */
pj_status_t ringtones_create()
{
	pj_status_t status;

	/* Set ringback tone params */
	{
		app_data.ringback_tone.bits_per_sample = BITS_PER_SAMPLE;
		app_data.ringback_tone.port_id = -1;
		app_data.ringback_tone.clock_rate = CLOCK_RATE;
		app_data.ringback_tone.on_duration = RINGBACK_TONE_ON_DURATION;
		app_data.ringback_tone.off_duration = RINGBACK_TONE_OFF_DURATION;
		app_data.ringback_tone.samples_per_frame = SAMPLES_PER_FRAME;
		app_data.ringback_tone.tone_freq = TONE_FREQUENCY;
		app_data.ringback_tone.tones_count =  TONES_COUNT;
		app_data.ringback_tone.channel_count = CHANNEL_COUNT;
	}

	/* Set dial tone params */
	{
		app_data.dial_tone.bits_per_sample = BITS_PER_SAMPLE;
		app_data.dial_tone.port_id = -1;
		app_data.dial_tone.clock_rate = CLOCK_RATE;
		app_data.dial_tone.on_duration = DIAL_TONE_ON_DURATION;
		app_data.dial_tone.off_duration = DIAL_TONE_OFF_DURATION;
		app_data.dial_tone.samples_per_frame = SAMPLES_PER_FRAME;
		app_data.dial_tone.tone_freq = TONE_FREQUENCY;
		app_data.dial_tone.tones_count =  TONES_COUNT;
		app_data.dial_tone.channel_count = CHANNEL_COUNT;
	}

	status = tone_generate(&app_data.ringback_tone);
	if (status != PJ_SUCCESS)
		return status;

	status = tone_generate(&app_data.dial_tone);
	if (status != PJ_SUCCESS)
		return status;

	status = file_player_create(pj_str(WAV_RINGTONE));
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
	pj_status_t status;
	pj_str_t disconnect_reason;
	pjsua_call_answer(call_id, PJSIP_SC_TRYING, NULL, NULL);

	/* Searching in calls_data for empty slots */
	call_data *current_call_data =  call_data_get_free();
	if ((!current_call_data))
	{
		PJ_LOG(3, (THIS_FILE, "Can't accept call: all call slots are busy\n"));
		pjsua_call_answer(call_id, PJSIP_SC_BUSY_EVERYWHERE, NULL, NULL);
	}
	else
	{
		pjsua_call_get_info(call_id, &call_info);

		/* Choose ring mode depending on TO field */
		current_call_data->ring_mode = ring_mode_get(rdata);
		current_call_data->call_id = call_id;
		if (current_call_data->ring_mode == NOT_SET)
		{	
			PJ_LOG(3, (THIS_FILE,"Can't get ring mode from URI, hanging up call %d", 
								call_id));
			pjsua_call_answer(call_id, PJSIP_SC_NOT_FOUND, NULL, NULL);
			return;
		}

		pjsua_call_answer(call_id, PJSIP_SC_RINGING, NULL, NULL);

		PJ_LOG(3, (THIS_FILE,
				   "Incoming call for account %d!\n"
				   "From: %.*s\n"
				   "To: %.*s\n",
				   acc_id, (int)call_info.remote_info.slen,
				   call_info.remote_info.ptr, (int)call_info.local_info.slen, call_info.local_info.ptr));

		/* Set pjsip timer */
		pjsip_endpoint *endpt = pjsua_get_pjsip_endpt();
		pj_time_val delay;

		pj_timer_entry_init(&current_call_data->answer_delay_timer, 
							current_call_data->call_id, 
							current_call_data, &answer_delay_callback);
		pj_timer_entry_init(&current_call_data->call_timeout_timer,
							current_call_data->call_id,
							current_call_data, &call_timeout_callback);
		delay.sec = CALL_DELAY_TIME_SEC;
		delay.msec = CALL_DELAY_TIME_MSEC;
		current_call_data->answer_delay_timer.id = call_id;

		status = pjsip_endpt_schedule_timer(endpt, &current_call_data->answer_delay_timer, &delay);
		if (status != PJ_SUCCESS)
		{	
			PJ_LOG(3, (THIS_FILE,"Can't schedule the answer delay timer for call %d, hanging up...",
						call_id));
			disconnect_reason = pj_str("Can't schedule the answer delay timer");
			pjsua_call_hangup(call_id, PJSIP_SC_INTERNAL_SERVER_ERROR, &disconnect_reason, NULL);
			return;
		}

		status = pjsua_call_set_user_data(call_id, current_call_data);
		if (status != PJ_SUCCESS)
		{
			PJ_LOG(3, (THIS_FILE,"Can't set call data for call %d, hanging up...",
						call_id));
			disconnect_reason = pj_str("Can't set call data");
			pjsua_call_hangup(call_id, PJSIP_SC_INTERNAL_SERVER_ERROR, &disconnect_reason, NULL);
			return;
		}
		PJ_LOG(3, (THIS_FILE,
				   "Ring mode %d was chosen for call %d!\n", current_call_data->ring_mode, call_id));
	}
	if (call_id >= MAX_CALLS)
	{
		PJ_LOG(3, (THIS_FILE, "Can't accept call: all %d call slots are busy\n", pjsua_call_get_count()));
		disconnect_reason = pj_str("Busy Here");
		pjsua_call_hangup(call_id, PJSIP_SC_BUSY_HERE, &disconnect_reason, NULL);
	}
}

/* Callback called by the library when call's state has changed */
static void on_call_state(pjsua_call_id call_id, pjsip_event *e)
{
	pjsua_call_info call_info;
	pjsip_msg *msg;
	int code;
	pj_status_t status;
	call_data *current_call_data;
	pj_str_t disconnect_reason;

	status = pjsua_call_get_info(call_id, &call_info);
	if (status != PJ_SUCCESS)
	{
		PJ_LOG(3, (THIS_FILE,"Can't get call info for call %d, hanging up...",
					call_id));
		disconnect_reason = pj_str("Can't get call info");
		pjsua_call_hangup(call_id, PJSIP_SC_INTERNAL_SERVER_ERROR, &disconnect_reason, NULL);
		return;
	}

	if (call_info.state == PJSIP_INV_STATE_DISCONNECTED)
	{
		current_call_data = call_data_get_by_id(call_id);
		if (current_call_data == NULL)
			PJ_LOG(3, (THIS_FILE, "Call %d is DISCONNECTED [reason=%d (%.*s)]", call_id,
					   call_info.last_status, (int)call_info.last_status_text.slen,
					   call_info.last_status_text.ptr));
		else
		{ 
			PJ_LOG(3, (THIS_FILE, "Call %d is DISCONNECTED [reason=%d (%.*s)]", call_id,
					   call_info.last_status, (int)call_info.last_status_text.slen,
					   call_info.last_status_text.ptr));

			/* Release timers after ending call */
			if (current_call_data->answer_delay_timer.id != PJSUA_INVALID_ID)
			{
				pjsip_endpoint *endpt = pjsua_get_pjsip_endpt();
				pjsip_endpt_cancel_timer(endpt, &current_call_data->answer_delay_timer);
				current_call_data->answer_delay_timer.id = PJSUA_INVALID_ID;
			}
			if (current_call_data->call_timeout_timer.id != PJSUA_INVALID_ID)
			{
				pjsip_endpoint *endpt = pjsua_get_pjsip_endpt();
				pjsip_endpt_cancel_timer(endpt, &current_call_data->call_timeout_timer);
				current_call_data->call_timeout_timer.id = PJSUA_INVALID_ID;
			}
			
			/* Release one call cell */
			PJ_LOG(4, (THIS_FILE, "Releasing call data cell for call %d\n", 
						call_id));
			call_data_release(current_call_data);
		}
	}
	else if (call_info.state == PJSIP_INV_STATE_EARLY)
	{
		int code;
		pj_str_t reason;
		pjsip_msg *msg;

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

/* Callback called by the library when call's media state has changed */
static void on_call_media_state(pjsua_call_id call_id)
{
	pjsua_call_info call_info;
	pj_status_t status;
	pj_str_t disconnect_reason;
	call_data *current_call_data;
	current_call_data = call_data_get_by_id(call_id);

	status = pjsua_call_get_info(call_id, &call_info);
	if (status != PJ_SUCCESS)
	{
		PJ_LOG(3, (THIS_FILE,"Can't get call_info for call %d, hanging up...",
					call_id));
		disconnect_reason = pj_str("Can't get call info");
		pjsua_call_hangup(call_id, PJSIP_SC_INTERNAL_SERVER_ERROR, &disconnect_reason, NULL);
		return;
	}

	/* When media is active, connect ringtone to caller and set call duration timer*/
	if (call_info.media_status == PJSUA_CALL_MEDIA_ACTIVE)
	{
		/* Set call duration timer */
		pjsip_endpoint *endpt = pjsua_get_pjsip_endpt();
		pj_time_val delay;

		delay.sec = CALL_DURATION_TIME_SEC;
		delay.msec = CALL_DURATION_TIME_MSEC;
		current_call_data->call_timeout_timer.id = call_id;

		status = pjsip_endpt_schedule_timer(endpt, &current_call_data->call_timeout_timer, &delay);
		if (status != PJ_SUCCESS)
		{	
			PJ_LOG(3, (THIS_FILE,"Can't schedule the call timeout timer for call %d, hanging up...",
						call_id));
			disconnect_reason = pj_str("Can't schedule the call timeout timer");
			pjsua_call_hangup(call_id, PJSIP_SC_INTERNAL_SERVER_ERROR, &disconnect_reason, NULL);
			return;
		}
		switch(current_call_data->ring_mode)
		{
			case DIAL_MODE:
			status = pjsua_conf_connect(app_data.dial_tone.port_id,
										pjsua_call_get_conf_port(call_id));
			break;

			case WAV_MODE:
			status = pjsua_conf_connect(pjsua_player_get_conf_port(app_data.player_id),
										pjsua_call_get_conf_port(call_id));
			break;

			case RINGBACK_MODE:
			status = pjsua_conf_connect(app_data.ringback_tone.port_id,	
										pjsua_call_get_conf_port(call_id));
			break;

			default:
			PJ_LOG(3, (THIS_FILE, "Can't get ring mode from URI, hanging up call %d",
					   call_id));
			disconnect_reason = pj_str("Can't get ring mode from URI");
			pjsua_call_hangup(call_id, PJSIP_SC_INTERNAL_SERVER_ERROR, &disconnect_reason, NULL);
			return;
			}

		if (status != PJ_SUCCESS)
		{
			PJ_LOG(3, (THIS_FILE, "Can't connect ringtone port to caller (call %d)",
					  call_id));
			disconnect_reason = pj_str("Can't connect ringtone port");
			pjsua_call_hangup(call_id, PJSIP_SC_INTERNAL_SERVER_ERROR, &disconnect_reason, NULL);
			return;
		}
	}
}

/* Create pjsua UDP transport */
pj_status_t udp_transport_create(pjsua_transport_id *t_id)
{
	pjsua_transport_config cfg;
	pj_status_t status;

	pjsua_transport_config_default(&cfg);
	cfg.port = UDP_PORT;

	status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &cfg, t_id);
	return status;
}

/* Initialize  pjsua app */
pj_status_t app_init()
{
	pjsua_config cfg;
	pjsua_media_config media_config;
	pjsua_logging_config log_config;
	pjsua_acc_id local_acc_id;
	pjsua_acc_id acc_id;
	pjsua_transport_id t_id;
	pj_status_t status;

	status = pjsua_create();
	if (status != PJ_SUCCESS)
		return status;

	app_data.pool = pjsua_pool_create("main_pool", 8000, 1500);
	if (!app_data.pool)
		return status;

	pjsua_config_default(&cfg);
	cfg.cb.on_incoming_call = &on_incoming_call;
	cfg.cb.on_call_media_state = &on_call_media_state;
	cfg.cb.on_call_state = &on_call_state;
	
	pjsua_logging_config_default(&log_config);
	log_config.console_level = LOG_LEVEL;

	pjsua_media_config_default(&media_config);

	status = pjsua_init(&cfg, &log_config, &media_config);
	if (status != PJ_SUCCESS)
		return status;

	udp_transport_create(&t_id);
	
	status = pjsua_start();
	if (status != PJ_SUCCESS)
		return status;

	status = pjsua_acc_add_local(t_id, PJ_TRUE, &local_acc_id);
	if (status != PJ_SUCCESS)
		return status;

	status = ringtones_create();
	if (status != PJ_SUCCESS)
		error_exit("Can't generate ringtones, will quit now...",status);

	app_data.calls_data = pj_pool_alloc(app_data.pool, sizeof(call_data) * MAX_CALLS);
	if (!app_data.calls_data)
		error_exit("Can't allocate memory for calls data array, will quit now", PJ_EUNKNOWN);
	for (int i = 0; i < MAX_CALLS; i++)
		app_data.calls_data[i].call_id = PJSUA_INVALID_ID;

	return PJ_SUCCESS;
}

/* Remove ringtone ports, release pool and destroy pjsua */
void app_destroy()
{
	pjsua_call_hangup_all();
	if (app_data.ringback_tone.port)
	{
		pjsua_conf_remove_port(app_data.ringback_tone.port_id);
		pjmedia_port_destroy(app_data.ringback_tone.port);
	}
	if (app_data.dial_tone.port)
	{
		pjsua_conf_remove_port(app_data.dial_tone.port_id);
		pjmedia_port_destroy(app_data.dial_tone.port);
	}
	pjsua_player_destroy(app_data.player_id);
	pj_pool_release(app_data.pool);
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
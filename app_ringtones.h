#define SAMPLES_PER_FRAME 160
#define CLOCK_RATE 8000
#define CHANNEL_COUNT 2
#define BITS_PER_SAMPLE 16
#define RINGBACK_TONE_ON_DURATION 1000
#define RINGBACK_TONE_OFF_DURATION 4000
#define TONES_COUNT 1
#define TONE_FREQUENCY 425
#define DIAL_TONE_ON_DURATION 4000
#define DIAL_TONE_OFF_DURATION 0
#define WAV_RINGTONE "sound/answer.wav"

typedef struct ringtone
{
    pjsua_conf_port_id port_id;
    pjmedia_port* port;
    int on_duration;
    int off_duration;
    int tone_freq;
    int tones_count;
    int bits_per_sample;
    int channel_count;
    int samples_per_frame;
    int clock_rate;
} ringtone;
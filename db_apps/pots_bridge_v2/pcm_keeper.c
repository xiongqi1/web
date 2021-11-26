#include <alsa/asoundlib.h>
#include <alsa/use-case.h>

#include <syslog.h>
#include <alloca.h>
#include <stdio.h>
#include <signal.h>

#define TRUE (1==1)
#define FALSE (!TRUE)

/* 8 KHz */
#define DEFAULT_SAMPLING_FREQUENCY 8000
#define TO_FRAME_SIZE(freq,ms) (((ms)*(freq)+1000-1)/1000)

#if __BYTE_ORDER == __LITTLE_ENDIAN
static snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
#else
static snd_pcm_format_t format = SND_PCM_FORMAT_S16_BE;
#endif

#define DEFAULT_PERIOD_FRAME_SIZE 20
#define DEFAULT_PLAYBACK_BUFFER_SIZE 20

static int sampling_frequency;
static int period_frame_size;
static int playback_buffer_size;

static char* main_ocard_dev_name = NULL;

static snd_pcm_t* main_ocard = NULL;

static int main_writedev = 0;

/**
 * @brief initiates ALSA card.
 *
 * @param dev_name is ALSA device name.
 * @param stream is ALSA stream.
 * @param rwdev is ALSA stream handle.
 *
 * @return pointer to a snd_pcm_t instance when it succeeds. Otherwise, NULL.
 */
static snd_pcm_t *alsa_card_init(char *dev_name, snd_pcm_stream_t stream, int* rwdev)
{
    int err;
    int direction;
    snd_pcm_t *handle = NULL;
    snd_pcm_hw_params_t *hwparams = NULL;
    snd_pcm_sw_params_t *swparams = NULL;
    struct pollfd pfd;
    snd_pcm_uframes_t period_size;
    snd_pcm_uframes_t buffer_size = 0;
    unsigned int rate;
    snd_pcm_uframes_t start_threshold, stop_threshold;

    if (!dev_name) {
        syslog(LOG_ERR, "dev name not specified");
        return NULL;
    }

    err = snd_pcm_open(&handle, dev_name, stream, SND_PCM_NONBLOCK);
    if (err < 0) {
        syslog(LOG_ERR, "snd_pcm_open failed: '%s' - %s", dev_name, snd_strerror(err));
        return NULL;
    } else {
        syslog(LOG_DEBUG, "Opening device %s in %s mode", dev_name, (stream == SND_PCM_STREAM_CAPTURE) ? "read" : "write");
    }

    snd_pcm_nonblock(handle, TRUE);

    hwparams = malloc(snd_pcm_hw_params_sizeof());
    if (!hwparams) {
        syslog(LOG_ERR, "failed to allocate hw params");
        goto err;
    }
    memset(hwparams, 0, snd_pcm_hw_params_sizeof());
    snd_pcm_hw_params_any(handle, hwparams);

    err = snd_pcm_hw_params_set_access(handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0)
        syslog(LOG_ERR, "set_access failed: %s", snd_strerror(err));

    err = snd_pcm_hw_params_set_format(handle, hwparams, format);

    if (err < 0)
        syslog(LOG_ERR, "set_format failed: %s", snd_strerror(err));

    err = snd_pcm_hw_params_set_channels(handle, hwparams, 1);
    if (err < 0)
        syslog(LOG_ERR, "set_channels failed: %s", snd_strerror(err));

    rate = sampling_frequency;
    period_size = period_frame_size;
    buffer_size = playback_buffer_size;

    syslog(LOG_DEBUG, "period size to config : %ld", period_size);
    syslog(LOG_DEBUG, "buffer size to config : %ld", buffer_size);

    direction = 0;
    err = snd_pcm_hw_params_set_rate_near(handle, hwparams, &rate, &direction);
    if (rate != sampling_frequency)
        syslog(LOG_WARNING, "Rate not correct, requested %d, got %u", sampling_frequency, rate);

    direction = 0;
    err = snd_pcm_hw_params_set_period_size_near(handle, hwparams, &period_size, &direction);
    if (err < 0)
        syslog(LOG_ERR, "period_size(%lu frames) is bad: %s", period_size, snd_strerror(err));

    err = snd_pcm_hw_params_set_buffer_size_near(handle, hwparams, &buffer_size);
    if (err < 0)
        syslog(LOG_WARNING, "Problem setting buffer size of %lu: %s", buffer_size, snd_strerror(err));

    err = snd_pcm_hw_params(handle, hwparams);
    if (err < 0)
        syslog(LOG_ERR, "Couldn't set the new hw params: %s", snd_strerror(err));

    swparams = malloc(snd_pcm_sw_params_sizeof());
    if (!swparams) {
        syslog(LOG_ERR, "failed to allocate pcm sw params");
        goto err;
    }
    memset(swparams, 0, snd_pcm_sw_params_sizeof());
    snd_pcm_sw_params_current(handle, swparams);

    if (stream == SND_PCM_STREAM_PLAYBACK)
        start_threshold = period_size;
    else
        start_threshold = period_size;

    err = snd_pcm_sw_params_set_start_threshold(handle, swparams, start_threshold);
    if (err < 0)
        syslog(LOG_ERR, "start threshold: %s", snd_strerror(err));

    if (stream == SND_PCM_STREAM_PLAYBACK)
        stop_threshold = buffer_size;
    else
        stop_threshold = buffer_size;

    err = snd_pcm_sw_params_set_stop_threshold(handle, swparams, stop_threshold);
    if (err < 0)
        syslog(LOG_ERR, "stop threshold: %s", snd_strerror(err));

    err = snd_pcm_sw_params(handle, swparams);
    if (err < 0)
        syslog(LOG_ERR, "sw_params: %s", snd_strerror(err));

    /* hardware and software configuration setup */
    {
        syslog(LOG_INFO, "* hw config summary (%s - %s)", (stream == SND_PCM_STREAM_CAPTURE) ? "record" : "playback", dev_name);

        unsigned int config_ch;
        err = snd_pcm_hw_params_get_channels(hwparams, &config_ch);
        if (err == 0)
            syslog(LOG_INFO, "channels : %d", err == 0 ? config_ch : -1);
        else
            syslog(LOG_INFO, "channels : unknown");

        unsigned int config_rate;
        int config_dir;

        err = snd_pcm_hw_params_get_rate(hwparams, &config_rate, &config_dir);
        if (err == 0)
            syslog(LOG_INFO, "rate : %d (dir=%d)", config_rate, config_dir);
        else
            syslog(LOG_INFO, "rate : unknown");

        snd_pcm_uframes_t config_period_size;

        err = snd_pcm_hw_params_get_period_size(hwparams, &config_period_size, &config_dir);
        if (err == 0)
            syslog(LOG_INFO, "period size : %d (dir=%d)", (int)config_period_size, config_dir);
        else
            syslog(LOG_INFO, "period size : unknown");

        snd_pcm_uframes_t config_buf_size;
        err = snd_pcm_hw_params_get_buffer_size(hwparams, &config_buf_size);
        if (err == 0)
            syslog(LOG_INFO, "buffer size : %d", (int)config_buf_size);
        else
            syslog(LOG_INFO, "buffer size : unknown");

        snd_pcm_uframes_t config_start_threshold;
        err = snd_pcm_sw_params_get_start_threshold(swparams, &config_start_threshold);
        if (err == 0)
            syslog(LOG_INFO, "start threshold : %d", (int)config_start_threshold);
        else
            syslog(LOG_INFO, "start threshold : unknown");

        snd_pcm_uframes_t config_stop_threshold;
        err = snd_pcm_sw_params_get_stop_threshold(swparams, &config_stop_threshold);
        if (err == 0)
            syslog(LOG_INFO, "stop threshold : %d", (int)config_stop_threshold);
        else
            syslog(LOG_INFO, "stop threshold : unknown");

    }

    err = snd_pcm_poll_descriptors_count(handle);
    if (err <= 0)
        syslog(LOG_ERR, "Unable to get a poll descriptors count, error is %s", snd_strerror(err));
    if (err != 1) {
        syslog(LOG_DEBUG, "Can't handle more than one device");
    }

    snd_pcm_poll_descriptors(handle, &pfd, err);
    syslog(LOG_DEBUG, "Acquired fd %d from the poll descriptor", pfd.fd);

    *rwdev = pfd.fd;

    return handle;

err:
    return NULL;
}

/**
 * @brief opens PCM.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
int open_main_pcm_devices(void)
{
    /* init main dev */
    sampling_frequency = DEFAULT_SAMPLING_FREQUENCY;
    period_frame_size = TO_FRAME_SIZE(sampling_frequency, DEFAULT_PERIOD_FRAME_SIZE);
    playback_buffer_size = TO_FRAME_SIZE(sampling_frequency, DEFAULT_PLAYBACK_BUFFER_SIZE);

    syslog(LOG_INFO, "period_frame_size=%d", period_frame_size);
    syslog(LOG_INFO, "playback_buffer_size=%d", playback_buffer_size);

    if (main_ocard_dev_name) {
        syslog(LOG_INFO, "initiate main pcm ocards (ocard=%s)", main_ocard_dev_name);

        main_ocard = alsa_card_init(main_ocard_dev_name, SND_PCM_STREAM_PLAYBACK, &main_writedev);
        if (!main_ocard) {
            syslog(LOG_ERR, "failed to initate main ocard (ocard=%s)", main_ocard_dev_name);
            goto err;
        }

        fcntl(main_writedev, F_SETFD, FD_CLOEXEC);
    }

    return 0;

err:
    return -1;
}

/**
 * @brief closes main PCM.
 */
void close_main_pcm_devices(void)
{
    if (main_writedev > 0)
        close(main_writedev);

    if (main_ocard)
        snd_pcm_close(main_ocard);

    main_ocard = NULL;

    main_writedev = 0;
}

/**
 * @brief handles signals.
 *
 * @param sig is a signal number.
 */
static void sighandler(int sig)
{
    syslog(LOG_DEBUG, "caught signal %d", sig);
}

/**
 * @brief prints usage.
 */
void print_usage(void)
{
    printf(
        "pcm_keeper v1.0\n"
        "\n"
        "usage>\n"
        "\tpcm_keeper [options] -d <pcm_device>\n"
        "\n"
        "options>\n"
        "\t-b : run background\n"
        "\n"
        "examples>\n"
        "\tpcm_keeper -b -d hw:0,17\n"
        "\n"
    );
}

/**
 * @brief main.
 *
 * @param argc is argument count.
 * @param argv[] is an array of argument values.
 *
 * @return
 */
int main(int argc, char* argv[])
{
    fd_set fd;
    pid_t child = 0;

    int bg = FALSE;
    int errflag = 0;

    int c;

    while ((c = getopt(argc, argv, "hbd:")) != EOF) {
        switch (c) {

            case 'b': {
                bg = TRUE;
                break;
            }

            case 'd': {
                main_ocard_dev_name = optarg;
                break;
            }

            case 'h': {
                print_usage();
                break;
            }

            case ':':
                fprintf(stderr, "option -%c requires an operand\n", optopt);
                errflag++;
                break;

            case '?': {
                fprintf(stderr, "unrecognized option: '-%c'\n", optopt);
                errflag++;
                break;
            }
        }
    }

    if (errflag || !main_ocard_dev_name) {
        print_usage();
        exit(-1);
    }

    syslog(LOG_INFO, "start PCM clock");
    if (open_main_pcm_devices() < 0) {
        syslog(LOG_ERR, "failed to open PCM");
        goto err;
    }

    #ifdef CONFIG_PCM_CONTINUOUS
    syslog(LOG_INFO, "Continuous PCM clock is used");
    (void)bg;
    (void)fd;
    (void)sighandler;
    (void)child;
    #else
    if (bg) {
        child = fork();

        if (!child) {

            /* be session leader */
            if (setsid() < 0)
                exit(EXIT_FAILURE);

            /* clse stdio, stdout and stderr */
            close(0);
            close(1);
            close(2);

            /* initiate signals */
            signal(SIGHUP, sighandler);
            signal(SIGINT, sighandler);
            signal(SIGQUIT, sighandler);
            signal(SIGTERM, sighandler);

            syslog(LOG_INFO, "PCM clock keeper start");
        }
    }

    /* sleep */
    if ((bg && !child) || !bg) {
        FD_ZERO(&fd);
        select(0, &fd, NULL, NULL, NULL);

        syslog(LOG_INFO, "terminating PCM clock keeper");
    }
    #endif

    close_main_pcm_devices();

    return 0;

err:
    return -1;
}

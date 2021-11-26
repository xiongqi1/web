/*
 * slic_test - ATS voice test tool
 *
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Limited.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM LIMITED ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS LIMITED BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#include "netcomm_proslic.h"

#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include FXS_CONSTANTS_HDR

struct command_t {
    const char* name;
    int (*func)(int argc, char** argv);
};


static int cmd_tones(int argc, char** argv);
static int cmd_tone(int argc, char** argv);
static int cmd_ring(int argc, char** argv);
static int cmd_stop_tone(int argc, char** argv);

struct command_t _cmds[] = {
    {"tone", cmd_tone},
    {"tones", cmd_tones},
    {"ring", cmd_ring},
    {"stop_tone", cmd_stop_tone},
    {NULL, NULL},
};


static int _opt_channel = 0;

static SiVoiceChanType_ptr _cptr = NULL;

#define SAMPLING_RATE 8000.0
#define PI_CONSTANT 3.14159265358979323846
#define SAMPLES_PER_MILISEC (SAMPLING_RATE / 1000)


/**
 * @brief prints usage.
 */
void print_usage()
{
    printf(
        "slic_test [options] <command> [parameters] ...\n"
        "\n"
        "options>"
        "\n"
        "-c <channel number>"
        "\n"
        "\n"
        "commands>\n"
        "\n"
        "	ring <start|stop>\n"
        "	tone <frequency 100 Hz - 3600 Hz> <volume 0-100>\n"
        "	tones <dial,stutter,reorder,roh,confirmation,callwaiting>\n"
        "	stop_tone\n"
        "\n"
        "examples>\n"
        "\n"
        "	slic_test tone 440 50\n"
        "	slic_test tone 1004 100\n"
        "\n"
        "	slic_test stop_tone\n"
        "\n"
        "	slic_test tones dial\n"
        "	slic_test tones reorder\n"
        "\n"
        "	slic_test ring start\n"
        "	slic_test ring stop\n"
        "\n"
    );
}

/**
 * @brief plays standard tones.
 *
 * @param argc is number of arguments.
 * @param argv is an array of arguments.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
static int cmd_tones(int argc, char** argv)
{
    const char* tone_name;

    if (argc != 2) {
        fprintf(stderr, "missing parameter: tones <tone name>\n");
        goto err;
    }

    tone_name = argv[1];

    struct tone_map_t {
        const char* tone_name;
        int enable_timer;
        int tone_index;
    };

    struct tone_map_t tone_maps[] = {
        {"dial", FALSE, TONEGEN_FCC_DIAL},
        {"stutter", TRUE, TONEGEN_FCC_DIAL_STUTTER},
        {"reorder", TRUE, TONEGEN_FCC_REORDER},
        {"roh", TRUE, TONEGEN_FCC_ROH},
        {"confirmation", TRUE, TONEGEN_FCC_CONFIRMATION_0},
        {"callwaiting", TRUE, TONEGEN_FCC_CALLWAITING_0},

        {NULL, -1},
    };

    struct tone_map_t* tm = tone_maps;

    while (tm->tone_name) {
        if (!strcmp(tone_name, tm->tone_name)) {
            break;
        }
        tm++;
    }

    if (!tm->tone_name) {
        fprintf(stderr, "incorrect tone name\n");
        goto err;
    }

    /* load hard tone_name */
    ProSLIC_ToneGenSetup(_cptr, tm->tone_index);

    /* enable oscillators and active timers */
    if (tm->enable_timer) {
        netcomm_proslic_reg_set_and_reset(_cptr,
                                          PROSLIC_REG_OCON,
                                          PROSLIC_REG_OCON_OSCS_EN | PROSLIC_REG_OCON_OSCS_TA_EN | PROSLIC_REG_OCON_OSCS_TI_EN,
                                          0);
    } else {
        netcomm_proslic_reg_set_and_reset(_cptr,
                                          PROSLIC_REG_OCON,
                                          PROSLIC_REG_OCON_OSCS_EN,
                                          PROSLIC_REG_OCON_OSCS_TA_EN | PROSLIC_REG_OCON_OSCS_TI_EN);
    }

    return 0;

err:
    return -1;
}

/**
 * @brief stops tone.
 *
 * @param argc is number of arguments.
 * @param argv is an array of arguments.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
static int cmd_stop_tone(int argc, char** argv)
{
    netcomm_proslic_reg_set_and_reset(_cptr,
                                      PROSLIC_REG_OCON,
                                      0,
                                      PROSLIC_REG_OCON_OSCS_TA_EN | PROSLIC_REG_OCON_OSCS_TI_EN | PROSLIC_REG_OCON_OSCS_EN);

    return 0;
}

/**
 * @brief plays single frequency tone.
 *
 * @param argc is number of arguments.
 * @param argv is an array of arguments.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
static int cmd_tone(int argc, char** argv)
{
    int hz;
    int volume;

    double freq;
    double amp;

    double coeff;

    ProSLIC_Tone_Cfg tone_cfg;

    if (argc != 3) {
        fprintf(stderr, "missing parameter: tone <frequency 100 Hz - 3600 Hz> <volume 0-100>\n");
        goto err;
    }

    /* get parameters */
    hz = atoi(argv[1]);
    volume = atoi(argv[2]);

    /* get frequency and amplitude */
    coeff = cos(2 * PI_CONSTANT * hz / SAMPLING_RATE);
    freq = coeff * (1 << 14);
    amp = sqrt((1 - coeff) / (1 + coeff)) * (double)((1 << 15) - 1) / 4 * (double)volume / 100 / 1.11;

    /* configure SLIC */
    tone_cfg.osc1.freq = ((int)freq) << 13;
    tone_cfg.osc1.amp = ((int)amp) << 13;
    tone_cfg.osc1.phas = 0;
    tone_cfg.osc1.tahi = 0;
    tone_cfg.osc1.talo = 0;
    tone_cfg.omode = 0x06;
    ProSLIC_ToneGenSetupPtr(_cptr, &tone_cfg);

    /* enable oscillator1 */
    netcomm_proslic_reg_set_and_reset(_cptr,
                                      PROSLIC_REG_OCON,
                                      PROSLIC_REG_OCON_OSC1_EN,
                                      PROSLIC_REG_OCON_OSCS_TA_EN | PROSLIC_REG_OCON_OSCS_TI_EN | PROSLIC_REG_OCON_OSC2_EN);

    return 0;

err:
    return -1;
}

/**
 * @brief starts or stops ring.
 *
 * @param argc is number of arguments.
 * @param argv is an array of arguments.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
static int cmd_ring(int argc, char** argv)
{
    const char* ring_param;

    if (argc != 2) {
        fprintf(stderr, "missing parameter: ring <start|stop>\n");
        goto err;
    }

    ring_param = argv[1];

    if (!strcmp(ring_param, "start")) {

        /* setup continuous ring */
        netcomm_proslic_reg_set_and_reset(_cptr, PROSLIC_REG_RINGCON,
                                          PROSLIC_REG_RINGCON_TA_EN | PROSLIC_REG_RINGCON_TI_EN, 0);

        /* start ringing */
        netcomm_proslic_set_linefeed(_cptr, LF_RINGING);
    } else if (!strcmp(ring_param, "stop")) {

        netcomm_proslic_set_linefeed(_cptr, LF_FWD_ACTIVE);

    } else {
        fprintf(stderr, "incorrect parameter: %s\n", ring_param);
    }


    return 0;

err:
    return -1;
}

/**
 * @brief main.
 *
 * @param argc is number of arguments.
 * @param argv is an array of arguments.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
int main(int argc, char* argv[])
{
    int c;
    int errflag = 0;

    int cmd_argc;
    char** cmd_argv;
    struct command_t* cmd;
    const char* name;

    while ((c = getopt(argc, argv, "hc:")) != EOF) {
        switch (c) {

            case 'c': {
                _opt_channel = atoi(optarg);
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

    if (errflag) {
        print_usage();
        exit(-1);
    }

    cmd_argc = argc - optind;
    cmd_argv = &argv[optind];
    name = argv[optind];

    if (!name) {
        fprintf(stderr, "command missing\n");
        print_usage();
        exit(-1);
    }

    /* search command */
    cmd = _cmds;
    while (cmd->name) {
        if (!strcmp(name, cmd->name)) {
            break;
        }
        cmd++;
    }

    /* bypass if no command is matched */
    if (!cmd->name) {
        fprintf(stderr, "wrong command: '%s'\n\n", name);
        exit(-1);
    }

    /* initiate proslic */
    if (netcomm_proslic_init() < 0) {
        fprintf(stderr, "failed to initiate proslic\n");
        exit(-1);
    }

    /* get proslic voice channel type */
    _cptr = pbx_get_cptr(_opt_channel);

    /* call command function */
    cmd->func(cmd_argc, cmd_argv);

    netcomm_proslic_fini();
    return 0;


}

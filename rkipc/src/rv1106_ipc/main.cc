// Copyright 2022 Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <getopt.h>

#include "audio.h"
#include "common.h"
#include "isp.h"
#include "log.h"
#include "param.h"
#include "rockiva.h"
#include "server.h"
#include "storage.h"
#include "system.h"
#include "video.h"

#include "npu/npu.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "rkipc.c"

enum { LOG_ERROR, LOG_WARN, LOG_INFO, LOG_DEBUG };

int enable_minilog = 0;
int rkipc_log_level = LOG_INFO;

bool g_npu_run = true;
bool g_recv_data = false;
bool g_process_end = true;
pthread_mutex_t g_network_lock = PTHREAD_MUTEX_INITIALIZER;
void *g_input_data = NULL;
int g_img_width = 0;
int g_img_height = 0;
PIXEL_FORMAT_E enPixelFormat; 
VIDEO_FORMAT_E enVideoFormat;

static int g_main_run_ = 1;

char *rkipc_ini_path_ = NULL;
char *rkipc_iq_file_path_ = NULL;

static void sig_proc(int signo) {
	LOG_INFO("received signo %d \n", signo);
	g_main_run_ = 0;
}

static const char short_options[] = "c:a:l:";
static const struct option long_options[] = {{"config", required_argument, NULL, 'c'},
                                             {"aiq_file", no_argument, NULL, 'a'},
                                             {"log_level", no_argument, NULL, 'l'},
                                             {"help", no_argument, NULL, 'h'},
                                             {0, 0, 0, 0}};

static void usage_tip(FILE *fp, int argc, char **argv) {
	fprintf(fp,
	        "Usage: %s [options]\n"
	        "Version %s\n"
	        "Options:\n"
	        "-c | --config      rkipc ini file, default is "
	        "/userdata/rkipc.ini, need to be writable\n"
	        "-a | --aiq_file    aiq file dir path, default is /etc/iqfiles\n"
	        "-l | --log_level   log_level [0/1/2/3], default is 2\n"
	        "-h | --help        for help \n\n"
	        "\n",
	        argv[0], "V1.0");
}

void rkipc_get_opt(int argc, char *argv[]) {
	for (;;) {
		int idx;
		int c;
		c = getopt_long(argc, argv, short_options, long_options, &idx);
		if (-1 == c)
			break;
		switch (c) {
		case 0: /* getopt_long() flag */
			break;
		case 'c':
			rkipc_ini_path_ = optarg;
			break;
		case 'a':
			rkipc_iq_file_path_ = optarg;
			break;
		case 'l':
			rkipc_log_level = atoi(optarg);
			break;
		case 'h':
			usage_tip(stdout, argc, argv);
			exit(EXIT_SUCCESS);
		default:
			usage_tip(stderr, argc, argv);
			exit(EXIT_FAILURE);
		}
	}
}

int main(int argc, char **argv) {
	
	if(argc < 4)
	{
		printf("usage:rkipc -a /oem/usr/share/iqfiles model_path\n");
		return -1;
	}
	//model_path
	char * model_path = argv[3];
	printf("model_path:%s\n", model_path);
	if(network_init(model_path) != RKNN_SUCC)
	{
		printf("model init failed!\n");
		exit(-1);
	}

	LOG_DEBUG("main begin\n");
	signal(SIGINT, sig_proc);

	rkipc_get_opt(argc, argv);
	LOG_INFO("rkipc_ini_path_ is %s, rkipc_iq_file_path_ is %s, rkipc_log_level "
	         "is %d\n",
	         rkipc_ini_path_, rkipc_iq_file_path_, rkipc_log_level);

	// init
	rk_param_init(rkipc_ini_path_);
	rk_system_init();
	if (rk_param_get_int("video.source:enable_aiq", 1))
		rk_isp_init(0, rkipc_iq_file_path_);
	rk_isp_set_frame_rate(0, rk_param_get_int("isp.0.adjustment:fps", 30));
	RK_MPI_SYS_Init();
	rk_video_init();
	if (rk_param_get_int("audio.0:enable", 0))
		rkipc_audio_init();
	rkipc_server_init();
	rk_storage_init();
	if (rk_param_get_int("video.source:enable_npu", 0))
		rkipc_rockiva_init();

	while (g_main_run_) {
		usleep(1000 * 1000);
	}

	// deinit
	rk_storage_deinit();
	rkipc_server_deinit();
	rk_system_deinit();
	rk_video_deinit();
	if (rk_param_get_int("video.source:enable_aiq", 1))
		rk_isp_deinit(0);
	if (rk_param_get_int("audio.0:enable", 0))
		rkipc_audio_deinit();
	RK_MPI_SYS_Exit();
	if (rk_param_get_int("video.source:enable_npu", 0))
		rkipc_rockiva_deinit();
	rk_param_deinit();

	return 0;
}

// SPDX-License-Identifier: OSL-3.0
/*
 * Copyright (C) 2025  Trevor Woerner <twoerner@gmail.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <mosquitto.h>
#include <json-c/json.h>

#include "config.h"

#define NOTU __attribute__((unused))
#define DEFAULT_MQTT_SERVER "10.0.0.4"
#define DEFAULT_MQTT_PORT 1883
#define DEFAULT_LOG_PATH "/srvdata/sensor-data"

static unsigned verbose_G = 0;
static char *mqttServer_pG = DEFAULT_MQTT_SERVER;
static unsigned mqttPort_G = DEFAULT_MQTT_PORT;
static char mqttTopic_G[65535];
static struct mosquitto *mosq_G = NULL;
static char *shellyDeviceName_pG = NULL;
static char *logFileName_pG = NULL;
static int logFileFd_G = -1;

static void cleanup (void);
static void parse_cmdline (int argc, char *argv[]);
static void init_mosquitto (void);

int
main (int argc, char *argv[])
{
	atexit(cleanup);
	parse_cmdline(argc, argv);
	init_mosquitto();
	mosquitto_loop_forever(mosq_G, -1, 1);
	return 0;
}

static void
usage (char *pgm_p)
{
	printf("%s\n\n", PACKAGE_STRING);

	printf("A program to listen to a set of Shelly Gen3 H&T\n");
	printf("(humidity & temperature) devices and record the data\n\n");

	printf("usage: %s [OPTIONS]\n", pgm_p);
	printf("  where [OPTIONS] are:\n");
	printf("    -h | --help        Print help options and exit successfully\n");
	printf("    -v | --version     Print program version and exit successfully\n");
	printf("    -V | --verbose     Run program verbosely\n");
	printf("    -s | --server <s>  Specify the mqtt server to contact (default: %s)\n", DEFAULT_MQTT_SERVER);
	printf("    -p | --port <p>    Specify the port <p> to connect to on the server (default: %u)\n", DEFAULT_MQTT_PORT);
	printf("    -t | --topic <t>   Specify the full topic to which to subsribe on the server\n");
	printf("    -d | --device <d>  Specify the Shelly device, topic becomes: '<device>/events/rpc'\n");
	printf("    -l | --logfile <l> Specify the file into which to log the data\n");
	printf("                       If not specified and -d <device> is specified, log to %s/${device}.data\n", DEFAULT_LOG_PATH);
	printf("                       If not specified and -d <device> is not specified, log to stdout\n");
}

static void
cleanup (void)
{
	if (mosq_G != NULL) {
		mosquitto_destroy(mosq_G);
		mosquitto_lib_cleanup();
	}
	if (logFileName_pG != NULL) {
		free(logFileName_pG);
		if (logFileFd_G != -1)
			close(logFileFd_G);
	}
}

static void
parse_cmdline (int argc, char *argv[])
{
	int c;
	size_t maxSize;
	int badexit = 0;
	struct option longOpts[] = {
		{"help",    no_argument,       NULL, 'h'},
		{"version", no_argument,       NULL, 'v'},
		{"verbose", no_argument,       NULL, 'V'},
		{"server",  required_argument, NULL, 's'},
		{"port",    required_argument, NULL, 'p'},
		{"topic",   required_argument, NULL, 't'},
		{"device",  required_argument, NULL, 'd'},
		{"logfile", required_argument, NULL, 'l'},
		{NULL, 0, NULL, 0},
	};

	mqttTopic_G[0] = 0;
	while (1) {
		c = getopt_long(argc, argv, "hvVs:p:t:d:l:", longOpts, NULL);
		if (c == -1)
			break;
		switch (c) {
			case 'h':
				usage(argv[0]);
				exit(EXIT_SUCCESS);

			case 'v':
				printf("%s\n", PACKAGE_STRING);
				exit(EXIT_SUCCESS);

			case 'V':
				++verbose_G;
				break;

			case 's':
				mqttServer_pG = optarg;
				break;

			case 'p':
				if (sscanf(optarg, "%u", &mqttPort_G) != 1) {
					perror("sscanf()");
					exit(EXIT_FAILURE);
				}
				break;

			case 't':
				if (strlen(optarg) > (sizeof(mqttTopic_G)-1)) {
					fprintf(stderr, "topic string length is too long (len:%lu max:%lu)\n",
							strlen(optarg), sizeof(mqttTopic_G)-1);
					exit(EXIT_FAILURE);
				}
				strncpy(mqttTopic_G, optarg, sizeof(mqttTopic_G));
				break;

			case 'd':
				maxSize = sizeof(mqttTopic_G) - strlen("/events/rpc") - 1;
				if (strlen(optarg) > maxSize) {
					fprintf(stderr, "device name too long (len:%lu max:%lu)\n",
							strlen(optarg), maxSize);
					exit(EXIT_FAILURE);
				}
				sprintf(mqttTopic_G, "%s/events/rpc", optarg);
				shellyDeviceName_pG = optarg;
				break;

			case 'l':
				logFileName_pG = (char*)malloc(strlen(optarg) + 1);
				if (logFileName_pG == NULL) {
					perror("malloc(optarg)");
					exit(EXIT_FAILURE);
				}
				sprintf(logFileName_pG, "%s", optarg);
				break;

			default:
				if (verbose_G > 0)
					printf("getopt() issue: %c (0x%02x)\n", c, c);
				exit(EXIT_FAILURE);
		}
	}

	while (optind < argc) {
		++badexit;
		printf("extra cmdline arg: %s\n", argv[optind++]);
	}
	if (badexit > 0)
		exit(EXIT_FAILURE);
	if (mqttTopic_G[0] == 0) {
		printf("either a topic or a device needs to be provided\n");
		exit(EXIT_FAILURE);
	}

	// setup logging
	if ((logFileName_pG == NULL) && (shellyDeviceName_pG != NULL)) {
		logFileName_pG = (char*)malloc(strlen(DEFAULT_LOG_PATH) + strlen(shellyDeviceName_pG) + 16);
		if (logFileName_pG == NULL) {
			perror("malloc()");
			exit(EXIT_FAILURE);
		}
		sprintf(logFileName_pG, "%s/%s.data", DEFAULT_LOG_PATH, shellyDeviceName_pG);
	}
	if (verbose_G > 0)
		printf("logging to logfile: %s\n", (logFileName_pG == NULL)? "<stdout>":logFileName_pG);
	if (logFileName_pG != NULL) {
		logFileFd_G = open(logFileName_pG, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if (logFileFd_G == -1) {
			perror("open(log)");
			exit(EXIT_FAILURE);
		}
	}
	else
		logFileFd_G = fileno(stdout);
}

static void
connect_callback (struct mosquitto *mosq_p, NOTU void *userdata_p, int result)
{
	int ret;

	if (result == 0) {
		if (verbose_G > 0)
			printf("connect callback success\n");

		ret = mosquitto_subscribe(mosq_p, NULL, mqttTopic_G, 2);
		printf("%s to topic '%s'\n", ((ret == MOSQ_ERR_SUCCESS)? "subscribed" : "can't subscribe"), mqttTopic_G);
	}
}

static bool found_G = false;
static json_object *foundObj_pG = NULL;
static void
find_json_object_by_key (json_object *jsonObj_p, char *searchKey_p)
{
	size_t len;

	/* preconds */
	if (found_G)
		return;
	if (jsonObj_p == NULL)
		return;
	if (searchKey_p == NULL)
		return;
	/* preconds */

	len = strlen(searchKey_p);
	json_object_object_foreach(jsonObj_p, key_p, val_p) {
		if (found_G)
			return;

		if (strncmp(searchKey_p, key_p, len) == 0) {
			found_G = true;
			foundObj_pG = val_p;
			return;
		}

		if (json_object_get_type(val_p) == json_type_object)
			find_json_object_by_key(val_p, searchKey_p);
	}
}

/*
 * The other temperature sensors in use throughout this project (i.e.
 * sht3x, shtc3, ds18b20) report floating-point quantities (i.e.
 * temperature and humidity) by removing the decimal point and multiplying
 * by 1000. E.g. 22.924°C would be recorded as 22924, a relative humidity
 * of 77.053% would be recorded as 77053. Therefore in order to best
 * integrate the Shelly data with all the other data being recorded,
 * convert these double values the same way so this data can interact with
 * the rest of the system more easily.
 *
 * Use the following data type to map between the Shelly data and how it
 * will be reported.
 */
typedef enum {
	dt__double_as_int,
	dt__int,
	dt__END,
} DataTypes_t;

static void
report_shelly_data (json_object *jsonObj_p, char *key_p, char *label_p, DataTypes_t datatype)
{
	/* preconds */
	if (jsonObj_p == NULL)
		return;
	if (key_p == NULL)
		return;
	if (datatype >= dt__END)
		return;
	/* preconds */

	if (label_p != NULL)
		dprintf(logFileFd_G, " %s:", label_p);

	found_G = false;
	find_json_object_by_key(jsonObj_p, key_p);

	if (found_G) {
		switch (datatype) {
			case dt__double_as_int:
				if (json_object_get_type(foundObj_pG) == json_type_double) {
					dprintf(logFileFd_G, "%d", (int)(json_object_get_double(foundObj_pG) * 1000));
					return;
				}
				break;

			case dt__int:
				if (json_object_get_type(foundObj_pG) == json_type_int) {
					dprintf(logFileFd_G, "%d", json_object_get_int(foundObj_pG));
					return;
				}
				break;

			default:
				break;
		}
	}
	dprintf(logFileFd_G, "NaN");
}

static void
parse_shellyht_message (json_object *jsonObj_p)
{
	time_t now;
	char timeBuf[64];

	/* preconds */
	if (jsonObj_p == NULL)
		return;
	/* preconds */

	// when subscribed to the "<device>/events/rpc" topic, a Shelly device
	// will send 3 different messages. only the NotifyFullStatus message
	// contains all the data that is wanted
	// ignore the others
	found_G = false;
	find_json_object_by_key(jsonObj_p, "method");
	if (!found_G)
		return;
	if (json_object_get_type(foundObj_pG) != json_type_string)
		return;
	if (strncmp("NotifyFullStatus", json_object_get_string(foundObj_pG), 16) != 0)
		return;

	// timestamp
	now = time(NULL);
	strftime(timeBuf, sizeof(timeBuf), "%F %T %z", localtime(&now));
	dprintf(logFileFd_G, "%s", timeBuf);

	// data
	report_shelly_data(jsonObj_p, "tC", "temp", dt__double_as_int);
	report_shelly_data(jsonObj_p, "rh", "humidity", dt__double_as_int);
	report_shelly_data(jsonObj_p, "rssi", "rssi", dt__int);
	report_shelly_data(jsonObj_p, "percent", "battery", dt__int);
	report_shelly_data(jsonObj_p, "V", "battV", dt__double_as_int);

	dprintf(logFileFd_G, "\n");
}

static void
process_message (NOTU struct mosquitto *mosq_p, NOTU void *userdata_p, const struct mosquitto_message *msg_p)
{
	json_object *jsonObj_p;

	if (verbose_G > 1)
		printf("payload: %s\n", (char*)msg_p->payload);

	jsonObj_p = json_tokener_parse(msg_p->payload);
	parse_shellyht_message(jsonObj_p);
	json_object_put(jsonObj_p);
}

static void
init_mosquitto (void)
{
	int ret;
	unsigned sleepSec = 1;

	ret = mosquitto_lib_init();
	if (ret != MOSQ_ERR_SUCCESS) {
		printf("mosquitto library init failure\n");
		exit(EXIT_FAILURE);
	}

	mosq_G = mosquitto_new(NULL, true, NULL);
	if (mosq_G == NULL) {
		perror("mosquitto_new()");
		exit(EXIT_FAILURE);
	}

	mosquitto_connect_callback_set(mosq_G, connect_callback);
	mosquitto_message_callback_set(mosq_G, process_message);

	while (1) {
		if (verbose_G > 0)
			printf("attempting to connect to mqtt server %s port %u... ", mqttServer_pG, mqttPort_G);
		ret = mosquitto_connect(mosq_G, mqttServer_pG, (int)mqttPort_G, 10);
		if (ret == MOSQ_ERR_SUCCESS) {
			if (verbose_G > 0)
				printf("connected!\n");
			break;
		}
		sleep(sleepSec);
		if (sleepSec < 60)
			sleepSec *= 2;
	}
}

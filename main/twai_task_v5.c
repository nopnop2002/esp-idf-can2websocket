/*	TWAI Network receive Example

	This example code is in the Public Domain (or CC0 licensed, at your option.)

	Unless required by applicable law or agreed to in writing, this
	software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
	CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/message_buffer.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/twai.h" // Update from V4.2
#include "cJSON.h"

#include "twai.h"

static const char *TAG = "TWAI_V5";

extern MessageBufferHandle_t xMessageBufferMain;

extern TOPIC_t *publish;
extern int16_t npublish;

void dump_table(TOPIC_t *topics, int16_t ntopic);

static const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

#if CONFIG_CAN_BITRATE_25
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_25KBITS();
#define BITRATE "Bitrate is 25 Kbit/s"
#elif CONFIG_CAN_BITRATE_50
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_50KBITS();
#define BITRATE "Bitrate is 50 Kbit/s"
#elif CONFIG_CAN_BITRATE_100
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_100KBITS();
#define BITRATE "Bitrate is 100 Kbit/s"
#elif CONFIG_CAN_BITRATE_125
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_125KBITS();
#define BITRATE "Bitrate is 125 Kbit/s"
#elif CONFIG_CAN_BITRATE_250
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
#define BITRATE "Bitrate is 250 Kbit/s"
#elif CONFIG_CAN_BITRATE_500
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
#define BITRATE "Bitrate is 500 Kbit/s"
#elif CONFIG_CAN_BITRATE_800
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_800KBITS();
#define BITRATE "Bitrate is 800 Kbit/s"
#elif CONFIG_CAN_BITRATE_1000
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_1MBITS();
#define BITRATE "Bitrate is 1 Mbit/s"
#endif

static const twai_general_config_t g_config =
	TWAI_GENERAL_CONFIG_DEFAULT(CONFIG_CTX_GPIO, CONFIG_CRX_GPIO, TWAI_MODE_NORMAL);

// Format and print the twai message
void twai_print_frame(twai_message_t frame) {
	int ext = frame.extd;
	int rtr = frame.rtr;

	if (ext == 0) {
		printf("Standard ID: 0x%03"PRIx32"%*s", frame.identifier, 5, "");
	} else {
		printf("Extended ID: 0x%08"PRIx32, frame.identifier);
	}
	printf("  DLC: %d Data: ", frame.data_length_code);

	if (rtr == 0) {
		for (int i = 0; i < frame.data_length_code; i++) {
			printf("0x%02x ", frame.data[i]);
		}
	} else {
		printf("REMOTE REQUEST FRAME");
	}
	printf("\n");
}

void twai_task(void *pvParameters)
{
	ESP_LOGI(TAG, "Start");
	ESP_LOGI(TAG, "TWAI_BITRATE=%d",CONFIG_TWAI_BITRATE);
	ESP_LOGI(TAG, "CTX_GPIO=%d",CONFIG_CTX_GPIO);
	ESP_LOGI(TAG, "CRX_GPIO=%d",CONFIG_CRX_GPIO);

	ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
	ESP_LOGI(TAG, "Driver installed");
	ESP_ERROR_CHECK(twai_start());
	ESP_LOGI(TAG, "Driver started");

	dump_table(publish, npublish);

	bool running = true;
	while (running) {
		twai_message_t rx_msg;
		esp_err_t ret = twai_receive(&rx_msg, portMAX_DELAY);
		if (ret == ESP_OK) {
			ESP_LOGD(TAG,"twai_receive identifier=0x%"PRIu32" extd=0x%x rtr=0x%x data_length_code=%d",
				rx_msg.identifier, rx_msg.extd, rx_msg.rtr, rx_msg.data_length_code);
			int canid = rx_msg.identifier;
			int ext = rx_msg.extd;
			int rtr = rx_msg.rtr;
			ESP_LOGD(TAG, "canid=0x%x ext=0x%x rtr=0x%x", canid, ext, rtr);

#if CONFIG_ENABLE_PRINT
			twai_print_frame(rx_msg);
#endif

			for(int index=0;index<npublish;index++) {
				if (publish[index].frame != ext) continue;
				if (publish[index].canid == canid) {
					ESP_LOGI(TAG, "publish[%d] frame=%d canid=0x%"PRIx32" name=[%s] name_len=%d",
					index, publish[index].frame, publish[index].canid, publish[index].name, publish[index].name_len);

					cJSON *response;
					response = cJSON_CreateObject();
					cJSON_AddStringToObject(response, "id", "receive-data");
					cJSON_AddStringToObject(response, "name", publish[index].name);
					cJSON_AddNumberToObject(response, "name_len", publish[index].name_len);
					cJSON_AddNumberToObject(response, "canid", canid);
					cJSON_AddNumberToObject(response, "ext", ext);
					cJSON_AddNumberToObject(response, "rtr", rtr);
					int data_len = 0;
					if (rtr == 0) {
						data_len = rx_msg.data_length_code;
					}
					cJSON_AddNumberToObject(response, "data_len", data_len);
					int data[8];
					for(int i=0;i<8;i++) data[i] = rx_msg.data[i];
					cJSON *intArray;
					intArray = cJSON_CreateIntArray(data, 8);
					cJSON_AddItemToObject(response, "data", intArray);

					char *my_json_string = cJSON_Print(response);
					ESP_LOGD(TAG, "my_json_string\n%s",my_json_string);
					size_t xBytesSent = xMessageBufferSend(xMessageBufferMain, my_json_string, strlen(my_json_string), portMAX_DELAY);
					if (xBytesSent != strlen(my_json_string)) {
						ESP_LOGE(TAG, "xMessageBufferSend fail");
						running = false;
					}
					cJSON_Delete(response);
					cJSON_free(my_json_string);
				}
			} // end for
		} else {
			ESP_LOGE(TAG, "twai_receive Fail %s", esp_err_to_name(ret));
			running = false;
		}
	} // end while

	ESP_ERROR_CHECK(twai_stop());
	ESP_ERROR_CHECK(twai_driver_uninstall());
	vTaskDelete(NULL);
}


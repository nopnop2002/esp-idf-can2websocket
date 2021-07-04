/* HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_http_server.h"

#include "twai.h"

static const char *TAG = "HTTP";
static char *HTML_BODY = "/spiffs/body";
static SemaphoreHandle_t ctrl_task_sem;

extern QueueHandle_t xQueueHttp;
extern TOPIC_t *publish;
extern int16_t npublish;

static esp_err_t file2html(httpd_req_t *req, char * filename) {
	ESP_LOGI(TAG, "Reading %s", filename);
	FILE* fhtml = fopen(filename, "r");
	if (fhtml == NULL) {
		return ESP_FAIL;
	} else {
		char line[128];
		while (fgets(line, sizeof(line), fhtml) != NULL) {
			size_t linelen = strlen(line);
			//remove EOL (CR or LF)
			for (int i=linelen;i>0;i--) {
				if (line[i-1] == 0x0a) {
					line[i-1] = 0;
				} else if (line[i-1] == 0x0d) {
					line[i-1] = 0;
				} else {
					break;
				}
			}
			ESP_LOGD(TAG, "line=[%s]", line);
			esp_err_t ret = httpd_resp_sendstr_chunk(req, line);
			if (ret != ESP_OK) {
				ESP_LOGE(TAG, "httpd_resp_sendstr_chunk fail %d", ret);
			}
		}
		fclose(fhtml);
	}
	return ESP_OK;
}


/* HTTP GET handler */
static esp_err_t root_get_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "root_get_handler req->uri=[%s]", req->uri);
#if 0
	for(int index=0;index<npublish;index++) {
		ESP_LOGI(TAG, "publish[%d] frame=%d canid=0x%x topic=[%s] topic_len=%d",
		index, publish[index].frame, publish[index].canid, publish[index].topic, publish[index].topic_len);
	}
#endif

	// Send HTML header
	httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html><body>");

	httpd_resp_sendstr_chunk(req,
		"<table border=\"1\">"
		"<thead><tr><th colspan=\"4\">Data Format</th></tr></thead>"
		"<tbody>");

	httpd_resp_sendstr_chunk(req, "<tr>");
	httpd_resp_sendstr_chunk(req, "<td>");
	httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"/format/Binary\">");
	httpd_resp_sendstr_chunk(req, "<button type=\"submit\">Binary</button></form>");
	httpd_resp_sendstr_chunk(req, "</td>");

	httpd_resp_sendstr_chunk(req, "<td>");
	httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"/format/Octal\">");
	httpd_resp_sendstr_chunk(req, "<button type=\"submit\">Octal</button></form>");
	httpd_resp_sendstr_chunk(req, "</td>");

	httpd_resp_sendstr_chunk(req, "<td>");
	httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"/format/Decimal\">");
	httpd_resp_sendstr_chunk(req, "<button type=\"submit\">Decimal</button></form>");
	httpd_resp_sendstr_chunk(req, "</td>");

	httpd_resp_sendstr_chunk(req, "<td>");
	httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"/format/Hexadecimal\">");
	httpd_resp_sendstr_chunk(req, "<button type=\"submit\">Hexadecimal</button></form>");
	httpd_resp_sendstr_chunk(req, "</td>");
	httpd_resp_sendstr_chunk(req, "</tr>");

	/* Finish the file list table */
	httpd_resp_sendstr_chunk(req, "</tbody></table>");
	httpd_resp_sendstr_chunk(req, "<br><hr><br>");

	// Send html from file
	xSemaphoreTake(ctrl_task_sem, portMAX_DELAY);
	file2html(req, HTML_BODY);
	xSemaphoreGive(ctrl_task_sem);

	/* Send remaining chunk of HTML file to complete it */
	httpd_resp_sendstr_chunk(req, "</body></html>");

	/* Send empty chunk to signal HTTP response completion */
	httpd_resp_sendstr_chunk(req, NULL);

	return ESP_OK;
}

/* HTTP format POST handler */
static esp_err_t root_post_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "root_post_handler req->uri=[%s]", req->uri);
	ESP_LOGI(TAG, "root_post_handler req->uri=[%s]", req->uri+sizeof("/format"));
	FRAME_t frameBuf;
	if (strcmp(req->uri+sizeof("/format"), "Binary") == 0) {
		frameBuf.command = CMD_BINARY;
	} else if (strcmp(req->uri+sizeof("/format"), "Octal") == 0) {
		frameBuf.command = CMD_OCTAL;
	} else if (strcmp(req->uri+sizeof("/format"), "Decimal") == 0) {
		frameBuf.command = CMD_DECIMAL;
	} else if (strcmp(req->uri+sizeof("/format"), "Hexadecimal") == 0) {
		frameBuf.command = CMD_HEXA;
	}
	if (xQueueSend(xQueueHttp, &frameBuf, portMAX_DELAY) != pdPASS) {
		ESP_LOGE(TAG, "xQueueSend Fail");
	}

	/* Redirect onto root to see the updated file list */
	httpd_resp_set_status(req, "303 See Other");
	httpd_resp_set_hdr(req, "Location", "/");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
	httpd_resp_set_hdr(req, "Connection", "close");
#endif
	httpd_resp_sendstr(req, "post successfully");
	return ESP_OK;
}

#if 0
/* HTTP view POST handler */
static esp_err_t root_view_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "root_view_handler req->uri=[%s]", req->uri);

	/* Redirect onto root to see the updated file list */
	httpd_resp_set_status(req, "303 See Other");
	httpd_resp_set_hdr(req, "Location", "/");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
	httpd_resp_set_hdr(req, "Connection", "close");
#endif
	httpd_resp_sendstr(req, "view successfully");
	return ESP_OK;
}

/* HTTP hide POST handler */
static esp_err_t root_hide_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "root_hide_handler req->uri=[%s]", req->uri);

	/* Redirect onto root to see the updated file list */
	httpd_resp_set_status(req, "303 See Other");
	httpd_resp_set_hdr(req, "Location", "/");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
	httpd_resp_set_hdr(req, "Connection", "close");
#endif
	httpd_resp_sendstr(req, "hide successfully");
	return ESP_OK;
}
#endif


/* Function to start the file server */
esp_err_t start_server(const char *base_path, int port)
{
	httpd_handle_t server = NULL;
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	config.server_port = port;

	/* Use the URI wildcard matching function in order to
	 * allow the same handler to respond to multiple different
	 * target URIs which match the wildcard scheme */
	config.uri_match_fn = httpd_uri_match_wildcard;

	ESP_LOGI(TAG, "Starting HTTP Server on port: '%d'", config.server_port);
	if (httpd_start(&server, &config) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to start file server!");
		return ESP_FAIL;
	}

	/* URI handler for GET */
	httpd_uri_t root_get = {
		.uri	   = "/",	// Match all URIs of type /path/to/file
		.method    = HTTP_GET,
		.handler   = root_get_handler,
		//.user_ctx  = server_data	// Pass server data as context
	};
	httpd_register_uri_handler(server, &root_get);

	/* URI handler for POST */
	httpd_uri_t root_post = {
		.uri	   = "/format/*",	// Match all URIs of type /format/name
		.method    = HTTP_POST,
		.handler   = root_post_handler,
		//.user_ctx  = server_data	// Pass server data as context
	};
	httpd_register_uri_handler(server, &root_post);

#if 0
	/* URI handler for view */
	httpd_uri_t root_view = {
		.uri	   = "/view/*",	// Match all URIs of type /seletc/name
		.method    = HTTP_POST,
		.handler   = root_view_handler,
		//.user_ctx  = server_data	// Pass server data as context
	};
	httpd_register_uri_handler(server, &root_view);

	/* URI handler for hide */
	httpd_uri_t root_hide = {
		.uri	   = "/hide/*",	// Match all URIs of type /seletc/name
		.method    = HTTP_POST,
		.handler   = root_hide_handler,
		//.user_ctx  = server_data	// Pass server data as context
	};
	httpd_register_uri_handler(server, &root_hide);
#endif

	return ESP_OK;
}

QUEUE_t *queue_init(int n)
{
	QUEUE_t *q = malloc(sizeof(QUEUE_t));
	if (q != NULL) {
		q->head = 0;
		q->tail = 0;
		q->count = 0;
		q->size = n;
		q->data = malloc(sizeof(FRAME_t) * n);
		if (q->data == NULL) {
			free(q);
			return NULL;
		}
	}
	return q;
}

void queue_clear(QUEUE_t *q)
{
	q->head = 0;
	q->tail = 0;
	q->count = 0;
}

int queue_count(QUEUE_t *q)
{
	return q->count;
}


void queue_dump(QUEUE_t *q)
{
	ESP_LOGI(TAG, "q->head=%d q->tail=%d q->count=%d q->size=%d", q->head, q->tail, q->count, q->size);
	int index = q->head;
	for(int i=0;i<q->count;i++) {
		ESP_LOGI(TAG, "q->data[%d] now=%d topic=%s canid=%x",
		i, (q->data+index)->now, (q->data+index)->topic, (q->data+index)->canid);
		index++;
		if (index == q->size) index = 0;
	}
}

int queue_push(QUEUE_t *q, FRAME_t *data)
{
	ESP_LOGD(TAG, "q->head=%d q->tail=%d q->count=%d q->size=%d", q->head, q->tail, q->count, q->size);
	memcpy( q->data+q->tail, data, sizeof(FRAME_t));
	q->tail++;
	if (q->count < q->size) q->count++;
	if (q->tail == q->size) {
		q->tail = 0;
	}
	if (q->count == q->size) {
		q->head = q->tail;
	}
	ESP_LOGI(TAG, "q->head=%d q->tail=%d q->count=%d q->size=%d", q->head, q->tail, q->count, q->size);
	return(q->count);
}

bool queue_pop(QUEUE_t *q, int index, FRAME_t *data)
{
	ESP_LOGD(TAG, "q->head=%d q->tail=%d q->count=%d q->size=%d", q->head, q->tail, q->count, q->size);
	if (q->count == 0)	return false;
	int _index = q->head+index;
	if (_index >= q->size) _index = _index - q->size;
	memcpy(data, q->data+_index, sizeof(FRAME_t));
	return(true);
}

#define BCD(c) 5 * (5 * (5 * (5 * (5 * (5 * (5 * (c & 128) + (c & 64)) + (c & 32)) + (c & 16)) + (c & 8)) + (c & 4)) + (c & 2)) + (c & 1)

esp_err_t queue_write(QUEUE_t *q, int format)
{
	int count = queue_count(q);
	FRAME_t frameBuf;
	esp_err_t ret = ESP_OK;
	// Write html body
	ESP_LOGD(TAG, "File Write start");
	xSemaphoreTake(ctrl_task_sem, portMAX_DELAY);
	FILE *fhtml = fopen(HTML_BODY, "w");
	if (fhtml == NULL) {
		ESP_LOGE(TAG, "fopen HTML_BODY fail");
		ret = ESP_FAIL;
	} else {
		fprintf(fhtml, "<table border=\"1\">\n");
		fprintf(fhtml, "<thead><tr style=\"color:#ffffff;\" bgcolor=\"#800000\">\n");
		fprintf(fhtml, "<th>Tick</th><th>Frame Type</th><th>Frame ID</th><th>Name</th><th colspan=\"8\">Data</th>\n");
		fprintf(fhtml, "</tr></thead>\n");
		// Pop RingBuffer
		for(int i=0;i<count;i++) {
			queue_pop(q, i, &frameBuf);
			ESP_LOGI(TAG, "now=%d canid=%x ext=%d topic=[%s] data_len=%d",
			frameBuf.now, frameBuf.canid, frameBuf.ext, frameBuf.topic, frameBuf.data_len);
			char frameType[16];
			strcpy(frameType, "Standard");
			if (frameBuf.ext == 1) strcpy(frameType, "Entended");
			fprintf(fhtml, "<tr><td>%d</td><td>%s</td><td>0x%x</td><td>%s</td>\n",
			frameBuf.now, frameType, frameBuf.canid, frameBuf.topic);
			for (int j=0;j<8;j++) {
				if (j < frameBuf.data_len) {
					if (format == CMD_BINARY) {
						fprintf(fhtml, "<td>0b%08d</td>\n", BCD(frameBuf.data[j]));
					} else if (format == CMD_OCTAL) {
						fprintf(fhtml, "<td>0o%03o</td>\n", frameBuf.data[j]);
					} else if (format == CMD_DECIMAL) {
						fprintf(fhtml, "<td>%03u</td>\n", frameBuf.data[j]);
					} else if (format == CMD_HEXA) {
						fprintf(fhtml, "<td>0x%02x</td>\n", frameBuf.data[j]);
					}
				} else {
					fprintf(fhtml, "<td><br></td>");
				}
			}
			fprintf(fhtml, "</tr>\n");
		}
		fprintf(fhtml, "</tbody></table>");
		fclose(fhtml);
	}
	xSemaphoreGive(ctrl_task_sem);
	return ret;
}

#define MAX_ROWS 20

void http_server_task(void *pvParameters)
{
	char *task_parameter = (char *)pvParameters;
	ESP_LOGI(TAG, "Start task_parameter=%s", task_parameter);
	char url[64];
	sprintf(url, "http://%s:%d", task_parameter, CONFIG_WEB_PORT);
	ESP_LOGI(TAG, "Starting server on %s", url);

	// Create Semaphore
	// This Semaphore is used for file locking
	ctrl_task_sem = xSemaphoreCreateBinary();
	configASSERT( ctrl_task_sem );
	xSemaphoreGive(ctrl_task_sem);

	// Create RingBuffer
	QUEUE_t *que = queue_init(MAX_ROWS);
	if (que == NULL) {
		ESP_LOGE(TAG, "queue_init fail");
	}

	// Clear RingBuffer
	queue_clear(que);

	// Start Server
	ESP_LOGI(TAG, "Starting server on %s", url);
	ESP_ERROR_CHECK(start_server("/spiffs", CONFIG_WEB_PORT));
	
	FRAME_t frameBuf;
	int format = CMD_HEXA;
	while(1) {
		//Waiting for HTTP event.
		if (xQueueReceive(xQueueHttp, &frameBuf, portMAX_DELAY) == pdTRUE) {
			ESP_LOGI(TAG, "command=%d", frameBuf.command);
			if (frameBuf.command == CMD_CAPTURE) {
				ESP_LOGI(TAG, "canid=%x ext=%d topic=[%s] data_len=%d",
				frameBuf.canid, frameBuf.ext, frameBuf.topic, frameBuf.data_len);
				for (int j=0;j<frameBuf.data_len;j++) {
					ESP_LOGI(TAG, "0x%x", frameBuf.data[j]);
				}
				ESP_LOGI(TAG, "Open this in your browser %s", url);

				// Push RingBuffer
				queue_push(que, &frameBuf);
				//queue_dump(que);
				queue_write(que, format);
			} else if (frameBuf.command == CMD_BINARY) {
				ESP_LOGI(TAG, "foemat is BINARY");
				format = CMD_BINARY;
				queue_write(que, format);
			} else if (frameBuf.command == CMD_OCTAL) {
				ESP_LOGI(TAG, "foemat is OCTAL");
				format = CMD_OCTAL;
				queue_write(que, format);
			} else if (frameBuf.command == CMD_DECIMAL) {
				ESP_LOGI(TAG, "foemat is DECIMAL");
				format = CMD_DECIMAL;
				queue_write(que, format);
			} else if (frameBuf.command == CMD_HEXA) {
				ESP_LOGI(TAG, "foemat is HEXA");
				format = CMD_HEXA;
				queue_write(que, format);
			} else if (frameBuf.command == CMD_VIEW) {
				ESP_LOGI(TAG, "view canid=0x%x ext=%d", frameBuf.canid, frameBuf.ext);
			} else if (frameBuf.command == CMD_HIDE) {
				ESP_LOGI(TAG, "hide canid=0x%x ext=%d", frameBuf.canid, frameBuf.ext);
			}
		}
	}

	// Never reach here
	ESP_LOGI(TAG, "finish");
	vTaskDelete(NULL);
}

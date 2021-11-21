/*
	 Serial monitor client example using WEB Socket.
	 This example code is in the Public Domain (or CC0 licensed, at your option.)
	 Unless required by applicable law or agreed to in writing, this
	 software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
	 CONDITIONS OF ANY KIND, either express or implied.
*/

#include "string.h"
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/message_buffer.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_vfs.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h" 
#include "mdns.h"
#include "lwip/dns.h"
#include "cJSON.h"

#include "twai.h"
#include "websocket_server.h"

static QueueHandle_t client_queue;
MessageBufferHandle_t xMessageBufferMain;

const static int client_queue_size = 10;

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT			 BIT1

static int s_retry_num = 0;

static const char *TAG = "MAIN";

TOPIC_t *publish;
int16_t	npublish;

//#define __DEBUG__

static void event_handler(void* arg, esp_event_base_t event_base,
																int32_t event_id, void* event_data)
{
		if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
				esp_wifi_connect();
		} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
				if (s_retry_num < CONFIG_ESP_MAXIMUM_RETRY) {
						esp_wifi_connect();
						s_retry_num++;
						ESP_LOGI(TAG, "retry to connect to the AP");
				} else {
						xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
				}
				ESP_LOGI(TAG,"connect to the AP fail");
		} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
				ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
				ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
				s_retry_num = 0;
				xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
		}
}

void wifi_init_sta(void)
{
	s_wifi_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_t *netif = esp_netif_create_default_wifi_sta();
	assert(netif);

#if CONFIG_STATIC_IP

	ESP_LOGI(TAG, "CONFIG_STATIC_IP_ADDRESS=[%s]",CONFIG_STATIC_IP_ADDRESS);
	ESP_LOGI(TAG, "CONFIG_STATIC_GW_ADDRESS=[%s]",CONFIG_STATIC_GW_ADDRESS);
	ESP_LOGI(TAG, "CONFIG_STATIC_NM_ADDRESS=[%s]",CONFIG_STATIC_NM_ADDRESS);

	/* Stop DHCP client */
	ESP_ERROR_CHECK(esp_netif_dhcpc_stop(netif));
	ESP_LOGI(TAG, "Stop DHCP Services");

	/* Set STATIC IP Address */
	esp_netif_ip_info_t ip_info;
	memset(&ip_info, 0 , sizeof(esp_netif_ip_info_t));
	ip_info.ip.addr = ipaddr_addr(CONFIG_STATIC_IP_ADDRESS);
	ip_info.netmask.addr = ipaddr_addr(CONFIG_STATIC_NM_ADDRESS);
	ip_info.gw.addr = ipaddr_addr(CONFIG_STATIC_GW_ADDRESS);;
	esp_netif_set_ip_info(netif, &ip_info);


	/*
	I referred from here.
	https://www.esp32.com/viewtopic.php?t=5380
	if we should not be using DHCP (for example we are using static IP addresses),
	then we need to instruct the ESP32 of the locations of the DNS servers manually.
	Google publicly makes available two name servers with the addresses of 8.8.8.8 and 8.8.4.4.
	*/

	ip_addr_t d;
	d.type = IPADDR_TYPE_V4;
	d.u_addr.ip4.addr = 0x08080808; //8.8.8.8 dns
	dns_setserver(0, &d);
	d.u_addr.ip4.addr = 0x08080404; //8.8.4.4 dns
	dns_setserver(1, &d);

#endif

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
		ESP_EVENT_ANY_ID,
		&event_handler,
		NULL,
		&instance_any_id));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
		IP_EVENT_STA_GOT_IP,
		&event_handler,
		NULL,
		&instance_got_ip));

	wifi_config_t wifi_config = {
		.sta = {
			.ssid = CONFIG_ESP_WIFI_SSID,
			.password = CONFIG_ESP_WIFI_PASSWORD,
			/* Setting a password implies station will connect to all security modes including WEP/WPA.
			 * However these modes are deprecated and not advisable to be used. Incase your Access point
			 * doesn't support WPA2, these mode can be enabled by commenting below line */
			.threshold.authmode = WIFI_AUTH_WPA2_PSK,

			.pmf_cfg = {
				.capable = true,
				.required = false
			},
		},
	};
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
	ESP_ERROR_CHECK(esp_wifi_start() );

	ESP_LOGI(TAG, "wifi_init_sta finished.");

	/* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
	 * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
	EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
		WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
		pdFALSE,
		pdFALSE,
		portMAX_DELAY);

	/* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
	 * happened. */
	if (bits & WIFI_CONNECTED_BIT) {
		ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
			CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
	} else if (bits & WIFI_FAIL_BIT) {
		ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
			CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
	} else {
		ESP_LOGE(TAG, "UNEXPECTED EVENT");
	}

	/* The event will not be processed after unregister */
	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
	vEventGroupDelete(s_wifi_event_group);
}

void initialise_mdns(void)
{
	//initialize mDNS
	ESP_ERROR_CHECK( mdns_init() );
	//set mDNS hostname (required if you want to advertise services)
	ESP_ERROR_CHECK( mdns_hostname_set(CONFIG_MDNS_HOSTNAME) );
	ESP_LOGI(TAG, "mdns hostname set to: [%s]", CONFIG_MDNS_HOSTNAME);

#if 0
	//set default mDNS instance name
	ESP_ERROR_CHECK( mdns_instance_name_set("ESP32 with mDNS") );
#endif
}

esp_err_t mountSPIFFS(char * partition_label, char * base_path) {
	ESP_LOGI(TAG, "Initializing SPIFFS file system");

	esp_vfs_spiffs_conf_t conf = {
		.base_path = base_path,
		.partition_label = partition_label,
		.max_files = 5,
		.format_if_mount_failed = true
	};

	// Use settings defined above to initialize and mount SPIFFS filesystem.
	// Note: esp_vfs_spiffs_register is an all-in-one convenience function.
	esp_err_t ret = esp_vfs_spiffs_register(&conf);

	if (ret != ESP_OK) {
		if (ret == ESP_FAIL) {
			ESP_LOGE(TAG, "Failed to mount or format filesystem");
		} else if (ret == ESP_ERR_NOT_FOUND) {
			ESP_LOGE(TAG, "Failed to find SPIFFS partition");
		} else {
			ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
		}
		return ret;
	}

	size_t total = 0, used = 0;
	ret = esp_spiffs_info(partition_label, &total, &used);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
	} else {
		ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
		DIR* dir = opendir(base_path);
		assert(dir != NULL);
		while (true) {
			struct dirent*pe = readdir(dir);
			if (!pe) break;
			ESP_LOGI(TAG, "d_name=%s d_ino=%d d_type=%x", pe->d_name,pe->d_ino, pe->d_type);
		}
		closedir(dir);
	}
	ESP_LOGI(TAG, "Mount SPIFFS filesystem");
	return ret;
}

esp_err_t build_table(TOPIC_t **topics, char *file, int16_t *ntopic)
{
	ESP_LOGI(TAG, "build_table file=%s", file);
	char line[128];
	int _ntopic = 0;

	FILE* f = fopen(file, "r");
	if (f == NULL) {
		ESP_LOGE(TAG, "Failed to open file for reading");
		return ESP_FAIL;
	}
	while (1){
		if ( fgets(line, sizeof(line) ,f) == 0 ) break;
		// strip newline
		char* pos = strchr(line, '\n');
		if (pos) {
			*pos = '\0';
		}
		ESP_LOGD(TAG, "line=[%s]", line);
		if (strlen(line) == 0) continue;
		if (line[0] == '#') continue;
		_ntopic++;
	}
	fclose(f);
	ESP_LOGI(TAG, "build_table _ntopic=%d", _ntopic);
	
	*topics = calloc(_ntopic, sizeof(TOPIC_t));
	if (*topics == NULL) {
		ESP_LOGE(TAG, "Error allocating memory for topic");
		return ESP_ERR_NO_MEM;
	}

	f = fopen(file, "r");
	if (f == NULL) {
		ESP_LOGE(TAG, "Failed to open file for reading");
		return ESP_FAIL;
	}

	char *ptr;
	int index = 0;
	while (1){
		if ( fgets(line, sizeof(line) ,f) == 0 ) break;
		// strip newline
		char* pos = strchr(line, '\n');
		if (pos) {
			*pos = '\0';
		}
		ESP_LOGD(TAG, "line=[%s]", line);
		if (strlen(line) == 0) continue;
		if (line[0] == '#') continue;

		// Frame type
		ptr = strtok(line, ",");
		ESP_LOGD(TAG, "ptr=%s", ptr);
		if (strcmp(ptr, "S") == 0) {
			(*topics+index)->frame = 0;
		} else if (strcmp(ptr, "E") == 0) {
			(*topics+index)->frame = 1;
		} else {
			ESP_LOGE(TAG, "This line is invalid [%s]", line);
			continue;
		}

		// CAN ID
		uint32_t canid;
		ptr = strtok(NULL, ",");
		if(ptr == NULL) continue;
		ESP_LOGD(TAG, "ptr=%s", ptr);
		canid = strtol(ptr, NULL, 16);
		if (canid == 0) {
			ESP_LOGE(TAG, "This line is invalid [%s]", line);
			continue;
		}
		(*topics+index)->canid = canid;

		// mqtt topic
		char *sp;
		ptr = strtok(NULL, ",");
		if(ptr == NULL) {
			ESP_LOGE(TAG, "This line is invalid [%s]", line);
			continue;
		}
		ESP_LOGD(TAG, "ptr=[%s] strlen=%d", ptr, strlen(ptr));
		sp = strstr(ptr,"#");
		if (sp != NULL) {
			ESP_LOGE(TAG, "This line is invalid [%s]", line);
			continue;
		}
		sp = strstr(ptr,"+");
		if (sp != NULL) {
			ESP_LOGE(TAG, "This line is invalid [%s]", line);
			continue;
		}
		(*topics+index)->name = (char *)malloc(strlen(ptr)+1);
		strcpy((*topics+index)->name, ptr);
		(*topics+index)->name_len = strlen(ptr);
		index++;
	}
	fclose(f);
	*ntopic = index;
	return ESP_OK;
}

void dump_table(TOPIC_t *topics, int16_t ntopic)
{
	for(int i=0;i<ntopic;i++) {
		ESP_LOGI(pcTaskGetTaskName(0), "topics[%d] frame=%d canid=0x%x name=[%s] name_len=%d",
		i, (topics+i)->frame, (topics+i)->canid, (topics+i)->name, (topics+i)->name_len);
	}

}


// handles websocket events
void websocket_callback(uint8_t num,WEBSOCKET_TYPE_t type,char* msg,uint64_t len) {
	const static char* TAG = "websocket_callback";
	//int value;

	switch(type) {
		case WEBSOCKET_CONNECT:
			ESP_LOGI(TAG,"client %i connected!",num);
			break;
		case WEBSOCKET_DISCONNECT_EXTERNAL:
			ESP_LOGI(TAG,"client %i sent a disconnect message",num);
			break;
		case WEBSOCKET_DISCONNECT_INTERNAL:
			ESP_LOGI(TAG,"client %i was disconnected",num);
			break;
		case WEBSOCKET_DISCONNECT_ERROR:
			ESP_LOGI(TAG,"client %i was disconnected due to an error",num);
			break;
		case WEBSOCKET_TEXT:
			if(len) { // if the message length was greater than zero
				ESP_LOGI(TAG, "got message length %i: %s", (int)len, msg);
				size_t xBytesSent = xMessageBufferSend(xMessageBufferMain, msg, len, portMAX_DELAY);
				if (xBytesSent != len) {
					ESP_LOGE(TAG, "xMessageBufferSend fail");
				}
			}
			break;
		case WEBSOCKET_BIN:
			ESP_LOGI(TAG,"client %i sent binary message of size %i:\n%s",num,(uint32_t)len,msg);
			break;
		case WEBSOCKET_PING:
			ESP_LOGI(TAG,"client %i pinged us with message of size %i:\n%s",num,(uint32_t)len,msg);
			break;
		case WEBSOCKET_PONG:
			ESP_LOGI(TAG,"client %i responded to the ping",num);
			break;
	}
}

// serves any clients
static void http_serve(struct netconn *conn) {
	const static char* TAG = "http_server";
	const static char HTML_HEADER[] = "HTTP/1.1 200 OK\nContent-type: text/html\n\n";
	const static char ERROR_HEADER[] = "HTTP/1.1 404 Not Found\nContent-type: text/html\n\n";
	const static char JS_HEADER[] = "HTTP/1.1 200 OK\nContent-type: text/javascript\n\n";
	const static char CSS_HEADER[] = "HTTP/1.1 200 OK\nContent-type: text/css\n\n";
	//const static char PNG_HEADER[] = "HTTP/1.1 200 OK\nContent-type: image/png\n\n";
	const static char ICO_HEADER[] = "HTTP/1.1 200 OK\nContent-type: image/x-icon\n\n";
	//const static char PDF_HEADER[] = "HTTP/1.1 200 OK\nContent-type: application/pdf\n\n";
	//const static char EVENT_HEADER[] = "HTTP/1.1 200 OK\nContent-Type: text/event-stream\nCache-Control: no-cache\nretry: 3000\n\n";
	struct netbuf* inbuf;
	static char* buf;
	static uint16_t buflen;
	static err_t err;

	// default page
	extern const uint8_t root_html_start[] asm("_binary_root_html_start");
	extern const uint8_t root_html_end[] asm("_binary_root_html_end");
	const uint32_t root_html_len = root_html_end - root_html_start;

	// main.js
	extern const uint8_t main_js_start[] asm("_binary_main_js_start");
	extern const uint8_t main_js_end[] asm("_binary_main_js_end");
	const uint32_t main_js_len = main_js_end - main_js_start;

	// main.css
	extern const uint8_t main_css_start[] asm("_binary_main_css_start");
	extern const uint8_t main_css_end[] asm("_binary_main_css_end");
	const uint32_t main_css_len = main_css_end - main_css_start;

#if 0
	// bulma.css
	extern const uint8_t bulma_css_start[] asm("_binary_bulma_css_start");
	extern const uint8_t bulma_css_end[] asm("_binary_bulma_css_end");
	const uint32_t bulma_css_len = bulma_css_end - bulma_css_start;
#endif

	// favicon.ico
	extern const uint8_t favicon_ico_start[] asm("_binary_favicon_ico_start");
	extern const uint8_t favicon_ico_end[] asm("_binary_favicon_ico_end");
	const uint32_t favicon_ico_len = favicon_ico_end - favicon_ico_start;

	// error page
	extern const uint8_t error_html_start[] asm("_binary_error_html_start");
	extern const uint8_t error_html_end[] asm("_binary_error_html_end");
	const uint32_t error_html_len = error_html_end - error_html_start;

	netconn_set_recvtimeout(conn,1000); // allow a connection timeout of 1 second
	ESP_LOGI(TAG,"reading from client...");
	err = netconn_recv(conn, &inbuf);
	ESP_LOGI(TAG,"read from client");
	if(err==ERR_OK) {
		netbuf_data(inbuf, (void**)&buf, &buflen);
		if(buf) {

			ESP_LOGD(TAG, "buf=[%s]", buf);
			// default page
			if		 (strstr(buf,"GET / ")
					&& !strstr(buf,"Upgrade: websocket")) {
				ESP_LOGI(TAG,"Sending /");
				netconn_write(conn, HTML_HEADER, sizeof(HTML_HEADER)-1,NETCONN_NOCOPY);
				netconn_write(conn, root_html_start,root_html_len,NETCONN_NOCOPY);
				netconn_close(conn);
				netconn_delete(conn);
				netbuf_delete(inbuf);
			}

			// default page websocket
			else if(strstr(buf,"GET / ")
					 && strstr(buf,"Upgrade: websocket")) {
				ESP_LOGI(TAG,"Requesting websocket on /");
				ws_server_add_client(conn,buf,buflen,"/",websocket_callback);
				netbuf_delete(inbuf);
			}

			else if(strstr(buf,"GET /main.js ")) {
				ESP_LOGI(TAG,"Sending /main.js");
				netconn_write(conn, JS_HEADER, sizeof(JS_HEADER)-1,NETCONN_NOCOPY);
				netconn_write(conn, main_js_start, main_js_len,NETCONN_NOCOPY);
				netconn_close(conn);
				netconn_delete(conn);
				netbuf_delete(inbuf);
			}

			else if(strstr(buf,"GET /main.css ")) {
				ESP_LOGI(TAG,"Sending /main.css");
				netconn_write(conn, CSS_HEADER, sizeof(CSS_HEADER)-1,NETCONN_NOCOPY);
				netconn_write(conn, main_css_start, main_css_len,NETCONN_NOCOPY);
				netconn_close(conn);
				netconn_delete(conn);
				netbuf_delete(inbuf);
			}

#if 0
			else if(strstr(buf,"GET /bulma.css ")) {
				ESP_LOGI(TAG,"Sending /bulma.css");
				netconn_write(conn, CSS_HEADER, sizeof(CSS_HEADER)-1,NETCONN_NOCOPY);
				netconn_write(conn, bulma_css_start, bulma_css_len,NETCONN_NOCOPY);
				netconn_close(conn);
				netconn_delete(conn);
				netbuf_delete(inbuf);
			}
#endif

			else if(strstr(buf,"GET /favicon.ico ")) {
				ESP_LOGI(TAG,"Sending favicon.ico");
				netconn_write(conn,ICO_HEADER,sizeof(ICO_HEADER)-1,NETCONN_NOCOPY);
				netconn_write(conn,favicon_ico_start,favicon_ico_len,NETCONN_NOCOPY);
				netconn_close(conn);
				netconn_delete(conn);
				netbuf_delete(inbuf);
			}

			else if(strstr(buf,"GET /")) {
				ESP_LOGE(TAG,"Unknown request, sending error page: %s",buf);
				netconn_write(conn, ERROR_HEADER, sizeof(ERROR_HEADER)-1,NETCONN_NOCOPY);
				netconn_write(conn, error_html_start, error_html_len,NETCONN_NOCOPY);
				netconn_close(conn);
				netconn_delete(conn);
				netbuf_delete(inbuf);
			}

			else {
				ESP_LOGE(TAG,"Unknown request");
				netconn_close(conn);
				netconn_delete(conn);
				netbuf_delete(inbuf);
			}
		}
		else {
			ESP_LOGI(TAG,"Unknown request (empty?...)");
			netconn_close(conn);
			netconn_delete(conn);
			netbuf_delete(inbuf);
		}
	}
	else { // if err==ERR_OK
		ESP_LOGI(TAG,"error on read, closing connection");
		netconn_close(conn);
		netconn_delete(conn);
		netbuf_delete(inbuf);
	}
}

// handles clients when they first connect. passes to a queue
static void server_task(void* pvParameters) {
	const static char* TAG = "server_task";
	char *task_parameter = (char *)pvParameters;
	ESP_LOGI(TAG, "Start task_parameter=%s", task_parameter);
	char url[64];
	sprintf(url, "http://%s", task_parameter);
	ESP_LOGI(TAG, "Starting server on %s", url);

	struct netconn *conn, *newconn;
	static err_t err;
	client_queue = xQueueCreate(client_queue_size,sizeof(struct netconn*));
	configASSERT( client_queue );

	conn = netconn_new(NETCONN_TCP);
	netconn_bind(conn,NULL,80);
	netconn_listen(conn);
	ESP_LOGI(TAG,"server listening");
	do {
		err = netconn_accept(conn, &newconn);
		ESP_LOGI(TAG,"new client");
		if(err == ERR_OK) {
			xQueueSendToBack(client_queue,&newconn,portMAX_DELAY);
			//http_serve(newconn);
		}
	} while(err == ERR_OK);
	netconn_close(conn);
	netconn_delete(conn);
	ESP_LOGE(TAG,"task ending, rebooting board");
	esp_restart();
}

// receives clients from queue, handles them
static void server_handle_task(void* pvParameters) {
	const static char* TAG = "server_handle_task";
	struct netconn* conn;
	ESP_LOGI(TAG,"task starting");
	for(;;) {
		xQueueReceive(client_queue,&conn,portMAX_DELAY);
		if(!conn) continue;
		http_serve(conn);
	}
	vTaskDelete(NULL);
}

static int makeSendText(char* buf, char* c0, char* c1, char* c2, char* c3)
{
	char DEL = 0x04;
	sprintf(buf,"%s%c%s%c%s%c%s", c0, DEL, c1, DEL, c2, DEL, c3);
	ESP_LOGD(TAG, "buf=[%s]", buf);
	return strlen(buf);
}

#define BCD(c) 5 * (5 * (5 * (5 * (5 * (5 * (5 * (c & 128) + (c & 64)) + (c & 32)) + (c & 16)) + (c & 8)) + (c & 4)) + (c & 2)) + (c & 1)

static int makeSendFrame(char* buf, int format, TickType_t tick, FRAME_t frame)
{
	ESP_LOGD(TAG, "makeSendFrame format=%d", format);
	char DEL = 0x04;
	sprintf(buf,"DATA%c%d", DEL, tick);
	char wk[64];
	if (frame.ext == STANDARD_FRAME) {
		sprintf(wk,"%cStandard", DEL);
	} else {
		sprintf(wk,"%cExtended", DEL);
	}
	strcat(buf, wk);
	sprintf(wk,"%c0x%x", DEL, frame.canid);
	strcat(buf, wk);
	sprintf(wk,"%c%.*s", DEL, frame.name_len, frame.name);
	strcat(buf, wk);
	for(int i=0;i<8;i++) {
		ESP_LOGD(TAG, "frame.data[%d]=%d", i, frame.data[i]);
		if (i < frame.data_len) {
			if (format == FORMAT_BINARY) {
				sprintf(wk, "%c0b%08d", DEL, BCD(frame.data[i]));
			} else if (format == FORMAT_OCTAL) {
				sprintf(wk, "%c0o%03o", DEL, frame.data[i]);
			} else if (format == FORMAT_DECIMAL) {
				sprintf(wk, "%c%03u", DEL, frame.data[i]);
			} else if (format == FORMAT_HEXADECIMAL) {
				sprintf(wk, "%c0x%02x", DEL, frame.data[i]);
			}
		} else {
			sprintf(wk, "%c", DEL);
		}
		strcat(buf, wk);
	}
	ESP_LOGD(TAG, "buf=[%s]", buf);
	return strlen(buf);
}

#if defined __DEBUG__
static void time_task(void* pvParameters) {
	const static char* TAG = "time_task";
	ESP_LOGI(TAG,"starting task");

	char out[256];
	FRAME_t frame;
	char name[64];
	strcpy(name,"name");
	frame.name = name;
	frame.name_len = strlen(frame.name);
	frame.canid = 0x101;
	frame.ext = STANDARD_FRAME;
	frame.rtr = MESSAGE_FRAME;
	frame.data_len = 0;
	frame.data[0]=10;
	frame.data[1]=20;
	frame.data[2]=30;
	frame.data[3]=40;
	frame.data[4]=50;
	frame.data[5]=60;
	frame.data[6]=70;
	frame.data[7]=80;
	while(1) {
		TickType_t tick = xTaskGetTickCount();
		int len = makeSendFrame(out, FORMAT_BINARY, tick, frame);
		ESP_LOGI(TAG, "makeSendData len=%d", len);
		int clients = ws_server_send_text_all(out,len);
		if(clients > 0) {
			//ESP_LOGI(TAG,"sent: \"%s\" to %i clients",out,clients);
		}
		frame.data_len++;
		if (frame.data_len == 9) frame.data_len = 0;
		vTaskDelay(1000/portTICK_PERIOD_MS);
	}
}
#endif


int array2int(const cJSON * const item, int16_t* buf, int16_t buflen) {
	if (cJSON_IsArray(item)) {
		cJSON *current_element = NULL;
		int itemNumber = 0;
		cJSON_ArrayForEach(current_element, item) {
			//ESP_LOGI(TAG, "current_element->type=%s", JSON_Types(current_element->type));
			if (cJSON_IsNumber(current_element)) {
				int valueint = current_element->valueint;
				double valuedouble = current_element->valuedouble;
				ESP_LOGD(TAG, "valueint[%d]=%d valuedouble[%d]=%f",itemNumber, valueint, itemNumber, valuedouble);
				if (itemNumber < buflen) {
					buf[itemNumber] = valueint;
				}
			}
			if (cJSON_IsString(current_element)) {
				const char* string = current_element->valuestring;
				ESP_LOGD(TAG, "string[%d]=%s",itemNumber, string);
			}
			itemNumber++;
		}
		return cJSON_GetArraySize(item);
	} else {
		return 0;
	}
}

void twai_task(void *pvParameters);

void app_main()
{
	//Initialize NVS
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	// Initialize WiFi
	ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
	wifi_init_sta();

	// Initialize mDNS
	initialise_mdns();

	// Mount SPIFFS
	char *partition_label = "storage";
	char *base_path = "/spiffs"; 
	ret = mountSPIFFS(partition_label, base_path);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "mountSPIFFS fail");
		while(1) { vTaskDelay(1); }
	}

	// Create Message Buffer
	xMessageBufferMain = xMessageBufferCreate(1024);
	configASSERT( xMessageBufferMain );

	// build publish table
	ret = build_table(&publish, "/spiffs/can2http.csv", &npublish);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "build publish table fail");
		while(1) { vTaskDelay(1); }
	}
	dump_table(publish, npublish);

	/* Get the local IP address */
	tcpip_adapter_ip_info_t ip_info;
	ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));
	char cparam0[64];
	sprintf(cparam0, "%s", ip4addr_ntoa(&ip_info.ip));

	ws_server_start();
	xTaskCreate(&server_task, "server_task", 1024*2, (void *)cparam0, 9, NULL);
	xTaskCreate(&server_handle_task, "server_handle_task", 1024*3, NULL, 6, NULL);
#if defined __DEBUG__
	xTaskCreate(&time_task, "time_task", 1024*4, NULL, 2, NULL);
#endif
	xTaskCreate(&twai_task, "twai_task", 1024*6, NULL, 2, NULL);

	char cRxBuffer[512];
	int format = 0;
	bool paused = false;

	while(1) {
		size_t readBytes = xMessageBufferReceive(xMessageBufferMain, cRxBuffer, sizeof(cRxBuffer), portMAX_DELAY );
		ESP_LOGI(TAG, "readBytes=%d cRxBuffer=[%.*s]", readBytes, readBytes, cRxBuffer);
		cJSON *root = cJSON_Parse(cRxBuffer);
		if (cJSON_GetObjectItem(root, "id")) {
			char *id = cJSON_GetObjectItem(root,"id")->valuestring;
			ESP_LOGI(TAG, "id=%s",id);
			int len;
			char out[128];

			if ( strcmp (id, "init") == 0) {
#if 1
				len = makeSendText(out, "BUTTON", "pauseBtn", "visibility", "visible");
				ESP_LOGI(TAG, "len=%d", len);
				ws_server_send_text_all(out,len);
				len = makeSendText(out, "BUTTON", "resumeBtn", "visibility", "hidden");
				ESP_LOGI(TAG, "len=%d", len);
				ws_server_send_text_all(out,len);
#else
				len = makeSendText(out, "BUTTON", "pauseBtn", "display", "block");
				ws_server_send_text_all(out,len);
				len = makeSendText(out, "BUTTON", "resumeBtn", "display", "none");
				ws_server_send_text_all(out,len);
#endif
			} // end of init

			if ( strcmp (id, "format-binary") == 0 || strcmp (id, "init") == 0) {
				format = FORMAT_BINARY;
				len = makeSendText(out, "BUTTON", "binaryBtn", "class", "select");
				ESP_LOGI(TAG, "len=%d", len);
				ws_server_send_text_all(out,len);

				len = makeSendText(out, "BUTTON", "octalBtn", "class", "deselect");
				ESP_LOGI(TAG, "len=%d", len);
				ws_server_send_text_all(out,len);

				len = makeSendText(out, "BUTTON", "decimalBtn", "class", "deselect");
				ESP_LOGI(TAG, "len=%d", len);
				ws_server_send_text_all(out,len);

				len = makeSendText(out, "BUTTON", "hexadecimalBtn", "class", "deselect");
				ESP_LOGI(TAG, "len=%d", len);
				ws_server_send_text_all(out,len);
			} // end of format-binary

			if ( strcmp (id, "format-octal") == 0) {
				format = FORMAT_OCTAL;
				len = makeSendText(out, "BUTTON", "binaryBtn", "class", "deselect");
				ws_server_send_text_all(out,len);

				len = makeSendText(out, "BUTTON", "octalBtn", "class", "select");
				ws_server_send_text_all(out,len);

				len = makeSendText(out, "BUTTON", "decimalBtn", "class", "deselect");
				ws_server_send_text_all(out,len);

				len = makeSendText(out, "BUTTON", "hexadecimalBtn", "class", "deselect");
				ws_server_send_text_all(out,len);
			} // end of format-octal

			if ( strcmp (id, "format-decimal") == 0) {
				format = FORMAT_DECIMAL;
				len = makeSendText(out, "BUTTON", "binaryBtn", "class", "deselect");
				ws_server_send_text_all(out,len);

				len = makeSendText(out, "BUTTON", "octalBtn", "class", "deselect");
				ws_server_send_text_all(out,len);

				len = makeSendText(out, "BUTTON", "decimalBtn", "class", "select");
				ws_server_send_text_all(out,len);

				len = makeSendText(out, "BUTTON", "hexadecimalBtn", "class", "deselect");
				ws_server_send_text_all(out,len);
			} // end of format-decimal

			if ( strcmp (id, "format-hexadecimal") == 0) {
				format = FORMAT_HEXADECIMAL;
				len = makeSendText(out, "BUTTON", "binaryBtn", "class", "deselect");
				ws_server_send_text_all(out,len);

				len = makeSendText(out, "BUTTON", "octalBtn", "class", "deselect");
				ws_server_send_text_all(out,len);

				len = makeSendText(out, "BUTTON", "decimalBtn", "class", "deselect");
				ws_server_send_text_all(out,len);

				len = makeSendText(out, "BUTTON", "hexadecimalBtn", "class", "select");
				ws_server_send_text_all(out,len);
			} // end of format-hexadecimal

			if ( strcmp (id, "pause-request") == 0) {
				paused = true;
#if 1
				len = makeSendText(out, "BUTTON", "pauseBtn", "visibility", "hidden");
				ws_server_send_text_all(out,len);
				len = makeSendText(out, "BUTTON", "resumeBtn", "visibility", "visible");
				ws_server_send_text_all(out,len);
#else
				len = makeSendText(out, "BUTTON", "pauseBtn", "display", "none");
				ws_server_send_text_all(out,len);
				len = makeSendText(out, "BUTTON", "resumeBtn", "display", "block");
				ws_server_send_text_all(out,len);
#endif
			} // end of pause-request

			if ( strcmp (id, "resume-request") == 0) {
				paused = false;
#if 1
				len = makeSendText(out, "BUTTON", "pauseBtn", "visibility", "visible");
				ws_server_send_text_all(out,len);
				len = makeSendText(out, "BUTTON", "resumeBtn", "visibility", "hidden");
				ws_server_send_text_all(out,len);
#else
				len = makeSendText(out, "BUTTON", "pauseBtn", "display", "block");
				ws_server_send_text_all(out,len);
				len = makeSendText(out, "BUTTON", "resumeBtn", "display", "none");
				ws_server_send_text_all(out,len);
#endif
			} // end of resume-request

			if ( strcmp (id, "receive-data") == 0) {
				if (paused) continue;
				FRAME_t frame;
				frame.name = cJSON_GetObjectItem(root,"name")->valuestring;
				frame.name_len = cJSON_GetObjectItem(root,"name_len")->valueint;
				frame.canid = cJSON_GetObjectItem(root,"canid")->valueint;
				frame.ext = cJSON_GetObjectItem(root,"ext")->valueint;
				frame.rtr = cJSON_GetObjectItem(root,"rtr")->valueint;
				frame.data_len = cJSON_GetObjectItem(root,"data_len")->valueint;
				cJSON *intArray = cJSON_GetObjectItem(root,"data");
				array2int(intArray, frame.data, 8);
				char out[256];
				TickType_t tick = xTaskGetTickCount();
				int len = makeSendFrame(out, format, tick, frame);
				ESP_LOGI(TAG, "makeSendData len=%d", len);
				int clients = ws_server_send_text_all(out,len);
				if(clients > 0) {
					//ESP_LOGI(TAG,"sent: \"%s\" to %i clients",out,clients);
				}

			} // end of receive-data
		} // end of if

		// Delete a cJSON structure
		cJSON_Delete(root);
	} // end of while

}


// FORKED BY GOUNBEEE IN 2022
// SO THIS IS A SERVER PROGRAM USING HTTP.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_wifi.h"
#include "sys/param.h"

//#include "DHT22.h"
#include "http_server.h"
#include "sntp_time_sync.h"
#include "tasks_common.h"
#include "wifi_app.h"
#include "uart_master_wifi.h"
#include "rama_mathlink_db.h"



// ------------------------------------------------------
// Max HTTP URI Length IS IMPORTANT !!!!
// https://github.com/tonyp7/esp32-wifi-manager/issues/127
// :: THERE WAS AN ERROR BECAUSE OF LONG URI LENGTH
//    set idf.py menuconfig > Component config > HTTP Server > Max HTTP URI Length to 1024


// ------------------------------------------------------
// < SCREEN PROGRAM FOR MONITORING SERIAL COMMUNICATION >
// https://geekinc.ca/using-screen-as-a-serial-terminal-on-mac-os-x/




// ------------------------------------------------------
// < SCANNING SSID >
// https://github.com/LetsControltheController/wifi-scan/blob/master/main/main.c
// static void http_server_scan_ssid() {

// 	// configure and run the scan process in blocking mode
// 	wifi_scan_config_t scan_config = {
// 		.ssid = 0,
// 		.bssid = 0,
// 		.channel = 0,
// 	    .show_hidden = true
// 	};




// Tag used for ESP serial console messages
static const char TAG[] = "http_server";

// Wifi connect status
static int g_wifi_connect_status = NONE;

// Firmware update status
static int g_fw_update_status = OTA_UPDATE_PENDING;

// Local Time status
static bool g_is_local_time_set = false;



// CURRENT SUBJECT NUMBER (1) AND LINKED SUBJECT NUMBERS (9)  
//
// < INITIALIZING STATIC ARRAY IN C LANGUAGE >
// https://www.codegrepper.com/code-examples/c/initialize+empty+int+array+c
// 
//static int g_http_subjectLinked_result[RAMA_OTHERUNITS_MAX_IN_SINGLELINK + 1];


static http_linkedSubjects_result_t g_http_linkedSubjects_result;




// HTTP server task handle
static httpd_handle_t http_server_handle = NULL;

// HTTP server monitor task handle
static TaskHandle_t task_http_server_monitor = NULL;

// Queue handle used to manipulate the main queue of events
static QueueHandle_t http_server_monitor_queue_handle;



/**
 * ESP32 timer configuration passed to esp_timer_create.
 */
const esp_timer_create_args_t fw_update_reset_args = {
		.callback = &http_server_fw_update_reset_callback,
		.arg = NULL,
		.dispatch_method = ESP_TIMER_TASK,
		.name = "fw_update_reset"
};
esp_timer_handle_t fw_update_reset;




// Embedded files: JQuery, index.html, app.css, app.js and favicon.ico files
extern const uint8_t jquery_3_3_1_min_js_start[]	asm("_binary_jquery_3_3_1_min_js_start");
extern const uint8_t jquery_3_3_1_min_js_end[]		asm("_binary_jquery_3_3_1_min_js_end");
extern const uint8_t index_html_start[]				asm("_binary_index_html_start");
extern const uint8_t index_html_end[]				asm("_binary_index_html_end");
extern const uint8_t app_css_start[]				asm("_binary_app_css_start");
extern const uint8_t app_css_end[]					asm("_binary_app_css_end");
extern const uint8_t app_js_start[]					asm("_binary_app_js_start");
extern const uint8_t app_js_end[]					asm("_binary_app_js_end");
extern const uint8_t favicon_ico_start[]			asm("_binary_favicon_ico_start");
extern const uint8_t favicon_ico_end[]				asm("_binary_favicon_ico_end");





/**
 * Checks the g_fw_update_status and creates the fw_update_reset timer if g_fw_update_status is true.
 */
static void http_server_fw_update_reset_timer(void)
{
	if (g_fw_update_status == OTA_UPDATE_SUCCESSFUL)
	{
		ESP_LOGI(TAG, "http_server_fw_update_reset_timer: FW updated successful starting FW update reset timer");

		// Give the web page a chance to receive an acknowledge back and initialize the timer
		ESP_ERROR_CHECK(esp_timer_create(&fw_update_reset_args, &fw_update_reset));
		ESP_ERROR_CHECK(esp_timer_start_once(fw_update_reset, 8000000));
	}
	else
	{
		ESP_LOGI(TAG, "http_server_fw_update_reset_timer: FW update unsuccessful");
	}
}






// HTTP server monitor task used to track events of the HTTP server
// pvParameters parameter which can be passed to the task.
//
// < UPDATING STATUS OF WIFI CONNECTION >
// : BASICALLY IN THIS FUNCTION, WE GET MESSAGE AND SET PROPER STATUS
//
//   MESSAGE IS TYPE 'http_server_queue_message_t'
// 
static void http_server_monitor(void *parameter) {

	http_server_queue_message_t msg;

	// SETTING STATUS WITH MESSAGES WE GET
	for (;;) {


		// USING MESSAGE FROM QUEUE FOR MONITORING SERVER
		if ( xQueueReceive(http_server_monitor_queue_handle, &msg, portMAX_DELAY) ) {

			// < MESSAGES WE DEFINED >
			// 
			// HTTP_MSG_WIFI_CONNECT_INIT = 0,
			// HTTP_MSG_WIFI_CONNECT_SUCCESS,
			// HTTP_MSG_WIFI_CONNECT_FAIL,
			// HTTP_MSG_WIFI_USER_DISCONNECT,
			// HTTP_MSG_OTA_UPDATE_SUCCESSFUL,
			// HTTP_MSG_OTA_UPDATE_FAILED,
			// HTTP_MSG_TIME_SERVICE_INITIALIZED,
			//
			switch (msg.msgID) {


				case HTTP_MSG_WIFI_CONNECT_INIT:
					ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_INIT");
					
					// SETTING STATUS VALUE TO GLOBAL VARIABLE
					g_wifi_connect_status = HTTP_WIFI_STATUS_CONNECTING;

					break;

				case HTTP_MSG_WIFI_CONNECT_SUCCESS:
					ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_SUCCESS");

					// SETTING STATUS VALUE TO GLOBAL VARIABLE
					g_wifi_connect_status = HTTP_WIFI_STATUS_CONNECT_SUCCESS;

					break;

				case HTTP_MSG_WIFI_CONNECT_FAIL:
					ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_FAIL");

					// SETTING STATUS VALUE TO GLOBAL VARIABLE
					g_wifi_connect_status = HTTP_WIFI_STATUS_CONNECT_FAILED;

					break;

				case HTTP_MSG_WIFI_USER_DISCONNECT:
					ESP_LOGI(TAG, "HTTP_MSG_WIFI_USER_DISCONNECT");

					// SETTING STATUS VALUE TO GLOBAL VARIABLE
					g_wifi_connect_status = HTTP_WIFI_STATUS_DISCONNECTED;

					break;

				case HTTP_MSG_OTA_UPDATE_SUCCESSFUL:
					ESP_LOGI(TAG, "HTTP_MSG_OTA_UPDATE_SUCCESSFUL");
					g_fw_update_status = OTA_UPDATE_SUCCESSFUL;
					http_server_fw_update_reset_timer();

					break;

				case HTTP_MSG_OTA_UPDATE_FAILED:
					ESP_LOGI(TAG, "HTTP_MSG_OTA_UPDATE_FAILED");
					g_fw_update_status = OTA_UPDATE_FAILED;

					break;

				case HTTP_MSG_TIME_SERVICE_INITIALIZED:
					ESP_LOGI(TAG, "HTTP_MSG_TIME_SERVICE_INITIALIZED");
					g_is_local_time_set = true;

					break;


				default:
					break;
					
			}
		}
	}
}



/**
 * Jquery get handler is requested when accessing the web page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */

static esp_err_t http_server_jquery_handler(httpd_req_t *req) {

	ESP_LOGI(TAG, "Jquery requested");

	httpd_resp_set_type(req, "application/javascript");
	httpd_resp_send(req, (const char *)jquery_3_3_1_min_js_start, jquery_3_3_1_min_js_end - jquery_3_3_1_min_js_start);

	return ESP_OK;


}


// --------------------------------
// < SENDING index.html HTML FILE >
// HERE, WE SEND index.html TO URI'S REQUEST

/**
 * Sends the index.html page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_index_html_handler(httpd_req_t *req) {

	ESP_LOGI(TAG, "index.html requested");

	httpd_resp_set_type(req, "text/html");
	httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);

	return ESP_OK;

}





/**
 * app.css get handler is requested when accessing the web page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_app_css_handler(httpd_req_t *req) {

	ESP_LOGI(TAG, "app.css requested");

	httpd_resp_set_type(req, "text/css");
	httpd_resp_send(req, (const char *)app_css_start, app_css_end - app_css_start);

	return ESP_OK;

}




/**
 * app.js get handler is requested when accessing the web page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_app_js_handler(httpd_req_t *req) {

	ESP_LOGI(TAG, "app.js requested");

	httpd_resp_set_type(req, "application/javascript");
	httpd_resp_send(req, (const char *)app_js_start, app_js_end - app_js_start);

	return ESP_OK;

}




/**
 * Sends the .ico (icon) file when accessing the web page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_favicon_ico_handler(httpd_req_t *req) {

	ESP_LOGI(TAG, "favicon.ico requested");

	httpd_resp_set_type(req, "image/x-icon");
	httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_end - favicon_ico_start);

	return ESP_OK;
	
}



// < HANDLER FUNCTION FOR DISPLAYING NEW SCENE >
static esp_err_t http_server_displayScene_handler(httpd_req_t *req) {

	ESP_LOGI(TAG, "REQUESTED FROM CLIENT (WEB BROWSER) :: DISPLAYING SCENE");



	// WE ALLOW FOR THE SCENE NUMBER FOR 3-DIGITS
	// 1~999
    char content[SCENENUM_DIGIT_MAX];


    // < MAXIMUM LENGTH CHECK >
    // Truncate if content length larger than the buffer
    size_t recv_size = MIN(req->content_len, sizeof(content));



    // < RECEIVING DATA FROM REQUEST (POST) >
    //
    int ret = httpd_req_recv(req, content, recv_size);


    //printf(" ==       %d  IS RETURNED\r\n", ret);
	//printf(" ==       %s  IS THE CONTENT WE RECEIVED\r\n", content);
	//printf(" ==       %d  IS THE SIZE OF THE CONTENT\r\n", recv_size);


 	// 0 return value indicates connection closed
    if (ret <= 0) {  
        // Check if timeout occurred
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            

            // In case of timeout one can choose to retry calling
            // httpd_req_recv(), but to keep it simple, here we
            // respond with an HTTP 408 (Request Timeout) error

            httpd_resp_send_408(req);

        }

        // In case of error, returning ESP_FAIL will
        // ensure that the underlying socket is closed 
        return ESP_FAIL;
    }


	// TRIMMING UNUSED DIGITS
	content[recv_size] = '\0';


    printf("HTTP-SERVER :: BEFORE SENDING SCENE NUMBER -- TRIMMED content IS    %s   ! \r\n" , content );


	// ---------------------------------	
    // GETTING MY SSID AND UNIT'S NUMBER
    int unitNumber = wifi_get_Unit_Number();
    printf("HTTP-SERVER ::  UNIT NUMBER FROM SSID IS    %d   ! \r\n" , unitNumber );


    // ---------------------------------------------
    // < CONSTRUNCTING EXCLUDED UNIT'S INDEX ARRAY >
    int* excludedUnitNumList = wifi_get_Ex_Unit_Numbers(unitNumber);



	printf(" HTTP-SERVER ::  vvvv EXCLUDED UNIT NUMBER LIST   ::   \r\n");
	for(int i=0; i < 9; i++) {	
		printf(" | %d | ", excludedUnitNumList[i]);
	}
	printf("\r\n");



    // -------------------------------------------------
    // < CONSTRUCTING DISPLAY NUMBER AND UNIT'S NUMBER >

    // FROM NOW WE GOT THE SUBJECT NAME TO DISPLAY THE ANIMATION
    // NOW, 
    // WE NEED TO DO BELOW THINGS


    // ----------------------------------------------------------
    // 1. PLAY THE SUBJECT'S ANIMATION ON OUR OWN UNIT'S DISPLAY
    uart_wifi_SetSceneNumber(content);


    // ------------------------------------------------------------
    // 2. GETTING LINKED ANIMATION SCENE FROM INFORMATION DATABASE
	// +    
    // 3. CONTRUCTING "PLAY CHART" BY MAPPING THE UNIT'S INDEX AND ANIMATION SCENE NUMBER
    int contentInt = atoi(content) - 1;
    printf("contentInt  :::  %d \r\n", contentInt);

    // SETTING UP "FIRST ELEMENT" OF GLOBAL LINKED SUBJECTS DATA 
    g_http_linkedSubjects_result.unitNumber = contentInt;
    g_http_linkedSubjects_result.linkedCount = OTHERUNITS_MAX_IN_SINGLELINK;
    

    // SETTING OTHER SCENE NUMBERS
    // SETTING UP "REST ELEMENT" OF GLOBAL LINKED SUBJECTS DATA 
	for(int t=0; t < OTHERUNITS_MAX_IN_SINGLELINK; t++) {
		g_http_linkedSubjects_result.linkedSubjects[t] = g_rama_allMathRelations[contentInt][t];
		printf(" | %d | ", g_http_linkedSubjects_result.linkedSubjects[t] );
	}
	printf("\r\n");


	// ----------------------------
	// ABOVE WILL PRINT LIKE BELOW
	// contentInt  :::  2 
 	// | 3 |  | 4 |  | 5 |  | 6 |  | 7 |  | 8 |  | 9 |  | 10 |  | 11 | 



    // --------------------------------
    // 4. SENDING MESSAGE TO WIFI-APP

	// --------------------------------
	// WHEN SOME STATION IS CONNECTED,
	// TAKE ATTEMPT TO EASTABLISH SOCKET COMMUNICATIION



	wifi_app_send_message(WIFI_APP_MSG_SOCKET_COMMUNICATION);





    // SENDING RESPONSE TO WEB BROWSER
    const char resp[] = "URI POST Response";
    httpd_resp_set_type(req, "‘text/html");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);


    return ESP_OK;

}





// SENDING MESSAGE TO QUEUE WHICH MONITORS HTTP SERVER
BaseType_t http_server_monitor_send_message(http_server_message_e msgID) {


	http_server_queue_message_t msg;
	msg.msgID = msgID;


	return xQueueSend(http_server_monitor_queue_handle, &msg, portMAX_DELAY);

}



void http_server_fw_update_reset_callback(void *arg)
{
	ESP_LOGI(TAG, "http_server_fw_update_reset_callback: Timer timed-out, restarting the device");
	esp_restart();
}



/**
 * OTA status handler responds with the firmware update status after the OTA update is started
 * and responds with the compile time/date when the page is first requested
 * @param req HTTP request for which the uri needs to be handled
 * @return ESP_OK
 */
esp_err_t http_server_OTA_status_handler(httpd_req_t *req)
{
	char otaJSON[100];

	ESP_LOGI(TAG, "OTAstatus requested");

	sprintf(otaJSON, "{\"ota_update_status\":%d,\"compile_time\":\"%s\",\"compile_date\":\"%s\"}", g_fw_update_status, __TIME__, __DATE__);

	httpd_resp_set_type(req, "application/json");
	httpd_resp_send(req, otaJSON, strlen(otaJSON));

	return ESP_OK;
}






/**
 * Receives the .bin file fia the web page and handles the firmware update
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK, otherwise ESP_FAIL if timeout occurs and the update cannot be started.
 */
esp_err_t http_server_OTA_update_handler(httpd_req_t *req)
{
	esp_ota_handle_t ota_handle;

	char ota_buff[1024];
	int content_length = req->content_len;
	int content_received = 0;
	int recv_len;
	bool is_req_body_started = false;
	bool flash_successful = false;

	const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);

	do
	{
		// Read the data for the request
		if ((recv_len = httpd_req_recv(req, ota_buff, MIN(content_length, sizeof(ota_buff)))) < 0)
		{
			// Check if timeout occurred
			if (recv_len == HTTPD_SOCK_ERR_TIMEOUT)
			{
				ESP_LOGI(TAG, "http_server_OTA_update_handler: Socket Timeout");
				continue; ///> Retry receiving if timeout occurred
			}
			ESP_LOGI(TAG, "http_server_OTA_update_handler: OTA other Error %d", recv_len);
			return ESP_FAIL;
		}
		printf("http_server_OTA_update_handler: OTA RX: %d of %d\r", content_received, content_length);

		// Is this the first data we are receiving
		// If so, it will have the information in the header that we need.
		if (!is_req_body_started)
		{
			is_req_body_started = true;

			// Get the location of the .bin file content (remove the web form data)
			char *body_start_p = strstr(ota_buff, "\r\n\r\n") + 4;
			int body_part_len = recv_len - (body_start_p - ota_buff);

			printf("http_server_OTA_update_handler: OTA file size: %d\r\n", content_length);

			esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
			if (err != ESP_OK)
			{
				printf("http_server_OTA_update_handler: Error with OTA begin, cancelling OTA\r\n");
				return ESP_FAIL;
			}
			else
			{
				printf("http_server_OTA_update_handler: Writing to partition subtype %d at offset 0x%x\r\n", update_partition->subtype, update_partition->address);
			}

			// Write this first part of the data
			esp_ota_write(ota_handle, body_start_p, body_part_len);
			content_received += body_part_len;
		}
		else
		{
			// Write OTA data
			esp_ota_write(ota_handle, ota_buff, recv_len);
			content_received += recv_len;
		}

	} while (recv_len > 0 && content_received < content_length);

	if (esp_ota_end(ota_handle) == ESP_OK)
	{
		// Lets update the partition
		if (esp_ota_set_boot_partition(update_partition) == ESP_OK)
		{
			const esp_partition_t *boot_partition = esp_ota_get_boot_partition();
			ESP_LOGI(TAG, "http_server_OTA_update_handler: Next boot partition subtype %d at offset 0x%x", boot_partition->subtype, boot_partition->address);
			flash_successful = true;
		}
		else
		{
			ESP_LOGI(TAG, "http_server_OTA_update_handler: FLASHED ERROR!!!");
		}
	}
	else
	{
		ESP_LOGI(TAG, "http_server_OTA_update_handler: esp_ota_end ERROR!!!");
	}

	// We won't update the global variables throughout the file, so send the message about the status
	if (flash_successful) { http_server_monitor_send_message(HTTP_MSG_OTA_UPDATE_SUCCESSFUL); } else { http_server_monitor_send_message(HTTP_MSG_OTA_UPDATE_FAILED); }

	return ESP_OK;
}




/**
 * wifiConnect.json handler is invoked after the connect button is pressed
 * and handles receiving the SSID and password entered by the user
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_wifi_connect_json_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "/wifiConnect.json requested");

	size_t len_ssid = 0, len_pass = 0;
	char *ssid_str = NULL, *pass_str = NULL;

	// Get SSID header
	len_ssid = httpd_req_get_hdr_value_len(req, "my-connect-ssid") + 1;
	if (len_ssid > 1)
	{
		ssid_str = malloc(len_ssid);
		if (httpd_req_get_hdr_value_str(req, "my-connect-ssid", ssid_str, len_ssid) == ESP_OK)
		{
			ESP_LOGI(TAG, "http_server_wifi_connect_json_handler: Found header => my-connect-ssid: %s", ssid_str);
		}
	}

	// Get Password header
	len_pass = httpd_req_get_hdr_value_len(req, "my-connect-pwd") + 1;
	if (len_pass > 1)
	{
		pass_str = malloc(len_pass);
		if (httpd_req_get_hdr_value_str(req, "my-connect-pwd", pass_str, len_pass) == ESP_OK)
		{
			ESP_LOGI(TAG, "http_server_wifi_connect_json_handler: Found header => my-connect-pwd: %s", pass_str);
		}
	}

	// Update the Wifi networks configuration and let the wifi application know
	wifi_config_t* wifi_config = wifi_app_get_wifi_config();
	memset(wifi_config, 0x00, sizeof(wifi_config_t));
	memcpy(wifi_config->sta.ssid, ssid_str, len_ssid);
	memcpy(wifi_config->sta.password, pass_str, len_pass);
	wifi_app_send_message(WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER);

	free(ssid_str);
	free(pass_str);

	return ESP_OK;
}

/**
 * wifiConnectStatus handler updates the connection status for the web page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_wifi_connect_status_json_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "/wifiConnectStatus requested");

	char statusJSON[100];

	sprintf(statusJSON, "{\"wifi_connect_status\":%d}", g_wifi_connect_status);

	httpd_resp_set_type(req, "application/json");
	httpd_resp_send(req, statusJSON, strlen(statusJSON));

	return ESP_OK;
}

/**
 * wifiConnectInfo.json handler updates the web page with connection information.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_get_wifi_connect_info_json_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "/wifiConnectInfo.json requested");

	char ipInfoJSON[200];
	memset(ipInfoJSON, 0, sizeof(ipInfoJSON));

	char ip[IP4ADDR_STRLEN_MAX];
	char netmask[IP4ADDR_STRLEN_MAX];
	char gw[IP4ADDR_STRLEN_MAX];

	if (g_wifi_connect_status == HTTP_WIFI_STATUS_CONNECT_SUCCESS)
	{
		wifi_ap_record_t wifi_data;
		ESP_ERROR_CHECK(esp_wifi_sta_get_ap_info(&wifi_data));
		char *ssid = (char*)wifi_data.ssid;

		esp_netif_ip_info_t ip_info;
		ESP_ERROR_CHECK(esp_netif_get_ip_info(esp_netif_sta, &ip_info));
		esp_ip4addr_ntoa(&ip_info.ip, ip, IP4ADDR_STRLEN_MAX);
		esp_ip4addr_ntoa(&ip_info.netmask, netmask, IP4ADDR_STRLEN_MAX);
		esp_ip4addr_ntoa(&ip_info.gw, gw, IP4ADDR_STRLEN_MAX);

		sprintf(ipInfoJSON, "{\"ip\":\"%s\",\"netmask\":\"%s\",\"gw\":\"%s\",\"ap\":\"%s\"}", ip, netmask, gw, ssid);
	}

	httpd_resp_set_type(req, "application/json");
	httpd_resp_send(req, ipInfoJSON, strlen(ipInfoJSON));

	return ESP_OK;
}

/**
 * wifiDisconnect.json handler responds by sending a message to the Wifi application to disconnect.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_wifi_disconnect_json_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "wifiDisconect.json requested");

	wifi_app_send_message(WIFI_APP_MSG_USER_REQUESTED_STA_DISCONNECT);

	return ESP_OK;
}

/**
 * localTime.json handler responds by sending the local time.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_get_local_time_json_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "/localTime.json requested");

	char localTimeJSON[100] = {0};

	if (g_is_local_time_set)
	{
		sprintf(localTimeJSON, "{\"time\":\"%s\"}", sntp_time_sync_get_time());
	}

	httpd_resp_set_type(req, "application/json");
	httpd_resp_send(req, localTimeJSON, strlen(localTimeJSON));

	return ESP_OK;
}

/**
 * apSSID.json handler responds by sending the AP SSID.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_get_ap_ssid_json_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "/apSSID.json requested");

	char ssidJSON[50];

	wifi_config_t *wifi_config = wifi_app_get_wifi_config();
	esp_wifi_get_config(ESP_IF_WIFI_AP, wifi_config);
	char *ssid = (char*)wifi_config->ap.ssid;

	sprintf(ssidJSON, "{\"ssid\":\"%s\"}", ssid);

	httpd_resp_set_type(req, "application/json");
	httpd_resp_send(req, ssidJSON, strlen(ssidJSON));

	return ESP_OK;
}





/**
 * Sets up the default httpd server configuration.
 * @return http server instance handle if successful, NULL otherwise.
 */
static httpd_handle_t http_server_configure(void) {


	// Generate the default configuration
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();


	// Create HTTP server monitor task
	xTaskCreatePinnedToCore(&http_server_monitor, "http_server_monitor", HTTP_SERVER_MONITOR_STACK_SIZE, NULL, HTTP_SERVER_MONITOR_PRIORITY, &task_http_server_monitor, HTTP_SERVER_MONITOR_CORE_ID);


	// Create the message queue
	
	// TODO :: 3 MESSAGES IS ENOUGH ???

	http_server_monitor_queue_handle = xQueueCreate(3, sizeof(http_server_queue_message_t));


	// The core that the HTTP server will run on
	config.core_id = HTTP_SERVER_TASK_CORE_ID;


	// Adjust the default priority to 1 less than the wifi application task
	config.task_priority = HTTP_SERVER_TASK_PRIORITY;


	// Bump up the stack size (default is 4096)
	config.stack_size = HTTP_SERVER_TASK_STACK_SIZE;


	// Increase uri handlers
	config.max_uri_handlers = 20;

	// Increase the timeout limits
	// DEFAULT VALUE WAS 10
	config.recv_wait_timeout = 10;
	config.send_wait_timeout = 10;


	ESP_LOGI(TAG,
			"http_server_configure: Starting server on port: '%d' with task priority: '%d'",
			config.server_port,
			config.task_priority);



	
	// STARTING THE HTTPD SERVER
	// 
	if ( httpd_start(&http_server_handle, &config) == ESP_OK ) {

		ESP_LOGI(TAG, "http_server_configure: Registering URI handlers");


		// ----------------------------------------------------------------------------
		// < ACTUALLY, BELOWS ARE REGISTRATION OF CONTROLLER FUNCTIONS IN MVC MODEL ! >
		// ----------------------------------------------------------------------------


		// ------------------------------------------
		// < DEFINING URI STRUCTURES >
		//
		// register Jquery handler
		// 
		// DEFINING URI STRUCTURE (FOR JQUERY FILE)
		httpd_uri_t jquery_js = {
				.uri = "/jquery-3.3.1.min.js",					// URI ROUTING
				.method = HTTP_GET,								// REQUEST METHOD
				.handler = http_server_jquery_handler,			// HANDLER FUNCTION (CONTROLLER IN MVC)
																// WHEN ABOVE URI IS ACCESED, WE CALL THIS HANDLER FUNCTION
				.user_ctx = NULL
		};

		// < httpd_register_uri_handler FUNCTION >
		// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_http_server.html#_CPPv426httpd_register_uri_handler14httpd_handle_tPK11httpd_uri_t
		// : Registers a URI handler.
		//
		// < REGISTER ABOVE SETUP >
		httpd_register_uri_handler(http_server_handle, &jquery_js);




		// ------------------------------------------
		// < REGISTERING HANDLER FUNCTION FOR GETTING index.html FILE > 
		// 
		// register index.html handler
		httpd_uri_t index_html = {
				.uri = "/",										// URI ROUTING
				.method = HTTP_GET,								// METHOD OF REQUEST
				.handler = http_server_index_html_handler,		// HANDLER FUNCTION (CONTROLLER IN MVC)
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &index_html);



		// register app.css handler
		httpd_uri_t app_css = {
				.uri = "/app.css",
				.method = HTTP_GET,
				.handler = http_server_app_css_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &app_css);


		// register app.js handler
		httpd_uri_t app_js = {
				.uri = "/app.js",
				.method = HTTP_GET,
				.handler = http_server_app_js_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &app_js);


		// register favicon.ico handler
		httpd_uri_t favicon_ico = {
				.uri = "/favicon.ico",
				.method = HTTP_GET,
				.handler = http_server_favicon_ico_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &favicon_ico);


		// < REGISTER DISPLAY SCENE HANDLER WITH WEB BROWSER >
		httpd_uri_t displayScene = {
				.uri = "/displayScene",
				.method = HTTP_POST,
				.handler = http_server_displayScene_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &displayScene);


		// register apSSID.json handler
		httpd_uri_t ap_ssid_json = {
				.uri = "/apSSID.json",
				.method = HTTP_GET,
				.handler = http_server_get_ap_ssid_json_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &ap_ssid_json);


		// // < REGISTER HANDLER TO BROADCAST SCENE HANDLER >
		// httpd_uri_t get_sceneNumber = {
		// 		.uri = "/get_scene",
		// 		.method = HTTP_GET,
		// 		.handler = http_server_get_scene_handler,
		// 		.user_ctx = NULL
		// };
		// httpd_register_uri_handler(http_server_handle, &get_sceneNumber);


		// register OTAupdate handler
		httpd_uri_t OTA_update = {
				.uri = "/OTAupdate",
				.method = HTTP_POST,
				.handler = http_server_OTA_update_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &OTA_update);

		// register OTAstatus handler
		httpd_uri_t OTA_status = {
				.uri = "/OTAstatus",
				.method = HTTP_POST,
				.handler = http_server_OTA_status_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &OTA_status);



		// register wifiConnect.json handler
		httpd_uri_t wifi_connect_json = {
				.uri = "/wifiConnect.json",
				.method = HTTP_POST,
				.handler = http_server_wifi_connect_json_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &wifi_connect_json);

		// register wifiConnectStatus.json handler
		httpd_uri_t wifi_connect_status_json = {
				.uri = "/wifiConnectStatus",
				.method = HTTP_POST,
				.handler = http_server_wifi_connect_status_json_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &wifi_connect_status_json);

		// register wifiConnectInfo.json handler
		httpd_uri_t wifi_connect_info_json = {
				.uri = "/wifiConnectInfo.json",
				.method = HTTP_GET,
				.handler = http_server_get_wifi_connect_info_json_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &wifi_connect_info_json);

		// register wifiDisconnect.json handler
		httpd_uri_t wifi_disconnect_json = {
				.uri = "/wifiDisconnect.json",
				.method = HTTP_DELETE,
				.handler = http_server_wifi_disconnect_json_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &wifi_disconnect_json);

		// register localTime.json handler
		httpd_uri_t local_time_json = {
				.uri = "/localTime.json",
				.method = HTTP_GET,
				.handler = http_server_get_local_time_json_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &local_time_json);





		return http_server_handle;



	}

	return NULL;

}





http_linkedSubjects_result_t http_server_getLinkedSubjectsResult() {


	return g_http_linkedSubjects_result;


}


// ----------------------------
// < STARTING HTTP SERVER >
// : STARTING SERVER FUNCTION SIMPLY RUN ABOVE FUNCTION
// 
void http_server_start(void) {

	if (http_server_handle == NULL) {

		// ------------------------------------
		// SETUP HTTP SERVER AND GETTING HANDLE
		http_server_handle = http_server_configure();


	}

}


// STOP HTTPS SERVER
void http_server_stop(void) {

	if (http_server_handle) {

		httpd_stop(http_server_handle);
		ESP_LOGI(TAG, "http_server_stop: stopping HTTP server");
		http_server_handle = NULL;

	}

	if (task_http_server_monitor) {

		vTaskDelete(task_http_server_monitor);
		ESP_LOGI(TAG, "http_server_stop: stopping HTTP server monitor");
		task_http_server_monitor = NULL;

	}

}


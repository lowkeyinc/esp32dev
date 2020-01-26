#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#include "ble_hidd.c"

#define LED_GPIO 32
#define BUTTON_GPIO 5

static bool led_state = false;

void bluetooth_task(void *pvParameters){
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	while(1) {
		vTaskDelay(2000 / portTICK_PERIOD_MS);
		if(led_state){
			ESP_LOGI(BLE_HID_LOG_NAME, "Send the letter");
			// if (no keys in buffer) sleep until timer.
			uint8_t key_value = {HID_KEY_A}; // atomic load/pop next key
			esp_hidd_send_keyboard_value(hid_conn_id, 0, &key_value, 1);
			esp_hidd_send_keyboard_value(hid_conn_id, 0, &key_value, 0);
		}
		/*if (sec_conn) {
			//ESP_LOGI(BLE_HID_LOG_NAME, "Send the volume");
			//send_volum_up = true;
			esp_hidd_send_consumer_value(hid_conn_id, HID_CONSUMER_VOLUME_UP, true);
			vTaskDelay(3000 / portTICK_PERIOD_MS);
			if (send_volum_up) {
				send_volum_up = false;
				esp_hidd_send_consumer_value(hid_conn_id, HID_CONSUMER_VOLUME_UP, false);
				esp_hidd_send_consumer_value(hid_conn_id, HID_CONSUMER_VOLUME_DOWN, true);
				vTaskDelay(3000 / portTICK_PERIOD_MS);
				esp_hidd_send_consumer_value(hid_conn_id, HID_CONSUMER_VOLUME_DOWN, false);
			}
			}*/
	}
}

void setup_ble_hidd(){
	esp_err_t ret;

	// Initialize NVS.
	ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK( ret );
    
	ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    
	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
	ret = esp_bt_controller_init(&bt_cfg);
	if (ret) {
		ESP_LOGE(BLE_HID_LOG_NAME, "%s initialize controller failed\n", __func__);
		return;
	}
    
	ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
	if (ret) {
		ESP_LOGE(BLE_HID_LOG_NAME, "%s enable controller failed\n", __func__);
		return;
	}
    
	ret = esp_bluedroid_init();
	if (ret) {
		ESP_LOGE(BLE_HID_LOG_NAME, "%s init bluedroid failed\n", __func__);
		return;
	}
    
	ret = esp_bluedroid_enable();
	if (ret) {
		ESP_LOGE(BLE_HID_LOG_NAME, "%s init bluedroid failed\n", __func__);
		return;
	}
    
	if((ret = esp_hidd_profile_init()) != ESP_OK) {
		ESP_LOGE(BLE_HID_LOG_NAME, "%s init bluedroid failed\n", __func__);
	}

	///register the callback function to the gap module
	esp_ble_gap_register_callback(gap_event_handler);
	esp_hidd_register_callbacks(hidd_event_callback);

	/* set the security iocap & auth_req & key size & init key response key parameters to the stack*/
	esp_ble_auth_req_t auth_req = ESP_LE_AUTH_BOND;     //bonding with peer device after authentication
	esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;           //set the IO capability to No output No input
	uint8_t key_size = 16;      //the key size should be 7~16 bytes
	uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
	uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
	esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
	esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
	esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
	/* If your BLE device act as a Slave, the init_key means you hope which types of key of the master should distribute to you,
	   and the response key means which key you can distribute to the Master;
	   If your BLE device act as a master, the response key means you hope which types of key of the slave should distribute to you, 
	   and the init key means which key you can distribute to the slave. */
	esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
	esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

	xTaskCreate(&bluetooth_task, "hid_task", 2048, NULL, 5, NULL);
}

void app_main(void){
	/* Configure the IOMUX register for pad LED_GPIO (some pads are
	   muxed to GPIO on reset already, but some default to other
	   functions and need to be switched to GPIO. Consult the
	   Technical Reference for a list of pads and their default
	   functions.)
	*/
	gpio_pad_select_gpio(LED_GPIO);
	gpio_pad_select_gpio(BUTTON_GPIO);
	gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
	gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);

	setup_ble_hidd();

	// Main loop
	bool pressed=false;
	gpio_set_level(LED_GPIO, led_state);
	while(1) {
		pressed=!gpio_get_level(BUTTON_GPIO);
		if(led_state!=pressed){
			printf("Turning %s the LED\n",pressed?"on":"off");
			gpio_set_level(LED_GPIO, (led_state=pressed));
		}
		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
}

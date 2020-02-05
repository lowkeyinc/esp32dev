// -*- mode: org -*-
// clangformat off
/*
* Prelude - File Format
  This file is both a valid org-mode file and a valid c file.  As such
  there is some strangeness to deal with clangformat's and org's
  opions on indenting differing.
* Include Statements
** Libraries
  - ~stdio.h~
    
    This library is for debug statements, only printf is used from
    here.

    TODO: Can that be disabled in non-debug builds?

  - ~driver/gpio.h~

  - ~freertos/FreeRTOS.h~

  - ~freertos/queue.h~

    An interrupt and thread safe queue for passing commands from the
    GPIO handling code to the USB and Bluetooth managers.

  - ~freertos/task.h~

  - ~sdkconfig.h~
*** Code
    #+BEGIN_SRC c
#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdkconfig.h"
    #+END_SRC
** Project Local
  - ~bl_hidd.c~
*** Code
    #+BEGIN_SRC c
 */
#include "ble_hidd.c"
/*
    #+END_SRC
* Design
** Requirements
   1. All normal keyboard features.
      1. Send all normal key codes.
      2. Send keys with status codes.
      3. Press and hold keys.
      4. Status lights
	 1. Caps Lock
	 2. Scroll Lock
	 3. Number Lock
   2. ErgoDox Features
      1. Key Layers
      2. Read Config File Formats
** Thinking about impossible interactions and requirements.
   Some combinations of interactions are impossible given how USB-HID
   and PS2 work.  Such an interaction is holding down 'a' at the same
   time as 'B' since 'B' requires shift and 'a' requires not shift.

   Here is a list of features:
   1) Holding down keys. (for games and application rep-keys)
   2) Custome rep-key times (a key code is repetitively sent while the
      button is held)
   3) Custom char per key
   4) Unicode chars

   And the interactions between these features:
   1,2 Incompatible
   1,3 Incompatible if keys are not in the same shit layers as default
   1,4 Incompatible
   2,3 Compatible
   2,4 Compatible
   3,4 Compatible

   Well, that's a clean divide.  When specifying a mode we will then
   set if we are using feature (1) or features (2,3,4) as a toggle on
   that mode.

   What are the implications of this?

   We want keys presses to be fast while in a mode.  We can discard
   the above "keeping layer keys between modes" idea since modes are
   now a larger and more important idea and the user will understand
   the hard swap.

   When a mode switch occurs, perform all key release actions and
   ignore those keys until they are released.  This ignoring already
   occurs in the above layout.

   The rep-keys feature isn't really implementable in the above
   system.  The key press and key release functions can add and remove
   a char(s) from the ~rep_buffer~ and a timed daemon (like the
   Bluetooth one) can keep re-adding the keys every unit time.
** State Machine Time
   When a key is pressed it can either send a (or more) key press to
   the computer or change the layer mode.  When a key is released it
   will either send a key release to the computer or revert the mode.

   This doesn't allow for persistent mode changes.

   Use Cases:
   1. Tap 'a' -> Send 'a' Key
   2. Press Shift, Tap 'a' -> Send 'A'
   3. Press Shift, Tap 'a'. Tap 'b' -> Send 'AB'
   4. Tap Caps-lock -> Switch to "Shift on by Default" Mode
   5. Press Hyper, Tap ESC -> Switch to IPA Mode (or APL Mode, or
   something)

   We might also want access to adjusting the pairing settings from
   the key interface, this would require arbitrary function calling.

   Storing a structure of function pointers would allow for arbitrary
   function calls.

   There exists modes, layers, and functions. When the first key is
   pressed it can:
   1) change the mode
   2) add a layer to further key presses
   3) call a function and register a (possibly null) release function

   The action performed by a key is looked up by: 1, the mode; 2, the
   layer stack.  A key is either a layer key or a function key in any
   given mode.

   Example Flow:
   1) A key is depressed.
   2) it is looked up if the key is a function or a layer
      1) it's a layer key
         - A layer is added onto the stack and registered to the key
           release
      2) it's a function key
         - if it's a mode key the mode is changed and the layer stack
           is dropped
         - A function is called and a release function is registered.

   1) A key is released
   - a function was registered to it
     The function is called
   - a layer was registered to it
     - If the mode has changed nothing happens
     - else the layer is removed from the layer sack

   The layer stack is an ordered stack and is used in lookup up a
   key's function(s). In most cases the order will not matter.

   If CTRL is pressed and then a mode change occurs, then the user
   will expect pressed keys to still send the CTRL versions.

   - If a layer key is pressed during a mode switch then they layer
     should also be active in the new mode.
* Unsorted Code
  #+BEGIN_SRC c
 */
// clangformat on
#define LED_GPIO 32
#define BUTTON_GPIO 5

static bool led_state = false;
typedef struct {	bool down;
	                uint8_t key;} key_press;
static QueueHandle_t key_buffer;

#define ifwhile(COND) if(COND)while(COND)

void bluetooth_task(void *pvParameters){
	#define LOG_NAME "ble_key_buffer_reader"
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	key_press key_value;
	while(1) {
		vTaskDelay(100 / portTICK_PERIOD_MS);
		// TODO: if(!connected){sleep;continue;}
		ifwhile(uxQueueMessagesWaiting(key_buffer)){
			ESP_LOGI(LOG_NAME, "Send the letter");
			xQueueReceive(key_buffer,&key_value,0/*no wait*/);
			esp_hidd_send_keyboard_value(hid_conn_id, 0, &key_value.key, key_value.down);
			vTaskDelay(1); // for interupts
		}else{
			ESP_LOGI(LOG_NAME, "No Letters, sleeping for a while");
			vTaskDelay(1000/portTICK_PERIOD_MS);
			// TODO: Go into low power sleep for the keys to wake up from.
		}
	}
	#undef LOG_NAME
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


#define NO_KEY 0
#define          NO_MODIFIER 0b00000000
#define   CTRL_LEFT_MODIFIER 0b00000001
#define  SHIFT_LEFT_MODIFIER 0b00000010
#define    ALT_LEFT_MODIFIER 0b00000100
#define    GUI_LEFT_MODIFIER 0b00001000
#define  CTRL_RIGHT_MODIFIER 0b00010000
#define SIHFT_RIGHT_MODIFIER 0b00100000
#define   ALT_RIGHT_MODIFIER 0b01000000
#define   GUI_RIGHT_MODIFIER 0b10000000

#define LSHIFT_MOD SHIFT_LEFT_MODIFIER

const struct KeyEvent{
	uint8_t key;
	uint8_t modifier;
} ASCII_TO_HID[] = {
	{NO_KEY, NO_MODIFIER}, /* 00 NUL                           */
	{NO_KEY, NO_MODIFIER}, /* 01 SOH Start of Heading          */
	{NO_KEY, NO_MODIFIER}, /* 02 STX Start of Text             */
	{NO_KEY, NO_MODIFIER}, /* 03 ETX End of Text               */
	{NO_KEY, NO_MODIFIER}, /* 04 EOT End  of Transmission      */
	{NO_KEY, NO_MODIFIER}, /* 05 ENQ Enquiry                   */
	{NO_KEY, NO_MODIFIER}, /* 06 ACK Acknowledgement           */
	{NO_KEY, NO_MODIFIER}, /* 07 BEL Bell                      */
	{    42, NO_MODIFIER}, /* 08 BS  Backspace                 */
	{    43, NO_MODIFIER}, /* 09 TAB Horizontal Tab            */
	{    40,  LSHIFT_MOD}, /* 0A LF  Line Feed                 */
	{NO_KEY, NO_MODIFIER}, /* 0B VT  Vertical Tab              */
	{NO_KEY, NO_MODIFIER}, /* 0C FF  Form Feed                 */
	{    40, NO_MODIFIER}, /* 0D CR  Carriage Return           */
	{NO_KEY, NO_MODIFIER}, /* 0E SO  Shift Out                 */
	{NO_KEY, NO_MODIFIER}, /* 0F ST  Shift In                  */
	{NO_KEY, NO_MODIFIER}, /* 10 DLE Data Link Escape          */
	{NO_KEY, NO_MODIFIER}, /* 11 DC1 Device Control 1          */
	{NO_KEY, NO_MODIFIER}, /* 12 DC2 Device Control 2          */
	{NO_KEY, NO_MODIFIER}, /* 13 DC3 Device Control 3          */
	{NO_KEY, NO_MODIFIER}, /* 14 DC4 Device Control 4          */
	{NO_KEY, NO_MODIFIER}, /* 15 NAK Negative Acknowledgement  */
	{NO_KEY, NO_MODIFIER}, /* 16 SYN Synchronous Idle          */
	{NO_KEY, NO_MODIFIER}, /* 17 ETB End of Transmission Block */
	{NO_KEY, NO_MODIFIER}, /* 18 CAN Cancel                    */
	{NO_KEY, NO_MODIFIER}, /* 19 EM  End of Medium             */
	{NO_KEY, NO_MODIFIER}, /* 1A SUB Substitute                */
	{    41, NO_MODIFIER}, /* 1B ESC Escape                    */
	{NO_KEY, NO_MODIFIER}, /* 1C FS  File Separator            */
	{NO_KEY, NO_MODIFIER}, /* 1D GS  Group Separator           */
	{NO_KEY, NO_MODIFIER}, /* 1E RS  Record Separator          */
	{NO_KEY, NO_MODIFIER}, /* 1F US  Unit Separator            */
	{    44, NO_MODIFIER}, /* 20 Space                         */
	{    30,  LSHIFT_MOD}, /* 21 !                             */
	{    52,  LSHIFT_MOD}, /* 22 "                             */
	{    32,  LSHIFT_MOD}, /* 23 #                             */
	{    33,  LSHIFT_MOD}, /* 24 $                             */
	{    34,  LSHIFT_MOD}, /* 25 %                             */
	{    36,  LSHIFT_MOD}, /* 26 &                             */
	{    52, NO_MODIFIER}, /* 27 '                             */
	{    38,  LSHIFT_MOD}, /* 28 (                             */
	{    39,  LSHIFT_MOD}, /* 29 )                             */
	{    37,  LSHIFT_MOD}, /* 2A *                             */
	{    46,  LSHIFT_MOD}, /* 2B +                             */
	{    54, NO_MODIFIER}, /* 2C ,                             */
	{    45, NO_MODIFIER}, /* 2D -                             */
	{    55, NO_MODIFIER}, /* 2E .                             */
	{    56, NO_MODIFIER}, /* 2F /                             */
	{    39, NO_MODIFIER}, /* 30 0                             */
	{    30, NO_MODIFIER}, /* 31 1                             */
	{    31, NO_MODIFIER}, /* 32 2                             */
	{    32, NO_MODIFIER}, /* 33 3                             */
	{    33, NO_MODIFIER}, /* 34 4                             */
	{    34, NO_MODIFIER}, /* 35 5                             */
	{    35, NO_MODIFIER}, /* 36 6                             */
	{    36, NO_MODIFIER}, /* 37 7                             */
	{    37, NO_MODIFIER}, /* 38 8                             */
	{    38, NO_MODIFIER}, /* 39 9                             */
	{    51,  LSHIFT_MOD}, /* 3A :                             */
	{    51, NO_MODIFIER}, /* 3B ;                             */
	{    54,  LSHIFT_MOD}, /* 3C <                             */
	{    46, NO_MODIFIER}, /* 3D =                             */
	{    54,  LSHIFT_MOD}, /* 3E >                             */
	{    56,  LSHIFT_MOD}, /* 3F ?                             */
	{    31,  LSHIFT_MOD}, /* 40 @                             */
	{     4,  LSHIFT_MOD}, /* 41 A                             */
	{     5,  LSHIFT_MOD}, /* 42 B                             */
	{     6,  LSHIFT_MOD}, /* 43 C                             */
	{     7,  LSHIFT_MOD}, /* 44 D                             */
	{     8,  LSHIFT_MOD}, /* 45 E                             */
	{     9,  LSHIFT_MOD}, /* 46 F                             */
	{    10,  LSHIFT_MOD}, /* 47 G                             */
	{    11,  LSHIFT_MOD}, /* 48 H                             */
	{    12,  LSHIFT_MOD}, /* 49 I                             */
	{    13,  LSHIFT_MOD}, /* 4A J                             */
	{    14,  LSHIFT_MOD}, /* 4B K                             */
	{    15,  LSHIFT_MOD}, /* 4C L                             */
	{    16,  LSHIFT_MOD}, /* 4D M                             */
	{    17,  LSHIFT_MOD}, /* 4E N                             */
	{    18,  LSHIFT_MOD}, /* 4F O                             */
	{    19,  LSHIFT_MOD}, /* 50 P                             */
	{    20,  LSHIFT_MOD}, /* 51 Q                             */
	{    21,  LSHIFT_MOD}, /* 52 R                             */
	{    22,  LSHIFT_MOD}, /* 53 S                             */
	{    23,  LSHIFT_MOD}, /* 54 T                             */
	{    24,  LSHIFT_MOD}, /* 55 U                             */
	{    25,  LSHIFT_MOD}, /* 56 V                             */
	{    26,  LSHIFT_MOD}, /* 57 W                             */
	{    27,  LSHIFT_MOD}, /* 58 X                             */
	{    28,  LSHIFT_MOD}, /* 59 Y                             */
	{    29,  LSHIFT_MOD}, /* 5A Z                             */
	{    47, NO_MODIFIER}, /* 5B [                             */
	{    49, NO_MODIFIER}, /* 5C \                             */
	{    48, NO_MODIFIER}, /* 5D ]                             */
	{    35,  LSHIFT_MOD}, /* 5E ^                             */
	{    45,  LSHIFT_MOD}, /* 5F _                             */
	{    53, NO_MODIFIER}, /* 60 `                             */
	{     4, NO_MODIFIER}, /* 61 a                             */
	{     5, NO_MODIFIER}, /* 62 b                             */
	{     6, NO_MODIFIER}, /* 63 c                             */
	{     7, NO_MODIFIER}, /* 64 d                             */
	{     8, NO_MODIFIER}, /* 65 e                             */
	{     9, NO_MODIFIER}, /* 66 f                             */
	{    10, NO_MODIFIER}, /* 67 g                             */
	{    11, NO_MODIFIER}, /* 68 h                             */
	{    12, NO_MODIFIER}, /* 69 i                             */
	{    13, NO_MODIFIER}, /* 6A j                             */
	{    14, NO_MODIFIER}, /* 6B k                             */
	{    15, NO_MODIFIER}, /* 6C l                             */
	{    16, NO_MODIFIER}, /* 6D m                             */
	{    17, NO_MODIFIER}, /* 6E n                             */
	{    18, NO_MODIFIER}, /* 6F o                             */
	{    19, NO_MODIFIER}, /* 70 p                             */
	{    20, NO_MODIFIER}, /* 71 q                             */
	{    21, NO_MODIFIER}, /* 72 r                             */
	{    22, NO_MODIFIER}, /* 73 s                             */
	{    23, NO_MODIFIER}, /* 74 t                             */
	{    24, NO_MODIFIER}, /* 75 u                             */
	{    25, NO_MODIFIER}, /* 76 v                             */
	{    26, NO_MODIFIER}, /* 77 w                             */
	{    27, NO_MODIFIER}, /* 78 x                             */
	{    28, NO_MODIFIER}, /* 79 y                             */
	{    29, NO_MODIFIER}, /* 7A z                             */
	{    47,  LSHIFT_MOD}, /* 7B {                             */
	{    49,  LSHIFT_MOD}, /* 7C |                             */
	{    48,  LSHIFT_MOD}, /* 7D }                             */
	{    53,  LSHIFT_MOD}, /* 7E ~                             */
	{    76, NO_MODIFIER}  /* 7F DEL                           */
};

bool send_char(unicode_char c,unsigned char modifiers=0){
	KeyEvent k={NO_KEY,NO_MODIFIER};
	if(c<=0x7F) k=ASCII_TO_HID[c];

	if(k.key!=NO_KEY){
		// send the key
	}else{
		// send it as Unicode
		/*
		 * Sending Unicode
		 *
		 * Source:
		 * https://github.com/duckythescientist/BinaryKeyboard.git
		 *
		 * * Windows
		 *
		 *   1) Press ALT
		 *   2) Send numpad_plus
		 *   3) Type hexadecimal value
		 *   4) Release ALT
		 *
		 *   Note: ~HKEY_CURRENT_USER\Control Panel\Input Method~
		 *   contains a string type (REG_SZ) value called
		 *   ~EnableHexNumpad~, which must have the value ~1~.
		 *
		 *   Note: Windows seems to only type 2-byte symbols.  There
		 *   are other methods that may have more symbols, but this is
		 *   the more universal method.
		 *
		 * * Linux (and hopefully macs)
		 *
		 *   1) Send CTRL+ALT+SHIFT+U
		 *   2) Type hexadecimal (no leading zeros)
		 *   3) Send space
		 *
		 *   Note: Some applications need the ALT, and some don't. It
		 *   seems to work on both if it is held.
		 *
		 *   Note: this may be X specific or application specific.
		 *   I've had success in Ubuntu 14.04 with Unity (Chrome,
		 *   Guake, Gedit, Sublime Text 3).
		 */
	}
}

#define BUTTON_ROW_COUNT 6
#define BUTTON_COLUMN_COUNT 14
typedef uint16_t ButtonRow;
struct Buttons{
	ButtonRow rows[BUTTON_ROW_COUNT];
};

buttons get_pressed_keys(){
	Buttons ret;
	for(int i=0;i<BUTTON_ROW_COUNT;i++)
		;// ret.rows[i]={GPIO_reads here}
	return ret;
}

RTC_DATA_ATTR uint64_t state;
void update_state_machine(){
	static RTC_DATA_ATTR Buttons prior_state;
	Buttons state = get_pressed_keys();
	Buttons changed=state^prior_state;
	prior_state=state;
	// for each changed key
	// update the state machine
}

/**
 * Setup state that isn't reverted by deep sleeping.
 */
void initial_setup(void){
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

	// Start bluetooth worker.
	setup_ble_hidd();
}

void /* No Return */ sleep_until_keypress(){
	// set each key polling to high so that they 
	// sleep until keypress
	// or until timeout for bluetooth keepalive
	// enter deep sleep
}

void app_main(void){
	static RTC_DATA_ATTR inited=false;
	if(!inited){
		initial_setup();
		inited=true;
	}

	// Setup global buffer.
	key_buffer = xQueueCreate(1<<7,sizeof(key_press));

	// Main loop
	bool pressed=false;
	gpio_set_level(LED_GPIO, led_state);
	bool toggel=false;
	while(1) {
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		// TODO: update state machine either here or in GPIO
		// interrupt.

		// if(keys to send)
		//   if(connected)
		//     continue;
		sleep_until_keypress();
	}
}
/*
pressed=!gpio_get_level(BUTTON_GPIO);
if(led_state!=pressed){
	printf("Turning %s the LED\n",pressed?"on":"off");
	gpio_set_level(LED_GPIO, (led_state=pressed));

	key_press k;
	if(pressed)if((toggel=!toggel)){
	printf("Sending \"Hello, world!\"");
	k.key=HID_KEY_RIGHT_SHIFT;k.down=true;
	xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);
	k.key=HID_KEY_H;k.down=true;
	xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);
	k.key=HID_KEY_H;k.down=false;
	xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);
	k.key=HID_KEY_RIGHT_SHIFT;k.down=false;

	xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);
	k.key=HID_KEY_E;k.down=true;
	xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);
	k.key=HID_KEY_E;k.down=false;
	xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);

	k.key=HID_KEY_L;k.down=true;
	xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);
	k.key=HID_KEY_L;k.down=false;
	xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);

	k.key=HID_KEY_L;k.down=true;
	xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);
	k.key=HID_KEY_L;k.down=false;
	xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);

	k.key=HID_KEY_O;k.down=true;
	xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);
	k.key=HID_KEY_O;k.down=false;
	xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);

	k.key=HID_KEY_COMMA;k.down=true;
	xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);
	k.key=HID_KEY_COMMA;k.down=false;
	xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);

	k.key=HID_KEY_SPACEBAR;k.down=true;
	xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);
	k.key=HID_KEY_SPACEBAR;k.down=false;
	xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);

	k.key=HID_KEY_W;k.down=true;
	xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);
	k.key=HID_KEY_W;k.down=false;
	xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);

	k.key=HID_KEY_O;k.down=true;
	xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);
	k.key=HID_KEY_O;k.down=false;
	xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);

	k.key=HID_KEY_R;k.down=true;
	xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);
	k.key=HID_KEY_R;k.down=false;
	xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);

	k.key=HID_KEY_L;k.down=true;
	xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);
	k.key=HID_KEY_L;k.down=false;
	xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);

	k.key=HID_KEY_D;k.down=true;
	xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);
	k.key=HID_KEY_D;k.down=false;
	xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);

	k.key=HID_KEY_LEFT_SHIFT;k.down=true;
	xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);
	k.key=HID_KEY_1;k.down=true;
	xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);
	k.key=HID_KEY_1;k.down=false;
	xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);
	k.key=HID_KEY_LEFT_SHIFT;k.down=false;
	xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);

	k.key=HID_KEY_RETURN;k.down=true;
	xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);
	k.key=HID_KEY_RETURN;k.down=false;
	xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);
}else{
	printf("Clearing \"Hello, world!\"");
	for(int i=0;i<14;i++){
		k.key=HID_KEY_DELETE;k.down=true;
		xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);
		k.key=HID_KEY_DELETE;k.down=false;
		xQueueSendToBackFromISR(key_buffer,&k,pdFALSE);
	}
}
*/
/*
 #+END_SRC
* End of File Format Corrector
//*/

// -*- mode: org -*-
// clangformat off
/*
* Prelude - File Format
  This file is both a valid org-mode file and a valid c file.  As such
  there is some strangeness to deal with clangformat's and org's
  opions on indenting differing.
* Global Configureation Macros
  #+BEGIN_SRC c
//*/
#define DEVELOPMENT_BUILD 1
/*
  #+END_SRC
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

   #+BEGIN_SRC c
//*/
#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdkconfig.h"
/*
   #+END_SRC
** Project Local
  - ~bl_hidd.c~
   #+BEGIN_SRC c
//*/
#include "ble_hidd.c"
/*
   #+END_SRC
* Design
** Requirements
*** High level
    1. All normal keyboard features.
       1. Send all normal key codes.
       2. Send keys with status codes.
       3. Press and hold keys.
       4. Status lights
          1. Caps Lock
	  2. Scroll Lock
          3. Number Lock
       5. Low Latency
    2. ErgoDox Features
       1. Key Layers
       2. Read Config File Formats
    3. Personal Requirements
       1. Support any Desired Key Arrangement
       2. Support all (human and programming) Languages
          1. Support being an APL keyboard
       3. Custom Rep-key times
          Added as it's easy once 3.1 is supported
       4. Doesn't interfere with other keyboards plugged-in in
          unintuitive ways.
*** Derived Requirements
    1. 3.1 & 3.2 Support sending Unicode chars
** Note on OS Language Settings
   When the language is set on an operating system key codes change
   meaning.  An extreme case is setting the language to Dvorak where
   the letter keys are remapped so that the key-code for 'q' is
   interpreted as '\''.

   The costs of Supporting the language being changed:
   1. Lots of maintenance
   2. Large conversion tables for some languages

   The benefits of supporting this are:
   1. The ability to send language specific chars in [[*Gaming Modes][~Gaming~ mode]].
   2. Better interactions with other keyboards.

   As the costs greatly out-way the benefits this is not supported.

   NOTE: Should this feature become greatly requested, a compile time
   conversion could be added to add partial support for this.
** Interactions of Requirements and Protocols
   Some combinations of interactions are impossible given how USB-HID
   and PS2 work.  Example: Holding down 'a' at the same time as 'B'
   since 'B' requires shift and 'a' requires not shift.

   The requirements that are in the most direct conflict are ~1.3~ and
   ~3.1~ since ~3.1~ allows the user to type combinations forbidden by
   the aforementioned protocols.  There are several other requirements
   in conflict with ~1.3~ for the same reason.

   As such, to meet the requirements the keyboard must support
   switching between which requirements are enabled at a point in time.

   The solution used is to add the concept of a ~Mode~ to the
   keyboard. A key combination can switch the Mode of the keyboard.
   While in a mode the keyboard follows either requirement ~1.3~ or all
   of those requirements precluded by ~1.3~.
** Concepts
*** Key-Code
    TODO: Get the rest of the HID standard here and make this a
    subsection of it.

    A Key-Code is a concept from the USB-HID standard and replaces the
    Scan-Code concept in PS2.

    A Key-Code is a unique code for each key the USB-HID standard
    accepts and are mend by that standard to map directly to each key
    on the keyboard.  This was meant to allow software on the computer
    to remap and reinterpret those keys.  This has historically not
    occurred outside of the operating system and only limitedly there.

    #+BEGIN_SRC c
//*/
// clangformat on
/*
 * USB HID Keyboard scan codes as per USB spec 1.11
 *
 * Adapted from:
 * https://source.android.com/devices/input/keyboard-devices.html
 */

enum struct usb_hid_modifier:unit8_t;
enum struct usb_hid_key:uint8_t;

/*
 * If more than N keys are pressed, the HID reports
 * KEY_ERR_OVF in all slots to indicate this condition.
 */

struct USB_HID_STATUS{
	usb_hid_modifier modifier;
	uint8_t; //RESERVED
	usb_hid_key key[6];
};

/*
 * Modifier masks - used for the first byte in the HID report.
 */
enum struct usb_hid_modifier:uint8_t{
	LCTRL	= 0x01,
	LSHIFT	= 0x02,
	LALT	= 0x04,
	LMETA	= 0x08,
	RCTRL	= 0x10,
	RSHIFT	= 0x20,
	RALT	= 0x40,
	RMETA	= 0x80
};

/*
 * USB HID Key Scan Codes
 */
enum struct usb_hid_key:uint8_t{
	none            = 0x00, // No key pressed
	error_overflow  = 0x01, // More than 8 keys pressed
	post_fail       = 0x02, // Keyboard failed post
	error_undefined = 0x03, // Keyboard Error Undefined

	key_a           = 0x04,
	key_b           = 0x05,
	key_c           = 0x06,
	key_d           = 0x07,
	key_e           = 0x08,
	key_f           = 0x09,
	key_g           = 0x0a,
	key_h           = 0x0b,
	key_i           = 0x0c,
	key_j           = 0x0d,
	key_k           = 0x0e,
	key_l           = 0x0f,
	key_m           = 0x10,
	key_n           = 0x11,
	key_o           = 0x12,
	key_p           = 0x13,
	key_q           = 0x14,
	key_r           = 0x15,
	key_s           = 0x16,
	key_t           = 0x17,
	key_u           = 0x18,
	key_v           = 0x19,
	key_w           = 0x1a,
	key_x           = 0x1b,
	key_y           = 0x1c,
	key_z           = 0x1d,

	key_1           = 0x1e, // Alternate !
	key_2           = 0x1f, // Alternate @
	key_3           = 0x20, // Alternate #
	key_4           = 0x21, // Alternate $
	key_5           = 0x22, // Alternate %
	key_6           = 0x23, // Alternate ^
	key_7           = 0x24, // Alternate &
	key_8           = 0x25, // Alternate *
	key_9           = 0x26, // Alternate (
	key_0           = 0x27, // Alternate )

	key_enter       = 0x28.
	key_esc	        = 0x29.
	key_backspace   = 0x2a.
	key_tab	        = 0x2b.
	key_space       = 0x2c.

	key_minus       = 0x2d, // Alternate  _
	key_equal       = 0x2e, // Alternate  +
	key_leftbrace   = 0x2f, // Alternate  {
	key_rightbrace  = 0x30, // Alternate  }
	key_backslash   = 0x31, // Alternate  |
	key_semicolon   = 0x33, // Alternate  :
	key_apostrophe  = 0x34, // Alternate  "
	key_grave       = 0x35, // Alternate  ~
	key_comma       = 0x36, // Alternate  <
	key_dot         = 0x37, // Alternate  >
	key_slash       = 0x38, // Alternate  ?

	key_capslock	= 0x39,

	key_f1          = 0x3a,
	key_f2          = 0x3b,
	key_f3          = 0x3c,
	key_f4          = 0x3d,
	key_f5          = 0x3e,
	key_f6          = 0x3f,
	key_f7          = 0x40,
	key_f8          = 0x41,
	key_f9          = 0x42,
	key_f10         = 0x43,
	key_f11         = 0x44,
	key_f12         = 0x45,

	key_prnt_screen = 0x46, // also SysRq
	key_scrolllock  = 0x47,
	key_pause       = 0x48,
	key_insert      = 0x49,
	key_home        = 0x4a,
	key_pageup      = 0x4b,
	key_delete      = 0x4c,
	key_end         = 0x4d,
	key_pagedown    = 0x4e,
	key_right       = 0x4f,
	key_left        = 0x50,
	key_down        = 0x51,
	key_up          = 0x52,

	//TODO: what's "Clear"?
	key_numlock     = 0x53, // Keyboard Num Lock and Clear

	numpad_slash    = 0x54,
	numpad_asterisk = 0x55,
	numpad_minus    = 0x56,
	numpad_plus     = 0x57,
	numpad_equal    = 0x67, // Out of order
	numpad_enter    = 0x58,
	numpad_1        = 0x59, // Alternate End
	numpad_2        = 0x5a, // Alternate Down Arrow
	numpad_3        = 0x5b, // Alternate PageDn
	numpad_4        = 0x5c, // Alternate Left Arrow
	numpad_5        = 0x5d,
	numpad_6        = 0x5e, // Alternate Right Arrow
	numpad_7        = 0x5f, // Alternate Home
	numpad_8        = 0x60, // Alternate Up Arrow
	numpad_9        = 0x61, // Alternate Page Up
	numpad_0        = 0x62, // Alternate Insert
	numpad_dot      = 0x63, // Alternate Delete

		// Common International Keys not on US keyboards
	key_hash        = 0x32, // Alternate  ~
	key_102nd       = 0x64, // Same as 0x31, also types \ Alternate |
	numpad_comma    = 0x85,

#if DEVELOPMENT_BUILD
		// TODO: Test what any of the keys after here actually do.

	key_compose     = 0x65, // Keyboard Application
	key_power       = 0x66, // Keyboard Power

		// Second Function Key Row
	key_f13         = 0x68,
	key_f14         = 0x69,
	key_f15         = 0x6a,
	key_f16         = 0x6b,
	key_f17         = 0x6c,
	key_f18         = 0x6d,
	key_f19         = 0x6e,
	key_f20         = 0x6f,
	key_f21         = 0x70,
	key_f22         = 0x71,
	key_f23         = 0x72,
	key_f24         = 0x73,

	key_open        = 0x74, // Keyboard Execute
	key_help        = 0x75, // Keyboard Help
	key_props       = 0x76, // Keyboard Menu
	key_front       = 0x77, // Keyboard Select
	key_stop        = 0x78, // Keyboard Stop
	key_again       = 0x79, // Keyboard Again
	key_undo        = 0x7a, // Keyboard Undo
	key_cut         = 0x7b, // Keyboard Cut
	key_copy        = 0x7c, // Keyboard Copy
	key_paste       = 0x7d, // Keyboard Paste
	key_find        = 0x7e, // Keyboard Find
	key_mute        = 0x7f, // Keyboard Mute
	key_volumeup    = 0x80, // Keyboard Volume Up
	key_volumedown  = 0x81, // Keyboard Volume Down

	key_lock_caps_lock   = 0x82, // Keyboard Locking Caps Lock
	key_lock_num_lock    = 0x83, // Keyboard Locking Num Lock
	key_lock_scroll_lock = 0x84, // Keyboard Locking Scroll Lock

	numpad_equal_2  = 0x86, // Keypad Equal Sign

	key_international_1  = 0x87, // Keyboard ro
	key_international_2  = 0x88, // Keyboard Katakana Hiragana
	key_international_3  = 0x89, // Keyboard Yen
	key_international_4  = 0x8a, // Keyboard henkan
	key_international_5  = 0x8b, // Keyboard muhenkan
	key_international_6  = 0x8c, // Keyboard Kp Jp Comma
	key_international_7  = 0x8d,
	key_international_8  = 0x8e,
	key_international_9  = 0x8f,
	key_lanugage_1  = 0x90, // Keyboard Hangeul
	key_lanugage_2  = 0x91, // Keyboard Hanja
	key_lanugage_3  = 0x92, // Keyboard Katakana
	key_lanugage_4  = 0x93, // Keyboard Hiragana
	key_lanugage_5  = 0x94, // Keyboard Zenkakuhankaku
	key_language_6  = 0x95,
	key_language_7  = 0x96,
	key_language_8  = 0x97,
	key_language_9  = 0x98,

// 0x99 Keyboard Alternate Erase
// 0x9a Keyboard SysReq/Attention
// 0x9b Keyboard Cancel
// 0x9c Keyboard Clear
// 0x9d Keyboard Prior
// 0x9e Keyboard Return
// 0x9f Keyboard Separator
// 0xa0 Keyboard Out
// 0xa1 Keyboard Oper
// 0xa2 Keyboard Clear/Again
// 0xa3 Keyboard CrSel/Props
// 0xa4 Keyboard ExSel

	numpad_00       = 0xb0, // Keypad 00
	numpad_000 = 0xb1, // Keypad 000
	numpad_thousand_seperator = 0xb2, // Thousands Separator
	numpad_decimal_seperator = 0xb3, // Decimal Separator
	numpad_currency = 0xb4, // Currency Unit
	numpad_currency_sub = 0xb5, // Currency Sub-unit
	numpad_left_paren = 0xb6, // Keypad (
	numpad_right_paren = 0xb7, // Keypad )
		numpad_ = 0xb8, // Keypad {
		                          numpad_ = 0xb9, // Keypad }
	numpad_tab = 0xba,
	numpad_backspace = 0xbb,
	numpad_A = 0xbc,
	numpad_B = 0xbd,
	numpad_C = 0xbe,
	numpad_D = 0xbf,
	numpad_E = 0xc0,
	numpad_F = 0xc1,
	numpad_XOR = 0xc2,
	numpad_carrot = 0xc3, // Keypad ^
	numpad_percent = 0xc4, // Keypad %
	numpad_lessthan = 0xc5, // Keypad <
	numpad_greaterthan = 0xc6, // Keypad >
	numpad_amper = 0xc7, // Keypad &
	numpad_amperamper = 0xc8, // Keypad &&
	numpad_pipe = 0xc9, // Keypad |
	numpad_pipepipe = 0xca, // Keypad ||
	numpad_colon = 0xcb, // Keypad :
	numpad_hash = 0xcc, // Keypad #
	numpad_space = 0xcd, // Keypad Space
	numpad_at = 0xce, // Keypad @
	numpad_bang = 0xcf, // Keypad !
	numpad_memory_store = 0xd0,
	numpad_memory_recall = 0xd1,
	numpad_memory_clear = 0xd2,
	numpad_memory_add = 0xd3,
	numpad_memory_subtract = 0xd4,
	numpad_memory_multiply = 0xd5,
	numpad_memory_divide = 0xd6,
	numpad_sign_toggle = 0xd7, // Keypad +/-
	numpad_clear = 0xd8,
	numpad_clear_entry = 0xd9,
	numpad_base_binary = 0xda,
	numpad_base_octal = 0xdb,
	numpad_base_decimal = 0xdc,
	numpad_base_hexadecimal = 0xdd,

		// TODO: Why do these have key events as well?
	// key_leftctrl = 0xe0, // Keyboard Left Control
	// key_leftshift = 0xe1, // Keyboard Left Shift
	// key_leftalt = 0xe2, // Keyboard Left Alt
	// key_leftmeta = 0xe3, // Keyboard Left GUI
	// key_rightctrl = 0xe4, // Keyboard Right Control
	// key_rightshift = 0xe5, // Keyboard Right Shift
	// key_rightalt = 0xe6, // Keyboard Right Alt
	// key_rightmeta = 0xe7, // Keyboard Right GUI

	media_playpause    = 0xe8,
	media_stopcd       = 0xe9,
	media_previoussong = 0xea,
	media_nextsong     = 0xeb,
	media_ejectcd      = 0xec,
	media_volumeup     = 0xed,
	media_volumedown   = 0xee,
	media_mute         = 0xef,
	media_www          = 0xf0,
	media_back         = 0xf1,
	media_forward      = 0xf2,
	media_stop         = 0xf3,
	media_find         = 0xf4,
	media_scrollup     = 0xf5,
	media_scrolldown   = 0xf6,
	media_edit         = 0xf7,
	media_sleep        = 0xf8,
	media_coffee       = 0xf9, // wat?
	media_refresh      = 0xfa,
	media_calc         = 0xfb
#endif
};
// clangformat off
/*
    #+END_SRC
*** Keyboard
**** Model
     There are several designs of keyboard hardware that this project
     attempts to support.

     #+BEGIN_SRC c
//*/
// clangformat on
#define ERGODOX_LEFT
#define ERGODOX_RIGHT
#define TEST_KEYBOADR_UNIFIED
#define KEYBOARD_HARDWARE_MODEL TEST_KEYBOADR_UNIFIED
// clangformat off
/*
     #+END_SRC
**** Rows and Columns
     TODO: text here.

     #+BEGIN_SRC c
//*/
// clangformat on
#if KEYBOARD_HARDWARE_MODEL == ERGODOX_LEFT
#define KEYBOARD_COLUMNS_COUNT  7
#define KEYBOARD_COLUMNS_PINS
#define KEYBOARD_ROWS_COUNT     6
#define KEYBOARD_ROWS_PINS
#if KEYBOARD_HARDWARE_MODEL == ERGODOX_RIGHT
#define KEYBOARD_COLUMNS_COUNT  7
#define KEYBOARD_COLUMNS_PINS
#define KEYBOARD_ROWS_COUNT     6
#define KEYBOARD_ROWS_PINS
#elif KEYBOARD_HARDWARE_MODEL == TEST_KEYBOADR_UNIFIED
#define KEYBOARD_COLUMNS_COUNT 14
#define KEYBOARD_COLUMNS_PINS
#define KEYBOARD_ROWS_COUNT     6
#define KEYBOARD_ROWS_PINS
#endif
// clangformat off
/*
     #+END_SRC
**** Key-Press
     A key press is either an up or a down event from an individual key
     on the keyboard.
*** Rep-Key
    Rep-Key was a hardware feature in old keyboards and was moved to
    software in the USB-HID protocol.  It is the feature of if a key
    is held down it is to be repetitively typed.

    Settings on Rep-Key are:
    1) How long to wait before starting to send the key after the
       initial pressing.
    2) How frequently to send the copies key.

    TODO: A possible feature addition is to send keys in "bursts" so
    that, per se, 8 keys would be sent and then a longer wait would
    occur.

    TODO: The rep-keys feature isn't really implementable in the above
    system.  The key press and key release functions can add and
    remove a char(s) from the ~rep_buffer~ and a timed daemon (like
    the Bluetooth one) can keep re-adding the keys every unit time.

    TODO: This belongs somewhere else.  Offer an Emacs like ~M-7 a~
    types 7 'a's.

    #+BEGIN_SRC c
//*/
// clangformat on
struct Rep_Key_Settings{
        uint8_t initial_wait_time_ms;
        uint8_t keypress_frequency_ms;
        // uint8_t burst_size;
        // uint8_t inter_burst_wait_ms;
};
// clangformat off
/*
    #+END_SRC
*** Layer
    The term "Layer" is borrowed from ErgoDox's parlance.  A layer
    contains the information about what is meant by a key press.  For
    example a layer called "Mirror" might be used to make all keys on
    the left side of the keyboard pretend to be on the right for ease
    of use with a mouse, or a function layer might map "WASD" to the
    arrow keys.

    Layers keys come in two forms, Tap and Hold.  The list of active
    layers uses a chronological "stack".
**** Hold Layer Keys
     If a layer key is a hold key then the key which invoked the layer
     needs to be held down while the mode is enabled.  Note that if a
     second layer is entered from a first releaseing the first will
     not change the meaning of the second.  (It is belived that
     ergodox does not support this as a defined behavior and so no
     compilence is needed.)

     Example:
     1. Let Normal, Mirror,  Funciton, and Mirrored-Funciton be
        layers.
     2. Startup
	- Layer Stack: Normal
     3. Mirror Key is pressed down
	- Layer Stack: Normal, Mirror
     4. Mirror-Function key is pressed down
	- Layer Stack: Normal, Mirror, Mirror-Function
     5. Mirror Key is released
	- Layer Stack: Normal, Mirror-Function

     As we can see, at the end of this the Mirror-Funciton Layer is
     still the active layer.
**** Tap Layer Key
     If a layer key is a Tap Layer Key then tapping the key switches
     to the layer and clears the Layer stack.

     Example:
     1. Let Normal, Meta, and Function be layers, and Function be
        assigned to a Tap Key in the Meta Layer.
     2. Startup
	- Layer Stack: Normal
     3. Meta is pressed
	- Layer Stack: Normal, Meta
     4. Function is Tapped
	- Layer Stack: Funciton
     5. Meta is Released
        - Layer Stack: Funciton
***** TODO Pop Layer Key
     It is not required that a Tap Layer Key clear the layer stack, as
     one can assign a key to "Pop the current layer".

     Example:
     Let Normal and Meta be layers.
     1. Startup
	- Stack: Normal
     2. Tap Meta
        - Stack: Normal, Meta
     3. Type things
        - Things done in Meta layer
        - Stack: Normal, Meta
     4. Tap Pop
        - Stack: Normal
***** TODO Sticky Layers
      Another variant of the Tap Layer Key is the sticky layer.  A
      sticky layer is a layer that is applied for only the next key
      press.
**** Layer Stack
     Layers are applied as an ordered operation.  When a layer key is
     pressed or tapped that layer is added to the layer stack.  When a
     key in the layer stack is released, if it is the current (top)
     key in the stack the layer is reverted to the layer added by the
     next top most key in the stack.  It that key is not the top most
     then it is removed from the layer stack and the current layer is
     left as it is.
*** Mode
    Each layer has a mode, either ~Gaming~ or ~Typing~.
**** Gaming Modes
     In ~Gaming~ mode the key layout must follow the requirements
     imposed by the PS2 and USB-HID protocols.

     More explicitly:
     1. Keys are assigned to either a key-code or a modifier key.
        1. Keys are assigned to PS2 keys rather than chars.  As such
           'A' can't be assigned to a key, only 'a' can.  This is more
           notable for symbol keys.
        2. Keys can't be Non-standard chars like '£.

     This is so that when a key is pressed the computer can see the
     key as held until it's release.  This generaly only matters when
     playing a game.
**** Typing Modes
     In ~Typing~ mode holding a keys sends the key as a press and a
     release.  The layer configuration must then also specify the
     settings for Rep-Key to emulate the standard key holding effect
     in most programs.
** Generating Key-Codes
   1. Respect Caps-Lock State
   2. Respect Num-lock State
   3. Latency
** Sending Unicode Characters
   Depending on the operating systems being used there are different
   and possibly no way to send Unicode characters.

   Original Source
   https://github.com/duckythescientist/BinaryKeyboard.git

   The currently known and supported methods of sending Unicode
   characters are:
*** Windows
    1) Press ALT
    2) Send numpad_plus
    3) Type hexadecimal value
    4) Release ALT

    Note: ~HKEY_CURRENT_USER\Control Panel\Input Method~ contains a
    string type (REG_SZ) value called ~EnableHexNumpad~, which must
    have the value ~1~.

    Note: Windows seems to only type 2-byte symbols.  There are other
    methods that may have more symbols, but this is the more universal
    method.

    #+BEGIN_SRC c
//*/
#define OS_WINDOWS_UNICODE {/*TODO*/}
/*
    #+END_SRC
*** Linux
    1) Send CTRL+ALT+SHIFT+U
    2) Type hexadecimal (no leading zeros)
    3) Send space

    Note: Some applications need the ALT, and some don't. It seems to
    work on both if it is held.

    Note: this may be X specific or application specific.  I've had
    success in Ubuntu 14.04 with Unity (Chrome, Guake, Gedit, Sublime
    Text 3).

    #+BEGIN_SRC c
//*/
#define OS_LINIX_UNICODE {/*TODO*/}
/*
    #+END_SRC
*** Mac
    No tested methods exist yet, assuming the Linux one works.

    #+BEGIN_SRC c
//*/
#define OS_DARWIN_UNICODE OS_LINIX_UNICODE
/*
    #+END_SRC
*** Emacs
    The function ~insert-char~ allows for the insertion of Unicode
    characters.  This function usually bound to ~C-x 8 <ret> hex_code~
    but for reliability the command ~M-x insert-char <ret> hex_code~
    will be used.  This is customizable to respect the user's
    configuration here.

    #+BEGIN_SRC c
//*/
#define OS_EMACS_UNICODE {/*TODO*/}
/*
    #+END_SRC
** OOOOOOOOOOOOOOOOO
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
   3. Press Shift, Tap 'a', Tap 'b' -> Send 'AB'
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
//*/
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
// clangformat off
/*
 #+END_SRC
* End of File Format Corrector
  This is so the C interpretation of this file is valid.

  Note: The final comment ends with an escape so that it eats the
  source block end.

  #+BEGIN_SRC c
//*/
// \
  #+END_SRC

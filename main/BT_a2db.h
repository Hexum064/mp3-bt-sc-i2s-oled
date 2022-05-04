#include <stdbool.h>
#ifndef BT_A2DB_H
#define BT_A2DB_H

#include "esp_bt.h"
#include "bt_app_core.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "esp_wifi.h"

#define BT_AV_TAG				"BT_AV"
#define BT_RC_CT_TAG			"RCCT"

// number of retries when connecting
#define BT_CONNECT_RETRY 2

// AVRCP used transaction label
#define APP_RC_CT_TL_GET_CAPS			 (0)
#define APP_RC_CT_TL_RN_VOLUME_CHANGE	 (1)

/* event for handler "bt_av_hdl_stack_up */
enum {
	BT_APP_EVT_STACK_UP = 0,
};

/* A2DP global state */
enum {
	APP_AV_STATE_IDLE,
	APP_AV_STATE_DISCOVERING,
	APP_AV_STATE_DISCOVERED,
	APP_AV_STATE_UNCONNECTED,
	APP_AV_STATE_CONNECTING,
	APP_AV_STATE_CONNECTED,
	APP_AV_STATE_DISCONNECTING,
};

/* sub states of APP_AV_STATE_CONNECTED */
enum {
	APP_AV_MEDIA_STATE_IDLE,
	APP_AV_MEDIA_STATE_STARTING,
	APP_AV_MEDIA_STATE_STARTED,
	APP_AV_MEDIA_STATE_STOPPING,
};

#define BT_APP_HEART_BEAT_EVT				 (0xff00)

class BT_a2db
{

    private :

    

    public :
		static bool ready;
        BT_a2db(esp_a2d_source_data_cb_t callback);
        bool discover_bluetooth(const char * speaker_name);
		bool connect_bluetooth(esp_bd_addr_t speaker_address);
		int get_media_state();
		int get_a2d_state();

};



#endif //BT_A2DB_H
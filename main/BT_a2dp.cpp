#include "BT_a2dp.h"
#include "esp_log.h"
#include "freertos/timers.h"

#include <string.h>

extern "C" {
	#include "btc_av.h"
}

bool BT_a2dp::ready = false;

/// handler for bluetooth stack enabled events
static void bt_av_hdl_stack_evt(uint16_t event, void *p_param);

/// callback function for A2DP source audio data stream
//static int32_t bt_app_a2d_data_cb(uint8_t *data, int32_t len);

/// callback function for AVRCP controller
static void bt_app_rc_ct_cb(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param);

static void a2d_app_heart_beat(void *arg);

/// A2DP application state machine
static void bt_app_av_sm_hdlr(uint16_t event, void *param);

/// avrc CT event handler
static void bt_av_hdl_avrc_ct_evt(uint16_t event, void *p_param);

/* A2DP application state machine handler for each state */
static void bt_app_av_state_unconnected(uint16_t event, void *param);
static void bt_app_av_state_connecting(uint16_t event, void *param);
static void bt_app_av_state_connected(uint16_t event, void *param);
static void bt_app_av_state_disconnecting(uint16_t event, void *param);

/// callback function for A2DP source
static void bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param);

static esp_bd_addr_t s_peer_bda = {0};
static uint8_t s_peer_bdname[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
static int s_a2d_state = APP_AV_STATE_IDLE;
static int s_media_state = APP_AV_MEDIA_STATE_IDLE;
static int s_intv_cnt = 0;
static int s_connecting_intv = 0;
static uint32_t s_pkt_cnt = 0;
static esp_avrc_rn_evt_cap_mask_t s_avrc_peer_rn_cap;
static TimerHandle_t s_tmr;
static esp_a2d_source_data_cb_t data_callback;
const char* bt_speaker_name;

//Discovery
static bool f_discover = false;
static esp_a2d_found_devices_cb_t found_devices_cb = NULL;
static bt_device_param found_devices[MAX_ADDRESSES];
static uint8_t found_device_count = 0;

//NVS
static nvs_handle_t bt_nvs_handle = 0;
static char nvs_name[] = "btnvs";

BT_a2dp::BT_a2dp(esp_a2d_source_data_cb_t callback)
{

    data_callback = callback;
    esp_err_t results = esp_wifi_stop();

	// Initialize NVS.
	esp_err_t ret = nvs_flash_init();

	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	
	ESP_ERROR_CHECK( ret );

	ESP_ERROR_CHECK( nvs_open(nvs_name, NVS_READWRITE, &bt_nvs_handle) );

	ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();


	if (esp_bt_controller_init(&bt_cfg) != ESP_OK) {
		ESP_LOGE(BT_AV_TAG, "%s initialize controller failed\n", __func__);
		return;
	}

	if (esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT) != ESP_OK) {
		ESP_LOGE(BT_AV_TAG, "%s enable controller failed\n", __func__);
		return;
	}

	if (esp_bluedroid_init() != ESP_OK) {
		ESP_LOGE(BT_AV_TAG, "%s initialize bluedroid failed\n", __func__);
		return;
	}

	if (esp_bluedroid_enable() != ESP_OK) {
		ESP_LOGE(BT_AV_TAG, "%s enable bluedroid failed\n", __func__);
		return;
	}
}


//------START BLUETOOTH -----

static char *bda2str(esp_bd_addr_t bda, char *str, size_t size)
{
	if (bda == NULL || str == NULL || size < 18) {
		return NULL;
	}

	uint8_t *p = bda;
	sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
			p[0], p[1], p[2], p[3], p[4], p[5]);
	return str;
}

static bool get_name_from_eir(uint8_t *eir, uint8_t *bdname, uint8_t *bdname_len)
{
	uint8_t *rmt_bdname = NULL;
	uint8_t rmt_bdname_len = 0;

	if (!eir) {
		return false;
	}

	rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &rmt_bdname_len);
	if (!rmt_bdname) {
		rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &rmt_bdname_len);
	}

	if (rmt_bdname) {
		if (rmt_bdname_len > ESP_BT_GAP_MAX_BDNAME_LEN) {
			rmt_bdname_len = ESP_BT_GAP_MAX_BDNAME_LEN;
		}

		if (bdname) {
			memcpy(bdname, rmt_bdname, rmt_bdname_len);
			bdname[rmt_bdname_len] = '\0';
		}
		if (bdname_len) {
			*bdname_len = rmt_bdname_len;
		}
		return true;
	}

	return false;
}


static bool hasAddress(esp_bd_addr_t address, uint8_t *name)
{
	for (int i = 0; i < found_device_count; i++)
	{
		if (found_devices[i].address[0] == address[0] && 
			found_devices[i].address[1] == address[1] && 
			found_devices[i].address[2] == address[2] && 
			found_devices[i].address[3] == address[3] && 
			found_devices[i].address[4] == address[4] && 
			found_devices[i].address[5] == address[5])
			{
				return true;
			}

		
	}

	memcpy(found_devices[found_device_count].name, name, MAX_BT_NAME_LEN);
	found_devices[found_device_count].address[0] = address[0];
	found_devices[found_device_count].address[1] = address[1];
	found_devices[found_device_count].address[2] = address[2];
	found_devices[found_device_count].address[3] = address[3];
	found_devices[found_device_count].address[4] = address[4];
	found_devices[found_device_count].address[5] = address[5];
	found_device_count++;
	return false;
}

static void filter_inquiry_scan_result(esp_bt_gap_cb_param_t *param)
{
	char bda_str[MAX_BT_NAME_LEN];
	uint32_t cod = 0;
	int32_t rssi = -129; /* invalid value */
	uint8_t *eir = NULL;
	esp_bt_gap_dev_prop_t *p;
	bda2str(param->disc_res.bda, bda_str, MAX_BT_NAME_LEN);
	
	if (!f_discover)
		ESP_LOGI(BT_AV_TAG, "Scanned device: %s", bda_str);

	for (int i = 0; i < param->disc_res.num_prop; i++) {
		p = param->disc_res.prop + i;
		switch (p->type) {
		case ESP_BT_GAP_DEV_PROP_COD:
			cod = *(uint32_t *)(p->val);
			if (!f_discover)
				ESP_LOGI(BT_AV_TAG, "--Class of Device: 0x%x", cod);
			break;
		case ESP_BT_GAP_DEV_PROP_RSSI:
			rssi = *(int8_t *)(p->val);
			if (!f_discover)
				ESP_LOGI(BT_AV_TAG, "--RSSI: %d", rssi);
			break;
		case ESP_BT_GAP_DEV_PROP_EIR:
			eir = (uint8_t *)(p->val);
			break;
		case ESP_BT_GAP_DEV_PROP_BDNAME:
		default:
			break;
		}
	}

	/* search for device with MAJOR service class as "rendering" in COD */
	if (!esp_bt_gap_is_valid_cod(cod) ||
			!(esp_bt_gap_get_cod_srvc(cod) & ESP_BT_COD_SRVC_RENDERING)) {
		return;
	}

	/* search for device named bt_speaker_name in its extended inqury response */
	if (eir) {
		get_name_from_eir(eir, s_peer_bdname, NULL);
		if (!f_discover)
			ESP_LOGI(BT_AV_TAG, "Checking potential device, address %s, name %s", bda_str, s_peer_bdname);
		//nop

		if (f_discover)
		{
			if (!hasAddress(param->disc_res.bda, s_peer_bdname))
			{
				ESP_LOGI(BT_AV_TAG, "Found a target device, address %s, name %s", bda_str, s_peer_bdname);
				if (found_devices_cb)
				{
					found_devices_cb(found_devices, found_device_count);
				}
			
			}
			return;
		}
		else
		{				
			//if (strcmp((char *)s_peer_bdname, "ESP_SPEAKER") != 0) {	
			if (strcmp((char *)s_peer_bdname, bt_speaker_name) != 0) 
			{
				return;
			}
		}

		ESP_LOGI(BT_AV_TAG, "Found a target device, address %s, name %s", bda_str, s_peer_bdname);
		s_a2d_state = APP_AV_STATE_DISCOVERED;
		memcpy(s_peer_bda, param->disc_res.bda, ESP_BD_ADDR_LEN);
		ESP_LOGI(BT_AV_TAG, "Cancel device discovery ...");
		esp_bt_gap_cancel_discovery();
	}
}



void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
	char bda_str[18];
	switch (event) {
	case ESP_BT_GAP_DISC_RES_EVT: {
		filter_inquiry_scan_result(param);
		break;
	}

	//TODO: HERE is where we end up when a specific BT device is found. Here we can start to modify the code to take out the connect and put it in a separate function
	//That uses the s_peer_bda (address) to connect. We can also remove the filter_inquiry_scan_result and use it to return all devices found.
	//DONE:

	case ESP_BT_GAP_DISC_STATE_CHANGED_EVT: {
		if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
			if (s_a2d_state == APP_AV_STATE_DISCOVERED) {
				s_a2d_state = APP_AV_STATE_CONNECTING;
				ESP_LOGI(BT_AV_TAG, "Device discovery stopped.");
				ESP_LOGI(BT_AV_TAG, "a2dp connecting to peer: %s", s_peer_bdname);
				esp_a2d_source_connect(s_peer_bda);
			} else {
				// not discovered, continue to discover
				ESP_LOGI(BT_AV_TAG, "Device discovery failed, continue to discover...");
				esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
			}
		} else if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED) {
			ESP_LOGI(BT_AV_TAG, "Discovery started.");
		}
		break;
	}
	case ESP_BT_GAP_RMT_SRVCS_EVT:
	case ESP_BT_GAP_RMT_SRVC_REC_EVT:
		break;
	case ESP_BT_GAP_AUTH_CMPL_EVT: {
		if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
			ESP_LOGI(BT_AV_TAG, "authentication success: %s", param->auth_cmpl.device_name);
			esp_log_buffer_hex(BT_AV_TAG, param->auth_cmpl.bda, ESP_BD_ADDR_LEN);
		} else {
			ESP_LOGE(BT_AV_TAG, "authentication failed, status:%d", param->auth_cmpl.stat);
		}
		break;
	}
	case ESP_BT_GAP_PIN_REQ_EVT: {
		ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_PIN_REQ_EVT min_16_digit:%d", param->pin_req.min_16_digit);
		if (param->pin_req.min_16_digit) {
			ESP_LOGI(BT_AV_TAG, "Input pin code: 0000 0000 0000 0000");
			esp_bt_pin_code_t pin_code = {0};
			esp_bt_gap_pin_reply(param->pin_req.bda, true, 16, pin_code);
		} else {
			ESP_LOGI(BT_AV_TAG, "Input pin code: 1234");
			esp_bt_pin_code_t pin_code;
			pin_code[0] = '1';
			pin_code[1] = '2';
			pin_code[2] = '3';
			pin_code[3] = '4';
			esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin_code);
		}
		break;
	}

#if (CONFIG_BT_SSP_ENABLED == true)
	case ESP_BT_GAP_CFM_REQ_EVT:
		ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_CFM_REQ_EVT Please compare the numeric value: %d", param->cfm_req.num_val);
		esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
		break;
	case ESP_BT_GAP_KEY_NOTIF_EVT:
		ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_KEY_NOTIF_EVT passkey:%d", param->key_notif.passkey);
		break;
	case ESP_BT_GAP_KEY_REQ_EVT:
		ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_KEY_REQ_EVT Please enter passkey!");
		break;
#endif

	case ESP_BT_GAP_MODE_CHG_EVT:
		ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_MODE_CHG_EVT mode:%d", param->mode_chg.mode);
		break;

	default: {
		ESP_LOGI(BT_AV_TAG, "event: %d", event);
		break;
	}
	}
	return;
}

static void bt_av_hdl_stack_evt(uint16_t event, void *p_param)
{
	ESP_LOGD(BT_AV_TAG, "%s evt %d", __func__, event);
	switch (event) {
	case BT_APP_EVT_STACK_UP: {
		/* set up device name */
		char *dev_name = "ESP_A2DP_SRC";
		esp_bt_dev_set_device_name(dev_name);

		/* register GAP callback function */
		esp_bt_gap_register_callback(bt_app_gap_cb);

		/* initialize AVRCP controller */
		esp_avrc_ct_init();
		esp_avrc_ct_register_callback(bt_app_rc_ct_cb);

		esp_avrc_rn_evt_cap_mask_t evt_set = {0};
		esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_SET, &evt_set, ESP_AVRC_RN_VOLUME_CHANGE);
		assert(esp_avrc_tg_set_rn_evt_cap(&evt_set) == ESP_OK);

		/* initialize A2DP source */
		esp_a2d_register_callback(&bt_app_a2d_cb);
		esp_a2d_source_register_data_callback(data_callback);
		esp_a2d_source_init();

		/* set discoverable and connectable mode */
		esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

//TODO: Here we can start discovery or branch this off and connect directly.
//DONE:
		if (!f_discover)
		{
			/* start device discovery */
			ESP_LOGI(BT_AV_TAG, "Starting connecting to the device...");
			s_a2d_state = APP_AV_STATE_CONNECTING;
			esp_a2d_source_connect(s_peer_bda);

		}
		else
		{

			/* start device discovery */
			ESP_LOGI(BT_AV_TAG, "Starting device discovery...");
			s_a2d_state = APP_AV_STATE_DISCOVERING;
			esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
		}

		/* create and start heart beat timer */
		do {
			int tmr_id = 0;
			s_tmr = xTimerCreate("connTmr", (10000 / portTICK_RATE_MS),
							   pdTRUE, (void *) &tmr_id, a2d_app_heart_beat);
			xTimerStart(s_tmr, portMAX_DELAY);
		} while (0);
		break;
	}
	default:
		ESP_LOGE(BT_AV_TAG, "%s unhandled evt %d", __func__, event);
		break;
	}
}

static void bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
	bt_app_work_dispatch(bt_app_av_sm_hdlr, event, param, sizeof(esp_a2d_cb_param_t), NULL);
}






static void a2d_app_heart_beat(void *arg)
{
	bt_app_work_dispatch(bt_app_av_sm_hdlr, BT_APP_HEART_BEAT_EVT, NULL, 0, NULL);
}

static void bt_app_av_sm_hdlr(uint16_t event, void *param)
{
	ESP_LOGI(BT_AV_TAG, "%s state %d, evt 0x%x", __func__, s_a2d_state, event);
	switch (s_a2d_state) {
	case APP_AV_STATE_DISCOVERING:
	case APP_AV_STATE_DISCOVERED:
		break;
	case APP_AV_STATE_UNCONNECTED:
		bt_app_av_state_unconnected(event, param);
		break;
	case APP_AV_STATE_CONNECTING:
		bt_app_av_state_connecting(event, param);
		break;
	case APP_AV_STATE_CONNECTED:
		bt_app_av_state_connected(event, param);
		break;
	case APP_AV_STATE_DISCONNECTING:
		bt_app_av_state_disconnecting(event, param);
		break;
	default:
		ESP_LOGE(BT_AV_TAG, "%s invalid state %d", __func__, s_a2d_state);
		break;
	}
}

static void bt_app_av_state_unconnected(uint16_t event, void *param)
{
	switch (event) {
	case ESP_A2D_CONNECTION_STATE_EVT:
	case ESP_A2D_AUDIO_STATE_EVT:
	case ESP_A2D_AUDIO_CFG_EVT:
	case ESP_A2D_MEDIA_CTRL_ACK_EVT:
		break;
	case BT_APP_HEART_BEAT_EVT: {
		uint8_t *p = s_peer_bda;
		ESP_LOGI(BT_AV_TAG, "a2dp connecting to peer: %02x:%02x:%02x:%02x:%02x:%02x",
				 p[0], p[1], p[2], p[3], p[4], p[5]);
		esp_a2d_source_connect(s_peer_bda);
		s_a2d_state = APP_AV_STATE_CONNECTING;
		s_connecting_intv = 0;
		break;
	}
	default:
		ESP_LOGE(BT_AV_TAG, "%s unhandled evt %d", __func__, event);
		break;
	}
}

static void bt_app_av_state_connecting(uint16_t event, void *param)
{
	esp_a2d_cb_param_t *a2d = NULL;
	switch (event) {
	case ESP_A2D_CONNECTION_STATE_EVT: {
		a2d = (esp_a2d_cb_param_t *)(param);
		if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
			ESP_LOGI(BT_AV_TAG, "a2dp connected");
			s_a2d_state =  APP_AV_STATE_CONNECTED;
			s_media_state = APP_AV_MEDIA_STATE_IDLE;
			esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
		} else if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
			s_a2d_state =  APP_AV_STATE_UNCONNECTED;
		}
		break;
	}
	case ESP_A2D_AUDIO_STATE_EVT:
	case ESP_A2D_AUDIO_CFG_EVT:
	case ESP_A2D_MEDIA_CTRL_ACK_EVT:
		break;
	case BT_APP_HEART_BEAT_EVT:
		if (++s_connecting_intv >= BT_CONNECT_RETRY) {
			s_a2d_state = APP_AV_STATE_UNCONNECTED;
			s_connecting_intv = 0;
		}
		break;
	default:
		ESP_LOGE(BT_AV_TAG, "%s unhandled evt %d", __func__, event);
		break;
	}
}

static void bt_app_av_media_proc(uint16_t event, void *param)
{
	esp_a2d_cb_param_t *a2d = NULL;
	switch (s_media_state) {
	case APP_AV_MEDIA_STATE_IDLE: {
		if (event == BT_APP_HEART_BEAT_EVT) {
			ESP_LOGI(BT_AV_TAG, "a2dp media ready checking ...");
			esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY);
		} else if (event == ESP_A2D_MEDIA_CTRL_ACK_EVT) {
			a2d = (esp_a2d_cb_param_t *)(param);
			if (a2d->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY &&
					a2d->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS) {
				ESP_LOGI(BT_AV_TAG, "a2dp media ready, starting ...");
				esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
				s_media_state = APP_AV_MEDIA_STATE_STARTING;
			}
		}
		break;
	}
	case APP_AV_MEDIA_STATE_STARTING: {
		if (event == ESP_A2D_MEDIA_CTRL_ACK_EVT) {
			a2d = (esp_a2d_cb_param_t *)(param);
			if (a2d->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_START &&
					a2d->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS) {
				ESP_LOGI(BT_AV_TAG, "a2dp media start successfully.");
				BT_a2dp::ready = true;
				s_intv_cnt = 0;
				s_media_state = APP_AV_MEDIA_STATE_STARTED;
			} else {
				// not started succesfully, transfer to idle state
				ESP_LOGI(BT_AV_TAG, "a2dp media start failed.");
				s_media_state = APP_AV_MEDIA_STATE_IDLE;
			}
		}
		break;
	}
	case APP_AV_MEDIA_STATE_STARTED: {
//nop
//play forever
#if 0
		if (event == BT_APP_HEART_BEAT_EVT) {
			if (++s_intv_cnt >= 10) {
				ESP_LOGI(BT_AV_TAG, "a2dp media stopping...");
				esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_STOP);
				s_media_state = APP_AV_MEDIA_STATE_STOPPING;
				s_intv_cnt = 0;
			}
		}
#endif
		break;
	}
	case APP_AV_MEDIA_STATE_STOPPING: {
		if (event == ESP_A2D_MEDIA_CTRL_ACK_EVT) {
			a2d = (esp_a2d_cb_param_t *)(param);
			if (a2d->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_STOP &&
					a2d->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS) {
				ESP_LOGI(BT_AV_TAG, "a2dp media stopped successfully, disconnecting...");
				s_media_state = APP_AV_MEDIA_STATE_IDLE;
				esp_a2d_source_disconnect(s_peer_bda);
				s_a2d_state = APP_AV_STATE_DISCONNECTING;
			} else {
				ESP_LOGI(BT_AV_TAG, "a2dp media stopping...");
				esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_STOP);
			}
		}
		break;
	}
	}
}

static void bt_app_av_state_connected(uint16_t event, void *param)
{
	esp_a2d_cb_param_t *a2d = NULL;
	switch (event) {
	case ESP_A2D_CONNECTION_STATE_EVT: {
		a2d = (esp_a2d_cb_param_t *)(param);
		if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
			ESP_LOGI(BT_AV_TAG, "a2dp disconnected");
			s_a2d_state = APP_AV_STATE_UNCONNECTED;

			// force a disconnect
			esp_a2d_source_disconnect(s_peer_bda);

			esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
		}
		break;
	}
	case ESP_A2D_AUDIO_STATE_EVT: {
		a2d = (esp_a2d_cb_param_t *)(param);
		if (ESP_A2D_AUDIO_STATE_STARTED == a2d->audio_stat.state) {
			s_pkt_cnt = 0;
		}
		break;
	}
	case ESP_A2D_AUDIO_CFG_EVT:
		// not suppposed to occur for A2DP source
		break;
	case ESP_A2D_MEDIA_CTRL_ACK_EVT:
	case BT_APP_HEART_BEAT_EVT: {
		bt_app_av_media_proc(event, param);
		break;
	}
	default:
		ESP_LOGE(BT_AV_TAG, "%s unhandled evt %d", __func__, event);
		break;
	}
}

static void bt_app_av_state_disconnecting(uint16_t event, void *param)
{
	esp_a2d_cb_param_t *a2d = NULL;
	switch (event) {
	case ESP_A2D_CONNECTION_STATE_EVT: {
		a2d = (esp_a2d_cb_param_t *)(param);
		if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
			ESP_LOGI(BT_AV_TAG, "a2dp disconnected");
			s_a2d_state =  APP_AV_STATE_UNCONNECTED;
			esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
		}
		break;
	}
	case ESP_A2D_AUDIO_STATE_EVT:
	case ESP_A2D_AUDIO_CFG_EVT:
	case ESP_A2D_MEDIA_CTRL_ACK_EVT:
	case BT_APP_HEART_BEAT_EVT:
		break;
	default:
		ESP_LOGE(BT_AV_TAG, "%s unhandled evt %d", __func__, event);
		break;
	}
}

static void bt_app_rc_ct_cb(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param)
{
	switch (event) {
	case ESP_AVRC_CT_METADATA_RSP_EVT:
	case ESP_AVRC_CT_CONNECTION_STATE_EVT:
	case ESP_AVRC_CT_PASSTHROUGH_RSP_EVT:
	case ESP_AVRC_CT_CHANGE_NOTIFY_EVT:
	case ESP_AVRC_CT_REMOTE_FEATURES_EVT:
	case ESP_AVRC_CT_GET_RN_CAPABILITIES_RSP_EVT:
	case ESP_AVRC_CT_SET_ABSOLUTE_VOLUME_RSP_EVT: {
		bt_app_work_dispatch(bt_av_hdl_avrc_ct_evt, event, param, sizeof(esp_avrc_ct_cb_param_t), NULL);
		break;
	}
	default:
		ESP_LOGE(BT_RC_CT_TAG, "Invalid AVRC event: %d", event);
		break;
	}
}

static void bt_av_volume_changed(void)
{
	if (esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_TEST, &s_avrc_peer_rn_cap,
										   ESP_AVRC_RN_VOLUME_CHANGE)) {
		esp_avrc_ct_send_register_notification_cmd(APP_RC_CT_TL_RN_VOLUME_CHANGE, ESP_AVRC_RN_VOLUME_CHANGE, 0);
	}
}

void bt_av_notify_evt_handler(uint8_t event_id, esp_avrc_rn_param_t *event_parameter)
{
	switch (event_id) {
	case ESP_AVRC_RN_VOLUME_CHANGE:
		ESP_LOGI(BT_RC_CT_TAG, "Volume changed: %d", event_parameter->volume);
		ESP_LOGI(BT_RC_CT_TAG, "Set absolute volume: volume %d", event_parameter->volume + 5);
		esp_avrc_ct_send_set_absolute_volume_cmd(APP_RC_CT_TL_RN_VOLUME_CHANGE, event_parameter->volume + 5);
		bt_av_volume_changed();
		break;
	}
}

static void bt_av_hdl_avrc_ct_evt(uint16_t event, void *p_param)
{
	ESP_LOGD(BT_RC_CT_TAG, "%s evt %d", __func__, event);
	esp_avrc_ct_cb_param_t *rc = (esp_avrc_ct_cb_param_t *)(p_param);
	switch (event) {
	case ESP_AVRC_CT_CONNECTION_STATE_EVT: {
		uint8_t *bda = rc->conn_stat.remote_bda;
		ESP_LOGI(BT_RC_CT_TAG, "AVRC conn_state evt: state %d, [%02x:%02x:%02x:%02x:%02x:%02x]",
				 rc->conn_stat.connected, bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);

		if (rc->conn_stat.connected) {
			// get remote supported event_ids of peer AVRCP Target
			esp_avrc_ct_send_get_rn_capabilities_cmd(APP_RC_CT_TL_GET_CAPS);
		} else {
			// clear peer notification capability record
			s_avrc_peer_rn_cap.bits = 0;
		}
		break;
	}
	case ESP_AVRC_CT_PASSTHROUGH_RSP_EVT: {
		ESP_LOGI(BT_RC_CT_TAG, "AVRC passthrough rsp: key_code 0x%x, key_state %d", rc->psth_rsp.key_code, rc->psth_rsp.key_state);
		break;
	}
	case ESP_AVRC_CT_METADATA_RSP_EVT: {
		ESP_LOGI(BT_RC_CT_TAG, "AVRC metadata rsp: attribute id 0x%x, %s", rc->meta_rsp.attr_id, rc->meta_rsp.attr_text);
		free(rc->meta_rsp.attr_text);
		break;
	}
	case ESP_AVRC_CT_CHANGE_NOTIFY_EVT: {
		ESP_LOGI(BT_RC_CT_TAG, "AVRC event notification: %d", rc->change_ntf.event_id);
		bt_av_notify_evt_handler(rc->change_ntf.event_id, &rc->change_ntf.event_parameter);
		break;
	}
	case ESP_AVRC_CT_REMOTE_FEATURES_EVT: {
		ESP_LOGI(BT_RC_CT_TAG, "AVRC remote features %x, TG features %x", rc->rmt_feats.feat_mask, rc->rmt_feats.tg_feat_flag);
		break;
	}
	case ESP_AVRC_CT_GET_RN_CAPABILITIES_RSP_EVT: {
		ESP_LOGI(BT_RC_CT_TAG, "remote rn_cap: count %d, bitmask 0x%x", rc->get_rn_caps_rsp.cap_count,
				 rc->get_rn_caps_rsp.evt_set.bits);
		s_avrc_peer_rn_cap.bits = rc->get_rn_caps_rsp.evt_set.bits;

		bt_av_volume_changed();
		break;
	}
	case ESP_AVRC_CT_SET_ABSOLUTE_VOLUME_RSP_EVT: {
		ESP_LOGI(BT_RC_CT_TAG, "Set absolute volume rsp: volume %d", rc->set_volume_rsp.volume);
		break;
	}

	default:
		ESP_LOGE(BT_RC_CT_TAG, "%s unhandled evt %d", __func__, event);
		break;
	}
}


bool init_bluetooth()
{
   

	/* create application task */
	bt_app_task_start_up();

	/* Bluetooth device name, connection mode and profile set up */
	bt_app_work_dispatch(bt_av_hdl_stack_evt, BT_APP_EVT_STACK_UP, NULL, 0, NULL);

#if (CONFIG_BT_SSP_ENABLED == true)
	/* Set default parameters for Secure Simple Pairing */
	esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
	esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO;
	esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));
#endif

	/*
	 * Set default parameters for Legacy Pairing
	 * Use variable pin, input pin code when pairing
	 */
	esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_VARIABLE;
	esp_bt_pin_code_t pin_code;
	esp_bt_gap_set_pin(pin_type, 0, pin_code);	

    return true;
}

bool BT_a2dp::connect_bluetooth(const char * speaker_name)
{ 
	f_discover = false;
	bt_speaker_name = speaker_name;
	return init_bluetooth();
}


bool BT_a2dp::connect_bluetooth(esp_bd_addr_t speaker_address)
{
	f_discover = false;
	for (int i = 0; i < ESP_BD_ADDR_LEN; i++)
	{
		s_peer_bda[i] = speaker_address[i];
	}

	return init_bluetooth();

}

bool BT_a2dp::discover_sinks(esp_a2d_found_devices_cb_t callback)
{

	found_devices_cb = callback;



	f_discover = true;
	found_device_count = 0;

// memcpy(found_devices[0].name, "one", 4);
// memcpy(found_devices[1].name, "two", 4);
// memcpy(found_devices[2].name, "three ", 6);
// memcpy(found_devices[3].name, "four", 5);
// memcpy(found_devices[4].name, "five", 5);
// memcpy(found_devices[5].name, "six", 4);
// memcpy(found_devices[6].name, "seven", 6);
// memcpy(found_devices[7].name, "eight", 6);

// found_device_count = 8;
// found_devices_cb(found_devices, found_device_count);

	return init_bluetooth();

}

void BT_a2dp::shut_down()
{
	esp_bt_gap_cancel_discovery();
	// bt_app_task_shut_down();
}

int BT_a2dp::get_media_state()
{
	return s_media_state;
}

int BT_a2dp::get_a2d_state()
{
	return s_a2d_state;
}

nvs_handle_t BT_a2dp::get_nvs_handle()
{
	return bt_nvs_handle;
}

//------END BLUETOOTH-----
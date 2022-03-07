#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOSConfig.h"
/* BLE */
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "ble_codelock.h"
#include "gatt_svr.h"

static const char *TAG = "BLE_CODELOCK";

#define MAX_CODE_LENGTH 24
// code and input_code are not null terminated
static char code[MAX_CODE_LENGTH];
static size_t code_length;

static void (*on_success_callback) (void) = NULL;
static void (*on_failure_callback) (void) = NULL;

// Setters
void ble_codelock_set_code(char *code_p)
{
    // Generating code
    code_length = strlen(code_p);
    strncpy(code, code_p, code_length);
}
void ble_codelock_set_on_success_callback(void (*callback) (void))
{
    on_success_callback = callback;
}
void ble_codelock_set_on_failure_callback(void (*callback) (void))
{
    on_failure_callback = callback;
}


static uint16_t conn_handle;
static const char *device_name = "ble_codelock";
static int gap_event(struct ble_gap_event *event, void *arg);
static uint8_t addr_type;


/*
 * Enables advertising with parameters:
 *     o General discoverable mode
 *     o Undirected connectable mode
 */
static void advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    /*
     *  Set the advertisement data included in our advertisements:
     *     o Flags (indicates advertisement type and other general info)
     *     o Advertising tx power
     *     o Device name
     */
    memset(&fields, 0, sizeof(fields));

    /*
     * Advertise two flags:
     *      o Discoverability in forthcoming advertisement (general)
     *      o BLE-only (BR/EDR unsupported)
     */
    fields.flags = BLE_HS_ADV_F_DISC_GEN |
                   BLE_HS_ADV_F_BREDR_UNSUP;

    /*
     * Indicate that the TX power level field should be included; have the
     * stack fill this value automatically.  This is done by assigning the
     * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    fields.name = (uint8_t *)device_name;
    fields.name_len = strlen(device_name);
    fields.name_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    // Begin advertising
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, gap_event, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error enabling advertisement; rc=%d\n", rc);
        return;
    }
}

static int gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        // A new connection was established or a connection attempt failed
        MODLOG_DFLT(INFO, "connection %s; status=%d\n",
                    event->connect.status == 0 ? "established" : "failed",
                    event->connect.status);

        if (event->connect.status != 0) {
            // Connection failed; resume advertising
            advertise();
        }
        conn_handle = event->connect.conn_handle;
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        MODLOG_DFLT(INFO, "disconnect; reason=%d\n", event->disconnect.reason);
        // Connection terminated; resume advertising
        advertise();
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        MODLOG_DFLT(INFO, "adv complete\n");
        advertise();
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        MODLOG_DFLT(INFO, "subscribe event; cur_notify=%d\n value handle; "
                    "val_handle=%d\n",
                    event->subscribe.cur_notify, hrm_handle);
        break;

    case BLE_GAP_EVENT_MTU:
        MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.value);
        break;

    }

    return 0;
}

static void on_sync(void)
{
    int rc = ble_hs_id_infer_auto(0, &addr_type);
    assert(rc == 0);

    MODLOG_DFLT(INFO, "Syncing state");

    // Begin advertising
    advertise();
}

static void on_reset(int reason)
{
    MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

void host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task Started");
    // This function will return only when nimble_port_stop() is executed
    nimble_port_run();

    nimble_port_freertos_deinit();
}


void on_code_received(char *received_code, uint16_t len) {
    ESP_LOGI(TAG, "received_code = %s, len = %u", received_code, len);
    if (len == code_length && strncmp(code, received_code, code_length) == 0) {
        if (on_success_callback)
            on_success_callback();
    }
    else {
        if (on_failure_callback)
            on_failure_callback();
    }
}


void init_ble_codelock(char *code)
{
    ble_codelock_set_code(code);

    ESP_ERROR_CHECK(esp_nimble_hci_and_controller_init());

    nimble_port_init();
    // Initialize the NimBLE host configuration
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.reset_cb = on_reset;

    int rc = gatt_svr_init();
    assert(rc == 0);

    // Set the default device name
    rc = ble_svc_gap_device_name_set(device_name);
    assert(rc == 0);

    // Start the task
    nimble_port_freertos_init(host_task);
}

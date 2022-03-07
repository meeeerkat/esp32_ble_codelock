#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "gatt_svr.h"


static const char *manuf_name = "Apache Mynewt ESP32 devkitC";
static const char *model_num = "";


/* 5c3a659e-897e-45e1-b016-007107c96df6 */
static const ble_uuid128_t GATT_SVR_UUID =
    BLE_UUID128_INIT(0xf6, 0x6d, 0xc9, 0x07, 0x71, 0x00, 0x16, 0xb0,
                     0xe1, 0x45, 0x7e, 0x89, 0x9e, 0x65, 0x3a, 0x5c);
#define GATT_CHR_CODE_LENGTH    24
static const ble_uuid16_t GATT_CHR_CODE_UUID =
    BLE_UUID16_INIT(0x2af6);
#define GATT_DEVICE_INFO_UUID               0x180A
#define GATT_MANUFACTURER_NAME_UUID         0x2A29
#define GATT_MODEL_NUMBER_UUID              0x2A24


uint16_t code_handle;

static int gatt_svr_chr_access_code(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);

static int gatt_svr_chr_access_device_info(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg);


static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        // Custom service
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &GATT_SVR_UUID.u,
        .characteristics = (struct ble_gatt_chr_def[])
        { {
                // Characteristic: code
                .uuid = &(GATT_CHR_CODE_UUID.u),
                .access_cb = gatt_svr_chr_access_code,
                .val_handle = &code_handle,
                .flags = BLE_GATT_CHR_F_WRITE,
            }, {
                0, // No more characteristics in this service
            },
        }
    },

    {
        // Device Information service
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(GATT_DEVICE_INFO_UUID),
        .characteristics = (struct ble_gatt_chr_def[])
        { {
                // Characteristic: Manufacturer name
                .uuid = BLE_UUID16_DECLARE(GATT_MANUFACTURER_NAME_UUID),
                .access_cb = gatt_svr_chr_access_device_info,
                .flags = BLE_GATT_CHR_F_READ,
            }, {
                // Characteristic: Model number string
                .uuid = BLE_UUID16_DECLARE(GATT_MODEL_NUMBER_UUID),
                .access_cb = gatt_svr_chr_access_device_info,
                .flags = BLE_GATT_CHR_F_READ,
            }, {
                0, // No more characteristics in this service
            },
        }
    },

    {
        0, // No more services
    },
};



static int gatt_svr_chr_write(struct os_mbuf *om, uint16_t min_len, uint16_t max_len,
                   void *dst, uint16_t *len)
{
    uint16_t om_len = OS_MBUF_PKTLEN(om);
    if (om_len < min_len || om_len > max_len)
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;

    int rc = ble_hs_mbuf_to_flat(om, dst, max_len, len);
    if (rc != 0)
        return BLE_ATT_ERR_UNLIKELY;

    return 0;
}


static char received_code[GATT_CHR_CODE_LENGTH];
static int gatt_svr_chr_access_code(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    const ble_uuid_t *uuid = ctxt->chr->uuid;

    // There is only one characteristic to handle here (code)
    // and it's write only
    assert (ble_uuid_cmp(uuid, &(GATT_CHR_CODE_UUID.u)) == 0
            && ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR);

    uint16_t len;
    int rc = gatt_svr_chr_write(ctxt->om,
                            1,
                            GATT_CHR_CODE_LENGTH,
                            received_code, &len);
    if (rc != 0)
        return rc;

    on_code_received(received_code, len);
    return 0;
}

static int gatt_svr_chr_access_device_info(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint16_t uuid = ble_uuid_u16(ctxt->chr->uuid);

    if (uuid == GATT_MODEL_NUMBER_UUID) {
        int rc = os_mbuf_append(ctxt->om, model_num, strlen(model_num));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    if (uuid == GATT_MANUFACTURER_NAME_UUID) {
        int rc = os_mbuf_append(ctxt->om, manuf_name, strlen(manuf_name));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    assert(0);
    return BLE_ATT_ERR_UNLIKELY;
}

void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        MODLOG_DFLT(DEBUG, "registered service %s with handle=%d\n",
                    ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                    ctxt->svc.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        MODLOG_DFLT(DEBUG, "registering characteristic %s with "
                    "def_handle=%d val_handle=%d\n",
                    ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                    ctxt->chr.def_handle,
                    ctxt->chr.val_handle);
        break;

    case BLE_GATT_REGISTER_OP_DSC:
        MODLOG_DFLT(DEBUG, "registering descriptor %s with handle=%d\n",
                    ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                    ctxt->dsc.handle);
        break;

    default:
        assert(0);
        break;
    }
}

int gatt_svr_init()
{
    int rc;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

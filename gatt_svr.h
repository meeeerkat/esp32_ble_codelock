#ifndef _GATT_SVR_H_
#define _GATT_SVR_H

#include "nimble/ble.h"
#include "modlog/modlog.h"

#ifdef __cplusplus
extern "C" {
#endif

uint16_t hrm_handle;

struct ble_hs_cfg;
struct ble_gatt_register_ctxt;

void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);

int gatt_svr_init();

void on_code_received(char *received_code, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif

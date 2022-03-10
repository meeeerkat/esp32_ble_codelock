#ifndef _BLE_CODELOCK_H_
#define _BLE_CODELOCK_H

#include "nimble/ble.h"
#include "modlog/modlog.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int init_ble_codelock(char *code);

// Setters
extern int ble_codelock_set_code(char *code_p);
extern void ble_codelock_set_on_success_callback(void (*callback) (void));
extern void ble_codelock_set_on_failure_callback(void (*callback) (void));

#ifdef __cplusplus
}
#endif

#endif

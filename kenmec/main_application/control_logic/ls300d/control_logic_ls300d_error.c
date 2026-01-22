/**
 * @file control_logic_ls300d_error.c
 * @brief LS300D 機型錯誤碼處理實作
 */

#include "control_logic_ls300d_error.h"
#include <stdio.h>
#include <string.h>

/* ========== 預設配置檔路徑 ========== */

#define LS300D_ERROR_CONFIG_DEFAULT_PATH \
    "/etc/kenmec/ls300d_error_config.json"

/* ========== 感測值獲取函數宣告 ========== */
/* 這些函數需要在 control_logic_ls300d 模組中實作 */

/* 壓力計讀值 */
extern float control_logic_ls300d_get_p1_value(void) __attribute__((weak));
extern float control_logic_ls300d_get_p2_value(void) __attribute__((weak));
extern float control_logic_ls300d_get_p3_value(void) __attribute__((weak));
extern float control_logic_ls300d_get_p4_value(void) __attribute__((weak));
extern float control_logic_ls300d_get_p5_value(void) __attribute__((weak));

/* 溫度計讀值 */
extern float control_logic_ls300d_get_t0_value(void) __attribute__((weak));
extern float control_logic_ls300d_get_t1_value(void) __attribute__((weak));
extern float control_logic_ls300d_get_t2_value(void) __attribute__((weak));
extern float control_logic_ls300d_get_t3_value(void) __attribute__((weak));
extern float control_logic_ls300d_get_t4_value(void) __attribute__((weak));

/* 流量計讀值 */
extern float control_logic_ls300d_get_fp_value(void) __attribute__((weak));
extern float control_logic_ls300d_get_fs_value(void) __attribute__((weak));

/* 濕度計讀值 */
extern float control_logic_ls300d_get_rh_value(void) __attribute__((weak));

/* 露點溫度讀值 */
extern float control_logic_ls300d_get_dewpoint_value(void) __attribute__((weak));

/* 通訊狀態檢查 */
extern int control_logic_ls300d_check_p1a_comm(void) __attribute__((weak));
extern int control_logic_ls300d_check_p1b_comm(void) __attribute__((weak));
extern int control_logic_ls300d_check_p2a_comm(void) __attribute__((weak));
extern int control_logic_ls300d_check_p2b_comm(void) __attribute__((weak));
extern int control_logic_ls300d_check_p3a_comm(void) __attribute__((weak));
extern int control_logic_ls300d_check_p3b_comm(void) __attribute__((weak));
extern int control_logic_ls300d_check_p4a_comm(void) __attribute__((weak));
extern int control_logic_ls300d_check_p4b_comm(void) __attribute__((weak));
extern int control_logic_ls300d_check_p5a_comm(void) __attribute__((weak));
extern int control_logic_ls300d_check_p5b_comm(void) __attribute__((weak));
extern int control_logic_ls300d_check_t1a_comm(void) __attribute__((weak));
extern int control_logic_ls300d_check_t1b_comm(void) __attribute__((weak));
extern int control_logic_ls300d_check_t2a_comm(void) __attribute__((weak));
extern int control_logic_ls300d_check_t2b_comm(void) __attribute__((weak));
extern int control_logic_ls300d_check_t3a_comm(void) __attribute__((weak));
extern int control_logic_ls300d_check_t3b_comm(void) __attribute__((weak));
extern int control_logic_ls300d_check_t4a_comm(void) __attribute__((weak));
extern int control_logic_ls300d_check_t4b_comm(void) __attribute__((weak));
extern int control_logic_ls300d_check_fp_comm(void) __attribute__((weak));
extern int control_logic_ls300d_check_fs_comm(void) __attribute__((weak));
extern int control_logic_ls300d_check_ls5a_comm(void) __attribute__((weak));
extern int control_logic_ls300d_check_ls5b_comm(void) __attribute__((weak));
extern int control_logic_ls300d_check_cv1_comm(void) __attribute__((weak));
extern int control_logic_ls300d_check_pump1_comm(void) __attribute__((weak));
extern int control_logic_ls300d_check_pump2_comm(void) __attribute__((weak));
extern int control_logic_ls300d_check_leak_sensor_comm(void) __attribute__((weak));
extern int control_logic_ls300d_check_th_sensor_comm(void) __attribute__((weak));
extern int control_logic_ls300d_check_refill_pump_comm(void) __attribute__((weak));

/* 液位開關狀態 */
extern int control_logic_ls300d_check_ls5a_status(void) __attribute__((weak));
extern int control_logic_ls300d_check_ls5b_status(void) __attribute__((weak));

/* 漏液檢測 */
extern int control_logic_ls300d_check_leak(void) __attribute__((weak));

/* 泵浦故障檢測 */
extern int control_logic_ls300d_check_pump1_fault(void) __attribute__((weak));
extern int control_logic_ls300d_check_pump2_fault(void) __attribute__((weak));

/* ========== 預設值函數 (當 weak 函數未定義時使用) ========== */

static float default_get_value(void) {
    return 0.0f;
}

static int default_check_comm(void) {
    return 0;  /* 0 = 正常 */
}

/* ========== 動作處理函數 ========== */

/**
 * @brief 停止水泵的動作處理
 */
static void action_pump_stop(uint32_t code, error_state_t new_state) {
    if (new_state == ERROR_STATE_ACTIVE) {
        printf("[LS300D_ERROR] Action: Stop pump due to error %u\n", code);
        /* TODO: 呼叫 control_logic 停止水泵 */
    }
}

/**
 * @brief 水泵降轉的動作處理
 */
static void action_pump_slow(uint32_t code, error_state_t new_state) {
    if (new_state == ERROR_STATE_ACTIVE) {
        printf("[LS300D_ERROR] Action: Slow pump to 30%% due to error %u\n", code);
        /* TODO: 呼叫 control_logic 設定水泵轉速為 30% */
    }
}

/**
 * @brief 開啟比例閥到最大的動作處理
 */
static void action_valve_full_open(uint32_t code, error_state_t new_state) {
    if (new_state == ERROR_STATE_ACTIVE) {
        printf("[LS300D_ERROR] Action: Open valve to maximum due to error %u\n", code);
        /* TODO: 呼叫 control_logic 設定比例閥開度為最大 */
    }
}

/* ========== 組合條件檢查函數 ========== */

/**
 * @brief 檢查 P1 通訊異常 (P1a 和 P1b 同時異常)
 */
static int check_p1_comm_error(void) {
    int p1a = control_logic_ls300d_check_p1a_comm ?
              control_logic_ls300d_check_p1a_comm() : 0;
    int p1b = control_logic_ls300d_check_p1b_comm ?
              control_logic_ls300d_check_p1b_comm() : 0;
    return (p1a != 0 && p1b != 0) ? 1 : 0;
}

/**
 * @brief 檢查 P2 通訊異常
 */
static int check_p2_comm_error(void) {
    int p2a = control_logic_ls300d_check_p2a_comm ?
              control_logic_ls300d_check_p2a_comm() : 0;
    int p2b = control_logic_ls300d_check_p2b_comm ?
              control_logic_ls300d_check_p2b_comm() : 0;
    return (p2a != 0 && p2b != 0) ? 1 : 0;
}

/**
 * @brief 檢查 P3 通訊異常
 */
static int check_p3_comm_error(void) {
    int p3a = control_logic_ls300d_check_p3a_comm ?
              control_logic_ls300d_check_p3a_comm() : 0;
    int p3b = control_logic_ls300d_check_p3b_comm ?
              control_logic_ls300d_check_p3b_comm() : 0;
    return (p3a != 0 && p3b != 0) ? 1 : 0;
}

/**
 * @brief 檢查 P4 通訊異常
 */
static int check_p4_comm_error(void) {
    int p4a = control_logic_ls300d_check_p4a_comm ?
              control_logic_ls300d_check_p4a_comm() : 0;
    int p4b = control_logic_ls300d_check_p4b_comm ?
              control_logic_ls300d_check_p4b_comm() : 0;
    return (p4a != 0 && p4b != 0) ? 1 : 0;
}

/**
 * @brief 檢查 P5 通訊異常
 */
static int check_p5_comm_error(void) {
    int p5a = control_logic_ls300d_check_p5a_comm ?
              control_logic_ls300d_check_p5a_comm() : 0;
    int p5b = control_logic_ls300d_check_p5b_comm ?
              control_logic_ls300d_check_p5b_comm() : 0;
    return (p5a != 0 && p5b != 0) ? 1 : 0;
}

/**
 * @brief 檢查 T1 通訊異常
 */
static int check_t1_comm_error(void) {
    int t1a = control_logic_ls300d_check_t1a_comm ?
              control_logic_ls300d_check_t1a_comm() : 0;
    int t1b = control_logic_ls300d_check_t1b_comm ?
              control_logic_ls300d_check_t1b_comm() : 0;
    return (t1a != 0 && t1b != 0) ? 1 : 0;
}

/**
 * @brief 檢查 T2 通訊異常
 */
static int check_t2_comm_error(void) {
    int t2a = control_logic_ls300d_check_t2a_comm ?
              control_logic_ls300d_check_t2a_comm() : 0;
    int t2b = control_logic_ls300d_check_t2b_comm ?
              control_logic_ls300d_check_t2b_comm() : 0;
    return (t2a != 0 && t2b != 0) ? 1 : 0;
}

/**
 * @brief 檢查 T3 通訊異常
 */
static int check_t3_comm_error(void) {
    int t3a = control_logic_ls300d_check_t3a_comm ?
              control_logic_ls300d_check_t3a_comm() : 0;
    int t3b = control_logic_ls300d_check_t3b_comm ?
              control_logic_ls300d_check_t3b_comm() : 0;
    return (t3a != 0 && t3b != 0) ? 1 : 0;
}

/**
 * @brief 檢查 T4 通訊異常
 */
static int check_t4_comm_error(void) {
    int t4a = control_logic_ls300d_check_t4a_comm ?
              control_logic_ls300d_check_t4a_comm() : 0;
    int t4b = control_logic_ls300d_check_t4b_comm ?
              control_logic_ls300d_check_t4b_comm() : 0;
    return (t4a != 0 && t4b != 0) ? 1 : 0;
}

/**
 * @brief 檢查 LS5 液位開關異常 (LS5a 和 LS5b 同時觸發)
 */
static int check_ls5_level_low(void) {
    int ls5a = control_logic_ls300d_check_ls5a_status ?
               control_logic_ls300d_check_ls5a_status() : 0;
    int ls5b = control_logic_ls300d_check_ls5b_status ?
               control_logic_ls300d_check_ls5b_status() : 0;
    return (ls5a != 0 && ls5b != 0) ? 1 : 0;
}

/**
 * @brief 檢查 LS5 通訊異常
 */
static int check_ls5_comm_error(void) {
    int ls5a = control_logic_ls300d_check_ls5a_comm ?
               control_logic_ls300d_check_ls5a_comm() : 0;
    int ls5b = control_logic_ls300d_check_ls5b_comm ?
               control_logic_ls300d_check_ls5b_comm() : 0;
    return (ls5a != 0 && ls5b != 0) ? 1 : 0;
}

/**
 * @brief 檢查所有泵浦故障
 */
static int check_all_pump_fault(void) {
    int pump1 = control_logic_ls300d_check_pump1_fault ?
                control_logic_ls300d_check_pump1_fault() : 0;
    int pump2 = control_logic_ls300d_check_pump2_fault ?
                control_logic_ls300d_check_pump2_fault() : 0;
    return (pump1 != 0 && pump2 != 0) ? 1 : 0;
}

/**
 * @brief 檢查所有泵浦通訊異常
 */
static int check_all_pump_comm_error(void) {
    int pump1 = control_logic_ls300d_check_pump1_comm ?
                control_logic_ls300d_check_pump1_comm() : 0;
    int pump2 = control_logic_ls300d_check_pump2_comm ?
                control_logic_ls300d_check_pump2_comm() : 0;
    return (pump1 != 0 && pump2 != 0) ? 1 : 0;
}

/* ========== 獲取感測值的包裝函數 ========== */

static float get_p1_value(void) {
    return control_logic_ls300d_get_p1_value ?
           control_logic_ls300d_get_p1_value() : default_get_value();
}

static float get_p2_value(void) {
    return control_logic_ls300d_get_p2_value ?
           control_logic_ls300d_get_p2_value() : default_get_value();
}

static float get_p3_value(void) {
    return control_logic_ls300d_get_p3_value ?
           control_logic_ls300d_get_p3_value() : default_get_value();
}

static float get_p4_value(void) {
    return control_logic_ls300d_get_p4_value ?
           control_logic_ls300d_get_p4_value() : default_get_value();
}

static float get_p5_value(void) {
    return control_logic_ls300d_get_p5_value ?
           control_logic_ls300d_get_p5_value() : default_get_value();
}

static float get_t0_value(void) {
    return control_logic_ls300d_get_t0_value ?
           control_logic_ls300d_get_t0_value() : default_get_value();
}

static float get_t1_value(void) {
    return control_logic_ls300d_get_t1_value ?
           control_logic_ls300d_get_t1_value() : default_get_value();
}

static float get_t2_value(void) {
    return control_logic_ls300d_get_t2_value ?
           control_logic_ls300d_get_t2_value() : default_get_value();
}

static float get_t3_value(void) {
    return control_logic_ls300d_get_t3_value ?
           control_logic_ls300d_get_t3_value() : default_get_value();
}

static float get_t4_value(void) {
    return control_logic_ls300d_get_t4_value ?
           control_logic_ls300d_get_t4_value() : default_get_value();
}

static float get_rh_value(void) {
    return control_logic_ls300d_get_rh_value ?
           control_logic_ls300d_get_rh_value() : default_get_value();
}

static float get_dewpoint_value(void) {
    return control_logic_ls300d_get_dewpoint_value ?
           control_logic_ls300d_get_dewpoint_value() : default_get_value();
}

/* ========== 錯誤碼定義表 ========== */

static error_definition_t g_ls300d_error_defs[] = {
    /* ===== 一次側入水管路 P1 壓力計 ===== */
    {
        .code = LS300D_ERR_P1_HIGH_ALERT,
        .name = "P1_HIGH_ALERT",
        .message = "P1壓力計高於警告值(alert)",
        .severity = ERROR_SEVERITY_CRITICAL,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_HIGH,
        .threshold = 6.0f,
        .hysteresis = 0.3f,  /* 5.7 = 6.0 - 0.3 */
        .delay_ms = 60000,   /* 1 min */
        .enabled = true,
        .get_value = get_p1_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_P1_HIGH_WARNING,
        .name = "P1_HIGH_WARNING",
        .message = "P1壓力計高於警報值(warning)",
        .severity = ERROR_SEVERITY_MINOR,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_HIGH,
        .threshold = 5.0f,
        .hysteresis = 0.3f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_p1_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_P1_LOW_WARNING,
        .name = "P1_LOW_WARNING",
        .message = "P1壓力計低於警報值(warning)",
        .severity = ERROR_SEVERITY_MINOR,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_LOW,
        .threshold = 0.0f,
        .hysteresis = 0.2f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_p1_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_P1_LOW_ALERT,
        .name = "P1_LOW_ALERT",
        .message = "P1壓力計低於警告值(alert)",
        .severity = ERROR_SEVERITY_CRITICAL,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_LOW,
        .threshold = -0.3f,
        .hysteresis = 0.3f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_p1_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_P1_COMM_ERROR,
        .name = "P1_COMM_ERROR",
        .message = "P1壓力計通訊異常",
        .severity = ERROR_SEVERITY_MAJOR,
        .recovery = ERROR_RECOVERY_MANUAL,
        .cond_type = ERROR_COND_CUSTOM,
        .threshold = 0,
        .hysteresis = 0,
        .delay_ms = 0,
        .enabled = true,
        .get_value = NULL,
        .condition_check = check_p1_comm_error,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_P1A_COMM_ERROR,
        .name = "P1A_COMM_ERROR",
        .message = "P1a壓力計通訊異常",
        .severity = ERROR_SEVERITY_MINOR,
        .recovery = ERROR_RECOVERY_MANUAL,
        .cond_type = ERROR_COND_CUSTOM,
        .threshold = 0,
        .hysteresis = 0,
        .delay_ms = 0,
        .enabled = true,
        .get_value = NULL,
        .condition_check = (int (*)(void))control_logic_ls300d_check_p1a_comm,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_P1B_COMM_ERROR,
        .name = "P1B_COMM_ERROR",
        .message = "P1b壓力計通訊異常",
        .severity = ERROR_SEVERITY_MINOR,
        .recovery = ERROR_RECOVERY_MANUAL,
        .cond_type = ERROR_COND_CUSTOM,
        .threshold = 0,
        .hysteresis = 0,
        .delay_ms = 0,
        .enabled = true,
        .get_value = NULL,
        .condition_check = (int (*)(void))control_logic_ls300d_check_p1b_comm,
        .action_handler = NULL
    },

    /* ===== 一次側入水管路 T1 溫度計 ===== */
    {
        .code = LS300D_ERR_T1_HIGH_ALERT,
        .name = "T1_HIGH_ALERT",
        .message = "T1溫度計高於警告值(alert) - CDU元件耐溫極限",
        .severity = ERROR_SEVERITY_CRITICAL,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_HIGH,
        .threshold = 80.0f,
        .hysteresis = 2.0f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_t1_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_T1_HIGH_WARNING,
        .name = "T1_HIGH_WARNING",
        .message = "T1溫度計高於警報值(warning)",
        .severity = ERROR_SEVERITY_MINOR,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_HIGH,
        .threshold = 45.0f,
        .hysteresis = 2.0f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_t1_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_T1_LOW_WARNING,
        .name = "T1_LOW_WARNING",
        .message = "T1溫度計低於警報值(warning)",
        .severity = ERROR_SEVERITY_MINOR,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_LOW,
        .threshold = 5.0f,
        .hysteresis = 2.0f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_t1_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_T1_LOW_ALERT,
        .name = "T1_LOW_ALERT",
        .message = "T1溫度計低於警告值(alert)",
        .severity = ERROR_SEVERITY_CRITICAL,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_LOW,
        .threshold = 2.0f,
        .hysteresis = 2.0f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_t1_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_T1_COMM_ERROR,
        .name = "T1_COMM_ERROR",
        .message = "T1溫度計通訊異常",
        .severity = ERROR_SEVERITY_MAJOR,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_CUSTOM,
        .threshold = 0,
        .hysteresis = 0,
        .delay_ms = 0,
        .enabled = true,
        .get_value = NULL,
        .condition_check = check_t1_comm_error,
        .action_handler = NULL
    },

    /* ===== 一次側入水管路 FP 流量計 ===== */
    {
        .code = LS300D_ERR_FP_COMM_ERROR,
        .name = "FP_COMM_ERROR",
        .message = "FP流量計通訊異常",
        .severity = ERROR_SEVERITY_MINOR,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_CUSTOM,
        .threshold = 0,
        .hysteresis = 0,
        .delay_ms = 0,
        .enabled = true,
        .get_value = NULL,
        .condition_check = (int (*)(void))control_logic_ls300d_check_fp_comm,
        .action_handler = NULL
    },

    /* ===== 一次側出水管路 P3 壓力計 ===== */
    {
        .code = LS300D_ERR_P3_HIGH_ALERT,
        .name = "P3_HIGH_ALERT",
        .message = "P3壓力計高於警告值(alert)",
        .severity = ERROR_SEVERITY_CRITICAL,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_HIGH,
        .threshold = 6.0f,
        .hysteresis = 0.3f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_p3_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_P3_HIGH_WARNING,
        .name = "P3_HIGH_WARNING",
        .message = "P3壓力計高於警報值(warning)",
        .severity = ERROR_SEVERITY_MINOR,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_HIGH,
        .threshold = 5.0f,
        .hysteresis = 0.3f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_p3_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_P3_LOW_WARNING,
        .name = "P3_LOW_WARNING",
        .message = "P3壓力計低於警報值(warning)",
        .severity = ERROR_SEVERITY_MINOR,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_LOW,
        .threshold = 0.0f,
        .hysteresis = 0.2f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_p3_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_P3_LOW_ALERT,
        .name = "P3_LOW_ALERT",
        .message = "P3壓力計低於警告值(alert)",
        .severity = ERROR_SEVERITY_CRITICAL,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_LOW,
        .threshold = -0.3f,
        .hysteresis = 0.3f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_p3_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_P3_COMM_ERROR,
        .name = "P3_COMM_ERROR",
        .message = "P3壓力計通訊異常",
        .severity = ERROR_SEVERITY_MAJOR,
        .recovery = ERROR_RECOVERY_MANUAL,
        .cond_type = ERROR_COND_CUSTOM,
        .threshold = 0,
        .hysteresis = 0,
        .delay_ms = 0,
        .enabled = true,
        .get_value = NULL,
        .condition_check = check_p3_comm_error,
        .action_handler = NULL
    },

    /* ===== 一次側出水管路 T3 溫度計 ===== */
    {
        .code = LS300D_ERR_T3_HIGH_ALERT,
        .name = "T3_HIGH_ALERT",
        .message = "T3溫度計高於警告值(alert)",
        .severity = ERROR_SEVERITY_CRITICAL,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_HIGH,
        .threshold = 80.0f,
        .hysteresis = 2.0f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_t3_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_T3_HIGH_WARNING,
        .name = "T3_HIGH_WARNING",
        .message = "T3溫度計高於警報值(warning)",
        .severity = ERROR_SEVERITY_MINOR,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_HIGH,
        .threshold = 45.0f,
        .hysteresis = 2.0f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_t3_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_T3_LOW_WARNING,
        .name = "T3_LOW_WARNING",
        .message = "T3溫度計低於警報值(warning)",
        .severity = ERROR_SEVERITY_MINOR,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_LOW,
        .threshold = 5.0f,
        .hysteresis = 2.0f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_t3_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_T3_LOW_ALERT,
        .name = "T3_LOW_ALERT",
        .message = "T3溫度計低於警告值(alert)",
        .severity = ERROR_SEVERITY_CRITICAL,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_LOW,
        .threshold = 2.0f,
        .hysteresis = 2.0f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_t3_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_T3_COMM_ERROR,
        .name = "T3_COMM_ERROR",
        .message = "T3溫度計通訊異常",
        .severity = ERROR_SEVERITY_MAJOR,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_CUSTOM,
        .threshold = 0,
        .hysteresis = 0,
        .delay_ms = 0,
        .enabled = true,
        .get_value = NULL,
        .condition_check = check_t3_comm_error,
        .action_handler = NULL
    },

    /* ===== 一次側出水管路 CV1 控制閥 ===== */
    {
        .code = LS300D_ERR_CV1_COMM_ERROR,
        .name = "CV1_COMM_ERROR",
        .message = "控制閥CV1通訊異常",
        .severity = ERROR_SEVERITY_CRITICAL,
        .recovery = ERROR_RECOVERY_MANUAL,
        .cond_type = ERROR_COND_CUSTOM,
        .threshold = 0,
        .hysteresis = 0,
        .delay_ms = 0,
        .enabled = true,
        .get_value = NULL,
        .condition_check = (int (*)(void))control_logic_ls300d_check_cv1_comm,
        .action_handler = NULL
    },

    /* ===== 二次側入水管路 P2 壓力計 ===== */
    {
        .code = LS300D_ERR_P2_HIGH_ALERT,
        .name = "P2_HIGH_ALERT",
        .message = "P2壓力計高於警告值(alert)",
        .severity = ERROR_SEVERITY_CRITICAL,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_HIGH,
        .threshold = 6.0f,
        .hysteresis = 0.3f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_p2_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_P2_HIGH_WARNING,
        .name = "P2_HIGH_WARNING",
        .message = "P2壓力計高於警報值(warning)",
        .severity = ERROR_SEVERITY_MINOR,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_HIGH,
        .threshold = 5.0f,
        .hysteresis = 0.3f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_p2_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_P2_LOW_WARNING,
        .name = "P2_LOW_WARNING",
        .message = "P2壓力計低於警報值(warning)",
        .severity = ERROR_SEVERITY_MINOR,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_LOW,
        .threshold = 0.0f,
        .hysteresis = 0.2f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_p2_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_P2_LOW_ALERT,
        .name = "P2_LOW_ALERT",
        .message = "P2壓力計低於警告值(alert)",
        .severity = ERROR_SEVERITY_CRITICAL,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_LOW,
        .threshold = -0.3f,
        .hysteresis = 0.3f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_p2_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_P2_COMM_ERROR,
        .name = "P2_COMM_ERROR",
        .message = "P2壓力計通訊異常",
        .severity = ERROR_SEVERITY_CRITICAL,
        .recovery = ERROR_RECOVERY_MANUAL,
        .cond_type = ERROR_COND_CUSTOM,
        .threshold = 0,
        .hysteresis = 0,
        .delay_ms = 0,
        .enabled = true,
        .get_value = NULL,
        .condition_check = check_p2_comm_error,
        .action_handler = NULL
    },

    /* ===== 二次側入水管路 T2 溫度計 ===== */
    {
        .code = LS300D_ERR_T2_HIGH_ALERT,
        .name = "T2_HIGH_ALERT",
        .message = "T2溫度計高於警告值(alert)",
        .severity = ERROR_SEVERITY_CRITICAL,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_HIGH,
        .threshold = 80.0f,
        .hysteresis = 2.0f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_t2_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_T2_HIGH_WARNING,
        .name = "T2_HIGH_WARNING",
        .message = "T2溫度計高於警報值(warning)",
        .severity = ERROR_SEVERITY_MINOR,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_HIGH,
        .threshold = 45.0f,
        .hysteresis = 2.0f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_t2_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_T2_LOW_WARNING,
        .name = "T2_LOW_WARNING",
        .message = "T2溫度計低於警報值(warning)",
        .severity = ERROR_SEVERITY_MINOR,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_LOW,
        .threshold = 5.0f,
        .hysteresis = 2.0f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_t2_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_T2_LOW_ALERT,
        .name = "T2_LOW_ALERT",
        .message = "T2溫度計低於警告值(alert)",
        .severity = ERROR_SEVERITY_CRITICAL,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_LOW,
        .threshold = 2.0f,
        .hysteresis = 2.0f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_t2_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_T2_COMM_ERROR,
        .name = "T2_COMM_ERROR",
        .message = "T2溫度計通訊異常",
        .severity = ERROR_SEVERITY_MAJOR,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_CUSTOM,
        .threshold = 0,
        .hysteresis = 0,
        .delay_ms = 0,
        .enabled = true,
        .get_value = NULL,
        .condition_check = check_t2_comm_error,
        .action_handler = NULL
    },

    /* ===== 二次側水箱 P5 壓力計 ===== */
    {
        .code = LS300D_ERR_P5_HIGH_ALERT,
        .name = "P5_HIGH_ALERT",
        .message = "P5壓力計高於警告值(alert)",
        .severity = ERROR_SEVERITY_CRITICAL,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_HIGH,
        .threshold = 6.0f,
        .hysteresis = 0.3f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_p5_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_P5_HIGH_WARNING,
        .name = "P5_HIGH_WARNING",
        .message = "P5壓力計高於警報值(warning)",
        .severity = ERROR_SEVERITY_MINOR,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_HIGH,
        .threshold = 5.0f,
        .hysteresis = 0.3f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_p5_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_P5_LOW_WARNING,
        .name = "P5_LOW_WARNING",
        .message = "P5壓力計低於警報值(warning)",
        .severity = ERROR_SEVERITY_MINOR,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_LOW,
        .threshold = 0.0f,
        .hysteresis = 0.2f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_p5_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_P5_LOW_ALERT,
        .name = "P5_LOW_ALERT",
        .message = "P5壓力計低於警告值(alert)",
        .severity = ERROR_SEVERITY_CRITICAL,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_LOW,
        .threshold = -0.3f,
        .hysteresis = 0.3f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_p5_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_P5_COMM_ERROR,
        .name = "P5_COMM_ERROR",
        .message = "P5壓力計通訊異常",
        .severity = ERROR_SEVERITY_CRITICAL,
        .recovery = ERROR_RECOVERY_MANUAL,
        .cond_type = ERROR_COND_CUSTOM,
        .threshold = 0,
        .hysteresis = 0,
        .delay_ms = 0,
        .enabled = true,
        .get_value = NULL,
        .condition_check = check_p5_comm_error,
        .action_handler = NULL
    },

    /* ===== 二次側水箱 LS5 液位開關 ===== */
    {
        .code = LS300D_ERR_LS5_LEVEL_LOW,
        .name = "LS5_LEVEL_LOW",
        .message = "二次側水箱液位低於警告線，水泵降轉",
        .severity = ERROR_SEVERITY_CRITICAL,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_CUSTOM,
        .threshold = 0,
        .hysteresis = 0,
        .delay_ms = 10000,  /* 10 秒 */
        .enabled = true,
        .get_value = NULL,
        .condition_check = check_ls5_level_low,
        .action_handler = action_pump_slow
    },
    {
        .code = LS300D_ERR_LS5_COMM_ERROR,
        .name = "LS5_COMM_ERROR",
        .message = "LS5通訊異常",
        .severity = ERROR_SEVERITY_CRITICAL,
        .recovery = ERROR_RECOVERY_MANUAL,
        .cond_type = ERROR_COND_CUSTOM,
        .threshold = 0,
        .hysteresis = 0,
        .delay_ms = 0,
        .enabled = true,
        .get_value = NULL,
        .condition_check = check_ls5_comm_error,
        .action_handler = NULL
    },

    /* ===== 泵浦 ===== */
    {
        .code = LS300D_ERR_PUMP_ALL_FAULT,
        .name = "PUMP_ALL_FAULT",
        .message = "水泵1與水泵2皆故障停轉",
        .severity = ERROR_SEVERITY_CRITICAL,
        .recovery = ERROR_RECOVERY_MANUAL,
        .cond_type = ERROR_COND_CUSTOM,
        .threshold = 0,
        .hysteresis = 0,
        .delay_ms = 0,
        .enabled = true,
        .get_value = NULL,
        .condition_check = check_all_pump_fault,
        .action_handler = action_pump_stop
    },
    {
        .code = LS300D_ERR_PUMP_ALL_COMM_ERROR,
        .name = "PUMP_ALL_COMM_ERROR",
        .message = "水泵1與水泵2皆通訊異常",
        .severity = ERROR_SEVERITY_CRITICAL,
        .recovery = ERROR_RECOVERY_MANUAL,
        .cond_type = ERROR_COND_CUSTOM,
        .threshold = 0,
        .hysteresis = 0,
        .delay_ms = 0,
        .enabled = true,
        .get_value = NULL,
        .condition_check = check_all_pump_comm_error,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_PUMP1_FAULT,
        .name = "PUMP1_FAULT",
        .message = "水泵1異常",
        .severity = ERROR_SEVERITY_MAJOR,
        .recovery = ERROR_RECOVERY_MANUAL,
        .cond_type = ERROR_COND_CUSTOM,
        .threshold = 0,
        .hysteresis = 0,
        .delay_ms = 0,
        .enabled = true,
        .get_value = NULL,
        .condition_check = (int (*)(void))control_logic_ls300d_check_pump1_fault,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_PUMP2_FAULT,
        .name = "PUMP2_FAULT",
        .message = "水泵2異常",
        .severity = ERROR_SEVERITY_MAJOR,
        .recovery = ERROR_RECOVERY_MANUAL,
        .cond_type = ERROR_COND_CUSTOM,
        .threshold = 0,
        .hysteresis = 0,
        .delay_ms = 0,
        .enabled = true,
        .get_value = NULL,
        .condition_check = (int (*)(void))control_logic_ls300d_check_pump2_fault,
        .action_handler = NULL
    },

    /* ===== 二次側出水管路 P4 壓力計 ===== */
    {
        .code = LS300D_ERR_P4_HIGH_ALERT,
        .name = "P4_HIGH_ALERT",
        .message = "P4壓力計高於警告值(alert)",
        .severity = ERROR_SEVERITY_CRITICAL,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_HIGH,
        .threshold = 6.0f,
        .hysteresis = 0.3f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_p4_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_P4_HIGH_WARNING,
        .name = "P4_HIGH_WARNING",
        .message = "P4壓力計高於警報值(warning)",
        .severity = ERROR_SEVERITY_MINOR,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_HIGH,
        .threshold = 5.0f,
        .hysteresis = 0.3f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_p4_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_P4_LOW_WARNING,
        .name = "P4_LOW_WARNING",
        .message = "P4壓力計低於警報值(warning)",
        .severity = ERROR_SEVERITY_MINOR,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_LOW,
        .threshold = 0.0f,
        .hysteresis = 0.2f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_p4_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_P4_LOW_ALERT,
        .name = "P4_LOW_ALERT",
        .message = "P4壓力計低於警告值(alert)",
        .severity = ERROR_SEVERITY_CRITICAL,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_LOW,
        .threshold = -0.3f,
        .hysteresis = 0.3f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_p4_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_P4_COMM_ERROR,
        .name = "P4_COMM_ERROR",
        .message = "P4壓力計通訊異常",
        .severity = ERROR_SEVERITY_CRITICAL,
        .recovery = ERROR_RECOVERY_MANUAL,
        .cond_type = ERROR_COND_CUSTOM,
        .threshold = 0,
        .hysteresis = 0,
        .delay_ms = 0,
        .enabled = true,
        .get_value = NULL,
        .condition_check = check_p4_comm_error,
        .action_handler = NULL
    },

    /* ===== 二次側出水管路 T4 溫度計 ===== */
    {
        .code = LS300D_ERR_T4_HIGH_ALERT,
        .name = "T4_HIGH_ALERT",
        .message = "T4溫度計高於警告值(alert)",
        .severity = ERROR_SEVERITY_CRITICAL,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_HIGH,
        .threshold = 55.0f,
        .hysteresis = 2.0f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_t4_value,
        .condition_check = NULL,
        .action_handler = action_pump_stop
    },
    {
        .code = LS300D_ERR_T4_HIGH_WARNING,
        .name = "T4_HIGH_WARNING",
        .message = "T4溫度計高於警報值(warning)",
        .severity = ERROR_SEVERITY_MINOR,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_HIGH,
        .threshold = 50.0f,
        .hysteresis = 2.0f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_t4_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_T4_LOW_WARNING,
        .name = "T4_LOW_WARNING",
        .message = "T4溫度計低於警報值(warning)",
        .severity = ERROR_SEVERITY_MINOR,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_LOW,
        .threshold = 5.0f,
        .hysteresis = 2.0f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_t4_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_T4_LOW_ALERT,
        .name = "T4_LOW_ALERT",
        .message = "T4溫度計低於警告值(alert)",
        .severity = ERROR_SEVERITY_CRITICAL,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_LOW,
        .threshold = 2.0f,
        .hysteresis = 2.0f,
        .delay_ms = 60000,
        .enabled = true,
        .get_value = get_t4_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_T4_COMM_ERROR,
        .name = "T4_COMM_ERROR",
        .message = "T4溫度計通訊異常",
        .severity = ERROR_SEVERITY_CRITICAL,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_CUSTOM,
        .threshold = 0,
        .hysteresis = 0,
        .delay_ms = 0,
        .enabled = true,
        .get_value = NULL,
        .condition_check = check_t4_comm_error,
        .action_handler = NULL
    },

    /* ===== 二次側出水管路 FS 流量計 ===== */
    {
        .code = LS300D_ERR_FS_COMM_ERROR,
        .name = "FS_COMM_ERROR",
        .message = "FS流量計通訊異常",
        .severity = ERROR_SEVERITY_CRITICAL,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_CUSTOM,
        .threshold = 0,
        .hysteresis = 0,
        .delay_ms = 0,
        .enabled = true,
        .get_value = NULL,
        .condition_check = (int (*)(void))control_logic_ls300d_check_fs_comm,
        .action_handler = NULL
    },

    /* ===== 機箱內 - 漏液檢知帶 ===== */
    {
        .code = LS300D_ERR_LEAK_SENSOR_COMM_ERROR,
        .name = "LEAK_SENSOR_COMM_ERROR",
        .message = "漏液檢知帶通訊異常",
        .severity = ERROR_SEVERITY_MAJOR,
        .recovery = ERROR_RECOVERY_MANUAL,
        .cond_type = ERROR_COND_CUSTOM,
        .threshold = 0,
        .hysteresis = 0,
        .delay_ms = 0,
        .enabled = true,
        .get_value = NULL,
        .condition_check = (int (*)(void))control_logic_ls300d_check_leak_sensor_comm,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_LEAK_DETECTED,
        .name = "LEAK_DETECTED",
        .message = "CDU檢測到漏液",
        .severity = ERROR_SEVERITY_CRITICAL,
        .recovery = ERROR_RECOVERY_MANUAL,
        .cond_type = ERROR_COND_CUSTOM,
        .threshold = 0,
        .hysteresis = 0,
        .delay_ms = 0,
        .enabled = true,
        .get_value = NULL,
        .condition_check = (int (*)(void))control_logic_ls300d_check_leak,
        .action_handler = action_pump_stop
    },

    /* ===== 機箱內 - 溫濕度感應器 ===== */
    {
        .code = LS300D_ERR_TH_SENSOR_COMM_ERROR,
        .name = "TH_SENSOR_COMM_ERROR",
        .message = "溫濕度計通訊異常",
        .severity = ERROR_SEVERITY_MAJOR,
        .recovery = ERROR_RECOVERY_MANUAL,
        .cond_type = ERROR_COND_CUSTOM,
        .threshold = 0,
        .hysteresis = 0,
        .delay_ms = 0,
        .enabled = true,
        .get_value = NULL,
        .condition_check = (int (*)(void))control_logic_ls300d_check_th_sensor_comm,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_DEWPOINT_HIGH_ALERT,
        .name = "DEWPOINT_HIGH_ALERT",
        .message = "露點溫度高於警告值(alert)",
        .severity = ERROR_SEVERITY_CRITICAL,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_HIGH,
        .threshold = 25.0f,
        .hysteresis = 2.0f,
        .delay_ms = 0,
        .enabled = true,
        .get_value = get_dewpoint_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_DEWPOINT_HIGH_WARNING,
        .name = "DEWPOINT_HIGH_WARNING",
        .message = "露點溫度高於警報值(warning)",
        .severity = ERROR_SEVERITY_MAJOR,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_HIGH,
        .threshold = 20.0f,
        .hysteresis = 2.0f,
        .delay_ms = 0,
        .enabled = true,
        .get_value = get_dewpoint_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_T0_HIGH_ALERT,
        .name = "T0_HIGH_ALERT",
        .message = "T0環境溫度高於警告值(alert)",
        .severity = ERROR_SEVERITY_MAJOR,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_HIGH,
        .threshold = 45.0f,
        .hysteresis = 2.0f,
        .delay_ms = 120000,  /* 2 min */
        .enabled = true,
        .get_value = get_t0_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_T0_HIGH_WARNING,
        .name = "T0_HIGH_WARNING",
        .message = "T0環境溫度高於警報值(warning)",
        .severity = ERROR_SEVERITY_MINOR,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_HIGH,
        .threshold = 40.0f,
        .hysteresis = 2.0f,
        .delay_ms = 120000,
        .enabled = true,
        .get_value = get_t0_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_T0_LOW_WARNING,
        .name = "T0_LOW_WARNING",
        .message = "T0環境溫度低於警報值(warning)",
        .severity = ERROR_SEVERITY_MINOR,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_LOW,
        .threshold = 10.0f,
        .hysteresis = 2.0f,
        .delay_ms = 120000,
        .enabled = true,
        .get_value = get_t0_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_T0_LOW_ALERT,
        .name = "T0_LOW_ALERT",
        .message = "T0環境溫度低於警告值(alert)",
        .severity = ERROR_SEVERITY_MAJOR,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_LOW,
        .threshold = 18.0f,
        .hysteresis = 2.0f,
        .delay_ms = 120000,
        .enabled = true,
        .get_value = get_t0_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_RH_HIGH_ALERT,
        .name = "RH_HIGH_ALERT",
        .message = "RH濕度計高於警告值(alert)",
        .severity = ERROR_SEVERITY_MAJOR,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_HIGH,
        .threshold = 80.0f,
        .hysteresis = 2.0f,
        .delay_ms = 120000,
        .enabled = true,
        .get_value = get_rh_value,
        .condition_check = NULL,
        .action_handler = NULL
    },
    {
        .code = LS300D_ERR_RH_LOW_ALERT,
        .name = "RH_LOW_ALERT",
        .message = "RH濕度計低於警告值(alert)",
        .severity = ERROR_SEVERITY_MAJOR,
        .recovery = ERROR_RECOVERY_AUTO,
        .cond_type = ERROR_COND_THRESHOLD_LOW,
        .threshold = 8.0f,
        .hysteresis = 2.0f,
        .delay_ms = 120000,
        .enabled = true,
        .get_value = get_rh_value,
        .condition_check = NULL,
        .action_handler = NULL
    },

    /* ===== 補水泵 ===== */
    {
        .code = LS300D_ERR_REFILL_PUMP_COMM_ERROR,
        .name = "REFILL_PUMP_COMM_ERROR",
        .message = "補水泵通訊異常",
        .severity = ERROR_SEVERITY_MAJOR,
        .recovery = ERROR_RECOVERY_MANUAL,
        .cond_type = ERROR_COND_CUSTOM,
        .threshold = 0,
        .hysteresis = 0,
        .delay_ms = 0,
        .enabled = true,
        .get_value = NULL,
        .condition_check = (int (*)(void))control_logic_ls300d_check_refill_pump_comm,
        .action_handler = NULL
    },
};

/* 錯誤碼定義數量 */
static const uint32_t g_ls300d_error_def_count =
    sizeof(g_ls300d_error_defs) / sizeof(g_ls300d_error_defs[0]);

/* ========== API 函數實作 ========== */

int control_logic_ls300d_error_init(void) {
    printf("[LS300D_ERROR] Initializing LS300D error codes...\n");

    /* 批量註冊錯誤碼 */
    int ret = error_handler_register_batch(g_ls300d_error_defs, g_ls300d_error_def_count);
    if (ret != 0) {
        printf("[LS300D_ERROR] Failed to register error codes\n");
        return ret;
    }

    printf("[LS300D_ERROR] Registered %u error codes\n", g_ls300d_error_def_count);

    /* 嘗試載入配置檔 */
    control_logic_ls300d_error_load_config(NULL);

    return 0;
}

void control_logic_ls300d_error_cleanup(void) {
    printf("[LS300D_ERROR] Cleanup completed\n");
}

int control_logic_ls300d_error_load_config(const char *config_path) {
    const char *path = config_path ? config_path : LS300D_ERROR_CONFIG_DEFAULT_PATH;

    int ret = error_handler_load_config(path);
    if (ret != 0) {
        printf("[LS300D_ERROR] No config file found at %s, using defaults\n", path);
        return 0;  /* 配置檔不存在不視為錯誤 */
    }

    printf("[LS300D_ERROR] Config loaded from %s\n", path);
    return 0;
}

const error_definition_t *control_logic_ls300d_error_get_definitions(uint32_t *count) {
    if (count != NULL) {
        *count = g_ls300d_error_def_count;
    }
    return g_ls300d_error_defs;
}

/**
 * @file control_logic_ls300d_error.h
 * @brief LS300D 機型錯誤碼定義
 *
 * 本文件定義 LS300D 機型專用的錯誤碼，基於 LS300D_error_code_20260108.pdf
 *
 * 錯誤碼範圍：3000-3999
 * - 3000-3099: 系統級錯誤
 * - 3100-3199: 一次側入水管路 (P1, T1, FP)
 * - 3200-3299: 一次側出水管路 (P3, T3, CV1)
 * - 3300-3399: 二次側入水管路 (P2, T2)
 * - 3400-3499: 二次側水箱 (P5, LS5)
 * - 3500-3599: 泵浦 (PUMP1, PUMP2)
 * - 3600-3699: 二次側出水管路 (P4, T4, FS)
 * - 3700-3799: 機箱內 (漏液檢知帶, 溫濕度)
 * - 3800-3899: 系統外 (預留)
 */

#ifndef CONTROL_LOGIC_LS300D_ERROR_H
#define CONTROL_LOGIC_LS300D_ERROR_H

#include "kenmec/main_application/control_logic/error_handler/error_handler.h"

/* ========== 錯誤碼基址 ========== */

#define LS300D_ERROR_BASE 3000

/* ========== 系統級錯誤碼 (3000-3099) ========== */

/** 啟動時二次側水壓不足，請補足冷卻液 */
#define LS300D_ERR_STARTUP_PRESSURE_LOW         (LS300D_ERROR_BASE + 1)

/** 啟動時二次側水箱液位低於警告線，請補足冷卻液 */
#define LS300D_ERR_STARTUP_LEVEL_LOW            (LS300D_ERROR_BASE + 2)

/** 二次側水壓過低，請確認冷卻液是否充足 */
#define LS300D_ERR_RUNNING_PRESSURE_LOW         (LS300D_ERROR_BASE + 3)

/** 二次側水箱液位低於警告線，水泵降轉 */
#define LS300D_ERR_LEVEL_LOW_PUMP_SLOW          (LS300D_ERROR_BASE + 4)

/** 二次側水量不足，請補足冷卻液 */
#define LS300D_ERR_WATER_INSUFFICIENT           (LS300D_ERROR_BASE + 5)

/** 水泵入出口壓力過大，系統停機 */
#define LS300D_ERR_PUMP_PRESSURE_HIGH           (LS300D_ERROR_BASE + 6)

/** 泵已達最高轉速，仍無法達到目標流量 */
#define LS300D_ERR_PUMP_MAX_SPEED_FLOW          (LS300D_ERROR_BASE + 7)

/** 泵已達最高轉速，仍無法達到目標壓差 */
#define LS300D_ERR_PUMP_MAX_SPEED_PRESSURE      (LS300D_ERROR_BASE + 8)

/** 二次側出水溫接近露點溫度警報(warning) */
#define LS300D_ERR_T4_DEWPOINT_WARNING          (LS300D_ERROR_BASE + 9)

/** 二次側出水溫低於露點溫度警告(alert) */
#define LS300D_ERR_T4_DEWPOINT_ALERT            (LS300D_ERROR_BASE + 10)

/** 二次側出水溫過高，系統停機 */
#define LS300D_ERR_T4_OVERHEAT_SHUTDOWN         (LS300D_ERROR_BASE + 11)

/** 二次側入水溫過高，系統停機 */
#define LS300D_ERR_T2_OVERHEAT_SHUTDOWN         (LS300D_ERROR_BASE + 12)

/** 補液失敗3次，請確認補液管路是否塞住 */
#define LS300D_ERR_REFILL_FAILED                (LS300D_ERROR_BASE + 13)

/** 二次側流量過低，請檢查二次側管線 */
#define LS300D_ERR_SECONDARY_FLOW_LOW           (LS300D_ERROR_BASE + 14)

/** 二次側壓差高於警告值(alert) */
#define LS300D_ERR_SECONDARY_DP_HIGH_ALERT      (LS300D_ERROR_BASE + 15)

/** 二次側壓差高於警報值(warning) */
#define LS300D_ERR_SECONDARY_DP_HIGH_WARNING    (LS300D_ERROR_BASE + 16)

/** 二次側壓差低於警報值(warning) */
#define LS300D_ERR_SECONDARY_DP_LOW_WARNING     (LS300D_ERROR_BASE + 17)

/** 二次側壓差低於警告值(alert) */
#define LS300D_ERR_SECONDARY_DP_LOW_ALERT       (LS300D_ERROR_BASE + 18)

/** 一次側流量過低，請檢查一次側管線 */
#define LS300D_ERR_PRIMARY_FLOW_LOW             (LS300D_ERROR_BASE + 19)

/** 一次側壓差高於警告值(alert) */
#define LS300D_ERR_PRIMARY_DP_HIGH_ALERT        (LS300D_ERROR_BASE + 20)

/** 一次側壓差高於警報值(warning) */
#define LS300D_ERR_PRIMARY_DP_HIGH_WARNING      (LS300D_ERROR_BASE + 21)

/** 一次側壓差低於警報值(warning) */
#define LS300D_ERR_PRIMARY_DP_LOW_WARNING       (LS300D_ERROR_BASE + 22)

/** 一次側壓差低於警告值(alert) */
#define LS300D_ERR_PRIMARY_DP_LOW_ALERT         (LS300D_ERROR_BASE + 23)

/* ========== 一次側入水管路 P1 壓力計 (3100-3119) ========== */

/** P1壓力計高於警告值(alert) - threshold:6, hysteresis:5.7, delay:1min */
#define LS300D_ERR_P1_HIGH_ALERT                (LS300D_ERROR_BASE + 100)

/** P1壓力計高於警報值(warning) - threshold:5, hysteresis:4.7, delay:1min */
#define LS300D_ERR_P1_HIGH_WARNING              (LS300D_ERROR_BASE + 101)

/** P1壓力計低於警報值(warning) - threshold:0, hysteresis:0.2, delay:1min */
#define LS300D_ERR_P1_LOW_WARNING               (LS300D_ERROR_BASE + 102)

/** P1壓力計低於警告值(alert) - threshold:-0.3, hysteresis:0, delay:1min */
#define LS300D_ERR_P1_LOW_ALERT                 (LS300D_ERROR_BASE + 103)

/** P1壓力計通訊異常 - P1a與P1b同時通訊異常 */
#define LS300D_ERR_P1_COMM_ERROR                (LS300D_ERROR_BASE + 104)

/** P1a壓力計通訊異常 */
#define LS300D_ERR_P1A_COMM_ERROR               (LS300D_ERROR_BASE + 105)

/** P1b壓力計通訊異常 */
#define LS300D_ERR_P1B_COMM_ERROR               (LS300D_ERROR_BASE + 106)

/** P1壓力計狀態異常 - 預留 */
#define LS300D_ERR_P1_STATUS_ERROR              (LS300D_ERROR_BASE + 107)

/** P1a壓力計狀態異常 - 預留 */
#define LS300D_ERR_P1A_STATUS_ERROR             (LS300D_ERROR_BASE + 108)

/** P1b壓力計狀態異常 - 預留 */
#define LS300D_ERR_P1B_STATUS_ERROR             (LS300D_ERROR_BASE + 109)

/* ========== 一次側入水管路 T1 溫度計 (3120-3139) ========== */

/** T1溫度計高於警告值(alert) - threshold:80, hysteresis:78, delay:1min */
#define LS300D_ERR_T1_HIGH_ALERT                (LS300D_ERROR_BASE + 120)

/** T1溫度計高於警報值(warning) - threshold:45, hysteresis:43, delay:1min */
#define LS300D_ERR_T1_HIGH_WARNING              (LS300D_ERROR_BASE + 121)

/** T1溫度計低於警報值(warning) - threshold:5, hysteresis:7, delay:1min */
#define LS300D_ERR_T1_LOW_WARNING               (LS300D_ERROR_BASE + 122)

/** T1溫度計低於警告值(alert) - threshold:2, hysteresis:4, delay:1min */
#define LS300D_ERR_T1_LOW_ALERT                 (LS300D_ERROR_BASE + 123)

/** T1溫度計通訊異常 - T1a與T1b同時通訊異常 */
#define LS300D_ERR_T1_COMM_ERROR                (LS300D_ERROR_BASE + 124)

/** T1a溫度計通訊異常 */
#define LS300D_ERR_T1A_COMM_ERROR               (LS300D_ERROR_BASE + 125)

/** T1b溫度計通訊異常 */
#define LS300D_ERR_T1B_COMM_ERROR               (LS300D_ERROR_BASE + 126)

/** T1溫度計狀態異常 - 預留 */
#define LS300D_ERR_T1_STATUS_ERROR              (LS300D_ERROR_BASE + 127)

/** T1a溫度計狀態異常 - 預留 */
#define LS300D_ERR_T1A_STATUS_ERROR             (LS300D_ERROR_BASE + 128)

/** T1b溫度計狀態異常 - 預留 */
#define LS300D_ERR_T1B_STATUS_ERROR             (LS300D_ERROR_BASE + 129)

/* ========== 一次側入水管路 FP 流量計 (3140-3149) ========== */

/** FP流量計通訊異常 */
#define LS300D_ERR_FP_COMM_ERROR                (LS300D_ERROR_BASE + 140)

/** FP流量計狀態異常 - 預留 */
#define LS300D_ERR_FP_STATUS_ERROR              (LS300D_ERROR_BASE + 141)

/* ========== 一次側出水管路 P3 壓力計 (3200-3219) ========== */

/** P3壓力計高於警告值(alert) - threshold:6, hysteresis:5.7, delay:1min */
#define LS300D_ERR_P3_HIGH_ALERT                (LS300D_ERROR_BASE + 200)

/** P3壓力計高於警報值(warning) - threshold:5, hysteresis:4.7, delay:1min */
#define LS300D_ERR_P3_HIGH_WARNING              (LS300D_ERROR_BASE + 201)

/** P3壓力計低於警報值(warning) - threshold:0, hysteresis:0.2, delay:1min */
#define LS300D_ERR_P3_LOW_WARNING               (LS300D_ERROR_BASE + 202)

/** P3壓力計低於警告值(alert) - threshold:-0.3, hysteresis:0, delay:1min */
#define LS300D_ERR_P3_LOW_ALERT                 (LS300D_ERROR_BASE + 203)

/** P3壓力計通訊異常 - P3a與P3b同時通訊異常 */
#define LS300D_ERR_P3_COMM_ERROR                (LS300D_ERROR_BASE + 204)

/** P3a壓力計通訊異常 */
#define LS300D_ERR_P3A_COMM_ERROR               (LS300D_ERROR_BASE + 205)

/** P3b壓力計通訊異常 */
#define LS300D_ERR_P3B_COMM_ERROR               (LS300D_ERROR_BASE + 206)

/** P3壓力計狀態異常 - 預留 */
#define LS300D_ERR_P3_STATUS_ERROR              (LS300D_ERROR_BASE + 207)

/** P3a壓力計狀態異常 - 預留 */
#define LS300D_ERR_P3A_STATUS_ERROR             (LS300D_ERROR_BASE + 208)

/** P3b壓力計狀態異常 - 預留 */
#define LS300D_ERR_P3B_STATUS_ERROR             (LS300D_ERROR_BASE + 209)

/* ========== 一次側出水管路 T3 溫度計 (3220-3239) ========== */

/** T3溫度計高於警告值(alert) - threshold:80, hysteresis:78, delay:1min */
#define LS300D_ERR_T3_HIGH_ALERT                (LS300D_ERROR_BASE + 220)

/** T3溫度計高於警報值(warning) - threshold:45, hysteresis:43, delay:1min */
#define LS300D_ERR_T3_HIGH_WARNING              (LS300D_ERROR_BASE + 221)

/** T3溫度計低於警報值(warning) - threshold:5, hysteresis:7, delay:1min */
#define LS300D_ERR_T3_LOW_WARNING               (LS300D_ERROR_BASE + 222)

/** T3溫度計低於警告值(alert) - threshold:2, hysteresis:4, delay:1min */
#define LS300D_ERR_T3_LOW_ALERT                 (LS300D_ERROR_BASE + 223)

/** T3溫度計通訊異常 - T3a與T3b同時通訊異常 */
#define LS300D_ERR_T3_COMM_ERROR                (LS300D_ERROR_BASE + 224)

/** T3a溫度計通訊異常 */
#define LS300D_ERR_T3A_COMM_ERROR               (LS300D_ERROR_BASE + 225)

/** T3b溫度計通訊異常 */
#define LS300D_ERR_T3B_COMM_ERROR               (LS300D_ERROR_BASE + 226)

/** T3溫度計狀態異常 - 預留 */
#define LS300D_ERR_T3_STATUS_ERROR              (LS300D_ERROR_BASE + 227)

/** T3a溫度計狀態異常 - 預留 */
#define LS300D_ERR_T3A_STATUS_ERROR             (LS300D_ERROR_BASE + 228)

/** T3b溫度計狀態異常 - 預留 */
#define LS300D_ERR_T3B_STATUS_ERROR             (LS300D_ERROR_BASE + 229)

/* ========== 一次側出水管路 CV1 控制閥 (3240-3249) ========== */

/** 閥門錯誤動作警告 */
#define LS300D_ERR_CV1_ACTION_ERROR             (LS300D_ERROR_BASE + 240)

/** 控制閥CV1控制異常 */
#define LS300D_ERR_CV1_CONTROL_ERROR            (LS300D_ERROR_BASE + 241)

/** 控制閥CV1通訊異常 */
#define LS300D_ERR_CV1_COMM_ERROR               (LS300D_ERROR_BASE + 242)

/* ========== 二次側入水管路 P2 壓力計 (3300-3319) ========== */

/** P2壓力計高於警告值(alert) - threshold:6, hysteresis:5.7, delay:1min */
#define LS300D_ERR_P2_HIGH_ALERT                (LS300D_ERROR_BASE + 300)

/** P2壓力計高於警報值(warning) - threshold:5, hysteresis:4.7, delay:1min */
#define LS300D_ERR_P2_HIGH_WARNING              (LS300D_ERROR_BASE + 301)

/** P2壓力計低於警報值(warning) - threshold:0, hysteresis:0.2, delay:1min */
#define LS300D_ERR_P2_LOW_WARNING               (LS300D_ERROR_BASE + 302)

/** P2壓力計低於警告值(alert) - threshold:-0.3, hysteresis:0, delay:1min */
#define LS300D_ERR_P2_LOW_ALERT                 (LS300D_ERROR_BASE + 303)

/** P2壓力計通訊異常 - P2a與P2b同時通訊異常 */
#define LS300D_ERR_P2_COMM_ERROR                (LS300D_ERROR_BASE + 304)

/** P2a壓力計通訊異常 */
#define LS300D_ERR_P2A_COMM_ERROR               (LS300D_ERROR_BASE + 305)

/** P2b壓力計通訊異常 */
#define LS300D_ERR_P2B_COMM_ERROR               (LS300D_ERROR_BASE + 306)

/** P2壓力計狀態異常 - 預留 */
#define LS300D_ERR_P2_STATUS_ERROR              (LS300D_ERROR_BASE + 307)

/** P2a壓力計狀態異常 - 預留 */
#define LS300D_ERR_P2A_STATUS_ERROR             (LS300D_ERROR_BASE + 308)

/** P2b壓力計狀態異常 - 預留 */
#define LS300D_ERR_P2B_STATUS_ERROR             (LS300D_ERROR_BASE + 309)

/* ========== 二次側入水管路 T2 溫度計 (3320-3339) ========== */

/** T2溫度計高於警告值(alert) - threshold:80, hysteresis:78, delay:1min */
#define LS300D_ERR_T2_HIGH_ALERT                (LS300D_ERROR_BASE + 320)

/** T2溫度計高於警報值(warning) - threshold:45, hysteresis:43, delay:1min */
#define LS300D_ERR_T2_HIGH_WARNING              (LS300D_ERROR_BASE + 321)

/** T2溫度計低於警報值(warning) - threshold:5, hysteresis:7, delay:1min */
#define LS300D_ERR_T2_LOW_WARNING               (LS300D_ERROR_BASE + 322)

/** T2溫度計低於警告值(alert) - threshold:2, hysteresis:4, delay:1min */
#define LS300D_ERR_T2_LOW_ALERT                 (LS300D_ERROR_BASE + 323)

/** T2溫度計通訊異常 - T2a與T2b同時通訊異常 */
#define LS300D_ERR_T2_COMM_ERROR                (LS300D_ERROR_BASE + 324)

/** T2a溫度計通訊異常 */
#define LS300D_ERR_T2A_COMM_ERROR               (LS300D_ERROR_BASE + 325)

/** T2b溫度計通訊異常 */
#define LS300D_ERR_T2B_COMM_ERROR               (LS300D_ERROR_BASE + 326)

/** T2溫度計狀態異常 - 預留 */
#define LS300D_ERR_T2_STATUS_ERROR              (LS300D_ERROR_BASE + 327)

/** T2a溫度計狀態異常 - 預留 */
#define LS300D_ERR_T2A_STATUS_ERROR             (LS300D_ERROR_BASE + 328)

/** T2b溫度計狀態異常 - 預留 */
#define LS300D_ERR_T2B_STATUS_ERROR             (LS300D_ERROR_BASE + 329)

/* ========== 二次側水箱 P5 壓力計 (3400-3419) ========== */

/** P5壓力計高於安全閥設定壓力，安全閥失效警告 */
#define LS300D_ERR_P5_SAFETY_VALVE_FAIL         (LS300D_ERROR_BASE + 400)

/** 水壓不足，請確認冷卻液是否充足 */
#define LS300D_ERR_P5_PRESSURE_INSUFFICIENT     (LS300D_ERROR_BASE + 401)

/** P5壓力計高於警告值(alert) - threshold:6, hysteresis:5.7, delay:1min */
#define LS300D_ERR_P5_HIGH_ALERT                (LS300D_ERROR_BASE + 402)

/** P5壓力計高於警報值(warning) - threshold:5, hysteresis:4.7, delay:1min */
#define LS300D_ERR_P5_HIGH_WARNING              (LS300D_ERROR_BASE + 403)

/** P5壓力計低於警報值(warning) - threshold:0, hysteresis:0.2, delay:1min */
#define LS300D_ERR_P5_LOW_WARNING               (LS300D_ERROR_BASE + 404)

/** P5壓力計低於警告值(alert) - threshold:-0.3, hysteresis:0, delay:1min */
#define LS300D_ERR_P5_LOW_ALERT                 (LS300D_ERROR_BASE + 405)

/** P5壓力計通訊異常 - P5a與P5b同時通訊異常 */
#define LS300D_ERR_P5_COMM_ERROR                (LS300D_ERROR_BASE + 406)

/** P5a壓力計通訊異常 */
#define LS300D_ERR_P5A_COMM_ERROR               (LS300D_ERROR_BASE + 407)

/** P5b壓力計通訊異常 */
#define LS300D_ERR_P5B_COMM_ERROR               (LS300D_ERROR_BASE + 408)

/** P5壓力計狀態異常 - 預留 */
#define LS300D_ERR_P5_STATUS_ERROR              (LS300D_ERROR_BASE + 409)

/** P5a壓力計狀態異常 - 預留 */
#define LS300D_ERR_P5A_STATUS_ERROR             (LS300D_ERROR_BASE + 410)

/** P5b壓力計狀態異常 - 預留 */
#define LS300D_ERR_P5B_STATUS_ERROR             (LS300D_ERROR_BASE + 411)

/* ========== 二次側水箱 LS5 液位開關 (3420-3439) ========== */

/** 二次側水箱液位低於警告線，水泵降轉 */
#define LS300D_ERR_LS5_LEVEL_LOW                (LS300D_ERROR_BASE + 420)

/** LS5通訊異常 - LS5a與LS5b同時通訊異常 */
#define LS300D_ERR_LS5_COMM_ERROR               (LS300D_ERROR_BASE + 421)

/** LS5a液位開關通訊異常 */
#define LS300D_ERR_LS5A_COMM_ERROR              (LS300D_ERROR_BASE + 422)

/** LS5b液位開關通訊異常 */
#define LS300D_ERR_LS5B_COMM_ERROR              (LS300D_ERROR_BASE + 423)

/* ========== 泵浦 PUMP (3500-3549) ========== */

/** 水泵1與水泵2皆故障停轉 */
#define LS300D_ERR_PUMP_ALL_FAULT               (LS300D_ERROR_BASE + 500)

/** 水泵1與水泵2皆通訊異常 */
#define LS300D_ERR_PUMP_ALL_COMM_ERROR          (LS300D_ERROR_BASE + 501)

/** 水泵1異常 - 欠壓/過壓/過電流/過溫/空轉/堵轉/通訊/驅動掉電 */
#define LS300D_ERR_PUMP1_FAULT                  (LS300D_ERROR_BASE + 502)

/** 水泵1狀態警告(warning) - 預留，水泵健康程度 */
#define LS300D_ERR_PUMP1_STATUS_WARNING         (LS300D_ERROR_BASE + 503)

/** 水泵1狀態告警(alert) - 預留，水泵健康程度 */
#define LS300D_ERR_PUMP1_STATUS_ALERT           (LS300D_ERROR_BASE + 504)

/** 水泵2異常 - 欠壓/過壓/過電流/過溫/空轉/堵轉/通訊/驅動掉電 */
#define LS300D_ERR_PUMP2_FAULT                  (LS300D_ERROR_BASE + 505)

/** 水泵2狀態警告(warning) - 預留，水泵健康程度 */
#define LS300D_ERR_PUMP2_STATUS_WARNING         (LS300D_ERROR_BASE + 506)

/** 水泵2狀態告警(alert) - 預留，水泵健康程度 */
#define LS300D_ERR_PUMP2_STATUS_ALERT           (LS300D_ERROR_BASE + 507)

/* ========== 二次側出水管路 P4 壓力計 (3600-3619) ========== */

/** P4壓力計高於警告值(alert) - threshold:6, hysteresis:5.7, delay:1min */
#define LS300D_ERR_P4_HIGH_ALERT                (LS300D_ERROR_BASE + 600)

/** P4壓力計高於警報值(warning) - threshold:5, hysteresis:4.7, delay:1min */
#define LS300D_ERR_P4_HIGH_WARNING              (LS300D_ERROR_BASE + 601)

/** P4壓力計低於警報值(warning) - threshold:0, hysteresis:0.2, delay:1min */
#define LS300D_ERR_P4_LOW_WARNING               (LS300D_ERROR_BASE + 602)

/** P4壓力計低於警告值(alert) - threshold:-0.3, hysteresis:0, delay:1min */
#define LS300D_ERR_P4_LOW_ALERT                 (LS300D_ERROR_BASE + 603)

/** P4壓力計通訊異常 - P4a與P4b同時通訊異常 */
#define LS300D_ERR_P4_COMM_ERROR                (LS300D_ERROR_BASE + 604)

/** P4a壓力計通訊異常 */
#define LS300D_ERR_P4A_COMM_ERROR               (LS300D_ERROR_BASE + 605)

/** P4b壓力計通訊異常 */
#define LS300D_ERR_P4B_COMM_ERROR               (LS300D_ERROR_BASE + 606)

/** P4壓力計狀態異常 - 預留 */
#define LS300D_ERR_P4_STATUS_ERROR              (LS300D_ERROR_BASE + 607)

/** P4a壓力計狀態異常 - 預留 */
#define LS300D_ERR_P4A_STATUS_ERROR             (LS300D_ERROR_BASE + 608)

/** P4b壓力計狀態異常 - 預留 */
#define LS300D_ERR_P4B_STATUS_ERROR             (LS300D_ERROR_BASE + 609)

/* ========== 二次側出水管路 T4 溫度計 (3620-3639) ========== */

/** 二次側出水溫接近露點溫度警報(warning) */
#define LS300D_ERR_T4_DEWPOINT_NEAR             (LS300D_ERROR_BASE + 620)

/** 二次側出水溫低於露點溫度警告(alert) */
#define LS300D_ERR_T4_BELOW_DEWPOINT            (LS300D_ERROR_BASE + 621)

/** T4溫度計高於警告值(alert) - threshold:55, hysteresis:53, delay:1min */
#define LS300D_ERR_T4_HIGH_ALERT                (LS300D_ERROR_BASE + 622)

/** T4溫度計高於警報值(warning) - threshold:50, hysteresis:48, delay:1min */
#define LS300D_ERR_T4_HIGH_WARNING              (LS300D_ERROR_BASE + 623)

/** T4溫度計低於警報值(warning) - threshold:5, hysteresis:7, delay:1min */
#define LS300D_ERR_T4_LOW_WARNING               (LS300D_ERROR_BASE + 624)

/** T4溫度計低於警告值(alert) - threshold:2, hysteresis:4, delay:1min */
#define LS300D_ERR_T4_LOW_ALERT                 (LS300D_ERROR_BASE + 625)

/** T4溫度計通訊異常 - T4a與T4b同時通訊異常 */
#define LS300D_ERR_T4_COMM_ERROR                (LS300D_ERROR_BASE + 626)

/** T4a溫度計通訊異常 */
#define LS300D_ERR_T4A_COMM_ERROR               (LS300D_ERROR_BASE + 627)

/** T4b溫度計通訊異常 */
#define LS300D_ERR_T4B_COMM_ERROR               (LS300D_ERROR_BASE + 628)

/** T4溫度計狀態異常 - 預留 */
#define LS300D_ERR_T4_STATUS_ERROR              (LS300D_ERROR_BASE + 629)

/** T4a溫度計狀態異常 - 預留 */
#define LS300D_ERR_T4A_STATUS_ERROR             (LS300D_ERROR_BASE + 630)

/** T4b溫度計狀態異常 - 預留 */
#define LS300D_ERR_T4B_STATUS_ERROR             (LS300D_ERROR_BASE + 631)

/* ========== 二次側出水管路 FS 流量計 (3640-3649) ========== */

/** FS流量計通訊異常 */
#define LS300D_ERR_FS_COMM_ERROR                (LS300D_ERROR_BASE + 640)

/** FS流量計狀態異常 - 預留 */
#define LS300D_ERR_FS_STATUS_ERROR              (LS300D_ERROR_BASE + 641)

/* ========== 補水泵 (3650-3659) ========== */

/** 補水泵通訊異常 */
#define LS300D_ERR_REFILL_PUMP_COMM_ERROR       (LS300D_ERROR_BASE + 650)

/* ========== 機箱內 - 承水盤/漏液檢知帶 (3700-3719) ========== */

/** 漏液檢知帶通訊異常 */
#define LS300D_ERR_LEAK_SENSOR_COMM_ERROR       (LS300D_ERROR_BASE + 700)

/** CDU檢測到漏液 */
#define LS300D_ERR_LEAK_DETECTED                (LS300D_ERROR_BASE + 701)

/* ========== 機箱內 - 電控盤/溫濕度感應器 (3720-3749) ========== */

/** 溫濕度計通訊異常 */
#define LS300D_ERR_TH_SENSOR_COMM_ERROR         (LS300D_ERROR_BASE + 720)

/** 露點溫度高於警告值(alert) - threshold:25 */
#define LS300D_ERR_DEWPOINT_HIGH_ALERT          (LS300D_ERROR_BASE + 721)

/** 露點溫度高於警報值(warning) - threshold:20 */
#define LS300D_ERR_DEWPOINT_HIGH_WARNING        (LS300D_ERROR_BASE + 722)

/** T0環境溫度高於警告值(alert) - threshold:45, hysteresis:43, delay:2min */
#define LS300D_ERR_T0_HIGH_ALERT                (LS300D_ERROR_BASE + 723)

/** T0環境溫度高於警報值(warning) - threshold:40 */
#define LS300D_ERR_T0_HIGH_WARNING              (LS300D_ERROR_BASE + 724)

/** T0環境溫度低於警報值(warning) - threshold:10 */
#define LS300D_ERR_T0_LOW_WARNING               (LS300D_ERROR_BASE + 725)

/** T0環境溫度低於警告值(alert) - threshold:18, hysteresis:20, delay:2min */
#define LS300D_ERR_T0_LOW_ALERT                 (LS300D_ERROR_BASE + 726)

/** RH濕度計高於警告值(alert) - threshold:80%, hysteresis:78%, delay:2min */
#define LS300D_ERR_RH_HIGH_ALERT                (LS300D_ERROR_BASE + 727)

/** RH濕度計高於警報值(warning) */
#define LS300D_ERR_RH_HIGH_WARNING              (LS300D_ERROR_BASE + 728)

/** RH濕度計低於警報值(warning) */
#define LS300D_ERR_RH_LOW_WARNING               (LS300D_ERROR_BASE + 729)

/** RH濕度計低於警告值(alert) - threshold:8%, hysteresis:10%, delay:2min */
#define LS300D_ERR_RH_LOW_ALERT                 (LS300D_ERROR_BASE + 730)

/* ========== 系統外 - 外接漏液檢知帶 (3800-3809) ========== */

/** 外接漏液檢知帶通訊異常 - 預留 */
#define LS300D_ERR_EXT_LEAK_SENSOR_COMM_ERROR   (LS300D_ERROR_BASE + 800)

/** 外部檢測到漏液 - 預留 */
#define LS300D_ERR_EXT_LEAK_DETECTED            (LS300D_ERROR_BASE + 801)

/* ========== 錯誤碼總數 ========== */

#define LS300D_ERROR_TOTAL_COUNT 120

/* ========== API 函數宣告 ========== */

/**
 * @brief 初始化 LS300D 錯誤碼處理模組
 *
 * 註冊所有 LS300D 錯誤碼定義到錯誤處理框架
 *
 * @return 成功返回 0，失敗返回負值
 */
int control_logic_ls300d_error_init(void);

/**
 * @brief 清理 LS300D 錯誤碼處理模組
 */
void control_logic_ls300d_error_cleanup(void);

/**
 * @brief 載入 LS300D 錯誤碼配置檔
 *
 * @param config_path 配置檔路徑，NULL 使用預設路徑
 * @return 成功返回 0
 */
int control_logic_ls300d_error_load_config(const char *config_path);

/**
 * @brief 獲取 LS300D 錯誤碼定義陣列
 *
 * @param count 輸出參數，返回定義數量
 * @return 錯誤碼定義陣列指標
 */
const error_definition_t *control_logic_ls300d_error_get_definitions(uint32_t *count);

#endif /* CONTROL_LOGIC_LS300D_ERROR_H */

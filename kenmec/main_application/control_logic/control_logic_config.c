/**
 * @file control_logic_config.c
 * @brief 控制邏輯配置管理實現
 *
 * 本文件實現了控制邏輯系統的配置管理功能,負責各種配置的加載、保存和管理。
 *
 * 主要功能:
 * 1. 系統配置管理(機器類型等)
 * 2. Modbus 設備配置管理
 * 3. 溫度傳感器配置管理
 * 4. 模擬量輸入/輸出配置管理(電壓/電流)
 * 5. 控制邏輯寄存器配置管理
 *
 * 實現原理:
 * - 使用 JSON 格式存儲配置數據
 * - 配置文件存儲在文件系統中
 * - 提供配置的加載、保存、獲取和設置接口
 * - 支援從字符串和文件加載配置
 * - 使用靜態全局變量緩存配置數據
 *
 * 配置類型:
 * - system_config_t: 系統配置(機器類型)
 * - modbus_device_config_t: Modbus 設備配置
 * - temperature_config_t: 溫度傳感器配置
 * - analog_config_t: 模擬量配置(輸入/輸出,電壓/電流)
 *
 * @note 配置變更後會同步保存到文件系統
 */

#include "dexatek/main_application/include/application_common.h"
#include "dexatek/main_application/managers/modbus_manager/modbus_manager.h"

#include "kenmec/main_application/kenmec_config.h"
#include "kenmec/main_application/control_logic/control_logic_manager.h"

/*---------------------------------------------------------------------------
                            Defined Constants
 ---------------------------------------------------------------------------*/
/* 日誌標籤 */
static const char* tag = "control_logic_config";

/*---------------------------------------------------------------------------
								Variables
 ---------------------------------------------------------------------------*/

/* 系統配置指標 */
static system_config_t *_system_configs = NULL;

/* Modbus 設備配置數量 */
static int _modbus_device_config_count = 0;
/* Modbus 設備配置陣列指標 */
static modbus_device_config_t *_modbus_device_config = NULL;

/* 溫度傳感器配置數量 */
static int _temperature_configs_count = 0;
/* 溫度傳感器配置陣列指標 */
static temperature_config_t *_temperature_configs = NULL;

/* 模擬量電流輸入配置數量 */
static int _analog_input_current_configs_count = 0;
/* 模擬量電流輸入配置陣列指標 */
static analog_config_t *_analog_input_current_configs = NULL;

/* 模擬量電壓輸入配置數量 */
static int _analog_input_voltage_configs_count = 0;
/* 模擬量電壓輸入配置陣列指標 */
static analog_config_t *_analog_input_voltage_configs = NULL;

/* 模擬量電壓輸出配置數量 */
static int _analog_output_voltage_configs_count = 0;
/* 模擬量電壓輸出配置陣列指標 */
static analog_config_t *_analog_output_voltage_configs = NULL;

/* 模擬量電流輸出配置數量 */
static int _analog_output_current_configs_count = 0;
/* 模擬量電流輸出配置陣列指標 */
static analog_config_t *_analog_output_current_configs = NULL;

/*---------------------------------------------------------------------------
                             Function Prototypes
 ---------------------------------------------------------------------------*/
static int _system_configs_clean(void);
static int _system_configs_init(void);
static int _system_configs_load_from_file(const char *path);
static int _system_configs_load_from_string(const char *json_string);

static int _modbus_device_configs_init(void);
static int _modbus_device_configs_clean(void);
static int _modbus_device_configs_load_from_file(const char *path);
static int _modbus_device_configs_load_from_string(const char *json_string);

static int _temperature_configs_init(void);
static int _temperature_configs_clean(void);
static int _temperature_configs_load_from_file(const char *path);
static int _temperature_configs_load_from_string(const char *json_string);

static int _analog_input_current_configs_init(void);
static int _analog_input_current_configs_clean(void);
static int _analog_input_current_configs_load_from_file(const char *path);
static int _analog_input_current_configs_load_from_string(const char *json_string);

static int _analog_input_voltage_configs_init(void);
static int _analog_input_voltage_configs_clean(void);
static int _analog_input_voltage_configs_load_from_file(const char *path);
static int _analog_input_voltage_configs_load_from_string(const char *json_string);

static int _analog_output_voltage_configs_init(void);
static int _analog_output_voltage_configs_clean(void);
static int _analog_output_voltage_configs_load_from_file(const char *path);
static int _analog_output_voltage_configs_load_from_string(const char *json_string);

static int _analog_output_current_configs_init(void);
static int _analog_output_current_configs_clean(void);
static int _analog_output_current_configs_load_from_file(const char *path);
static int _analog_output_current_configs_load_from_string(const char *json_string);

/*---------------------------------------------------------------------------
                                 Implementation
 ---------------------------------------------------------------------------*/

static int _system_configs_clean(void)
{
    if (_system_configs != NULL) {
        platform_slow_free(_system_configs);
        _system_configs = NULL;
    }

    return SUCCESS;
}

static int _modbus_device_configs_clean(void)
{
    if (_modbus_device_config != NULL) {
        platform_slow_free(_modbus_device_config);
        _modbus_device_config = NULL;
        _modbus_device_config_count = 0;
    }

    return SUCCESS;
}

static int _temperature_configs_clean(void)
{
    if (_temperature_configs != NULL) {
        platform_slow_free(_temperature_configs);
        _temperature_configs = NULL;
        _temperature_configs_count = 0;
    }

    return SUCCESS;
}

static int _analog_input_current_configs_clean(void)
{
    if (_analog_input_current_configs != NULL) {
        platform_slow_free(_analog_input_current_configs);
        _analog_input_current_configs = NULL;
        _analog_input_current_configs_count = 0;
    }

    return SUCCESS;
}

static int _analog_input_voltage_configs_clean(void)
{
    if (_analog_input_voltage_configs != NULL) {
        platform_slow_free(_analog_input_voltage_configs);
        _analog_input_voltage_configs = NULL;
        _analog_input_voltage_configs_count = 0;
    }

    return SUCCESS;
}

static int _analog_output_voltage_configs_clean(void)
{
    if (_analog_output_voltage_configs != NULL) {
        platform_slow_free(_analog_output_voltage_configs);
        _analog_output_voltage_configs = NULL;
        _analog_output_voltage_configs_count = 0;
    }

    return SUCCESS;
}

static int _analog_output_current_configs_clean(void)
{
    if (_analog_output_current_configs != NULL) {
        platform_slow_free(_analog_output_current_configs);
        _analog_output_current_configs = NULL;
        _analog_output_current_configs_count = 0;
    }

    return SUCCESS;
}

static int _save_string_to_file(const char *path, const char *json_string)
{
    int ret = SUCCESS;

    FILE *fp = fopen(path, "w");
    if (!fp) {
        error(tag, "Failed to open file for writing: %s", path);
        return FAIL;
    }

    size_t len = strlen(json_string);
    size_t written = fwrite(json_string, 1, len, fp);

    // flush stdio buffer to kernel page cache
    fflush(fp);

    // force flush to storage device
    int fd = fileno(fp);
    if (fsync(fd) != 0) {
        error(tag, "fsync failed");
        fclose(fp);
        return FAIL;
    }

    fclose(fp);    

    if (written != len) {
        error(tag, "Failed to write to file: %s", path);
        ret = FAIL;
    } else {
        info(tag, "Successfully saved to file: %s", path);
        ret = SUCCESS;
    }

    return ret;
}

/**
 * @brief 讀取整個文件內容
 *
 * 功能說明:
 * 讀取文件的完整內容到記憶體緩衝區。
 *
 * @param path    文件路徑
 * @param out_len 輸出文件長度的指標(可選)
 *
 * @return char* 文件內容緩衝區指標
 *         - 成功: 返回已分配的緩衝區指標(調用者需負責釋放)
 *         - 失敗: 返回 NULL
 *
 * 實現邏輯:
 * 1. 以二進制模式打開文件
 * 2. 移動到文件末尾獲取文件大小
 * 3. 分配對應大小的記憶體緩衝區
 * 4. 讀取文件內容到緩衝區
 * 5. 添加字符串結束符
 * 6. 返回緩衝區指標
 *
 * @note 調用者需使用 free() 釋放返回的緩衝區
 */
char* control_logic_read_entire_file(const char *path, long *out_len)
{
    /* 打開文件 */
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    /* 移動到文件末尾 */
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return NULL; }

    /* 獲取文件長度 */
    long len = ftell(fp);
    if (len < 0) { fclose(fp); return NULL; }

    /* 移動回文件開頭 */
    if (fseek(fp, 0, SEEK_SET) != 0) { fclose(fp); return NULL; }
    if (len < 0) { fclose(fp); return NULL; }
    if (fseek(fp, 0, SEEK_SET) != 0) { fclose(fp); return NULL; }

    /* 分配記憶體 */
    char *buf = (char*)malloc((size_t)len + 1);
    if (!buf) { fclose(fp); return NULL; }

    /* 讀取文件 */
    size_t rd = fread(buf, 1, (size_t)len, fp);

    /* 關閉文件 */
    fclose(fp);

    /* 檢查讀取長度 */
    if (rd != (size_t)len) { free(buf); return NULL; }

    /* 設置字符串結束符 */
    buf[len] = '\0';

    /* 設置輸出長度 */
    if (out_len) *out_len = len;

    /* 返回緩衝區 */
    return buf;
}

static int _modbus_device_configs_load_from_string(const char *json_string)
{
    // validate
    cJSON *root = cJSON_Parse(json_string);
    
    if (!root) {
        error(tag, "Failed to parse JSON config");
        return FAIL;
    }

    // check if is array
    cJSON *devices = NULL;
    if (cJSON_IsArray(root)) {
        devices = root;
    } else {
        cJSON_Delete(root);
        error(tag, "JSON config must be an array or contain a 'devices' array");
        return FAIL;
    }

    // check array size
    int total = cJSON_GetArraySize(devices);
    if (total <= 0) {
        // clean config
        _modbus_device_configs_clean();
        cJSON_Delete(root);
        return SUCCESS;
    }

    // allocate memory
    modbus_device_config_t *arr = (modbus_device_config_t*)platform_slow_calloc((size_t)total, sizeof(modbus_device_config_t));
    if (!arr) {
        cJSON_Delete(root);
        error(tag, "Out of memory allocating device configs");
        return FAIL;
    }

    // foreach devices
    int idx = 0;
    cJSON *it = NULL;
    cJSON_ArrayForEach(it, devices) {
        if (!cJSON_IsObject(it)) continue;

        cJSON *port = cJSON_GetObjectItemCaseSensitive(it, "board");
        cJSON *baudrate = cJSON_GetObjectItemCaseSensitive(it, "baudrate");
        cJSON *slave_id = cJSON_GetObjectItemCaseSensitive(it, "slave_id");
        cJSON *code = cJSON_GetObjectItemCaseSensitive(it, "code");
        cJSON *address = cJSON_GetObjectItemCaseSensitive(it, "address");
        cJSON *data_type = cJSON_GetObjectItemCaseSensitive(it, "data_type");
        cJSON *update_address = cJSON_GetObjectItemCaseSensitive(it, "update_address");
        cJSON *scale = cJSON_GetObjectItemCaseSensitive(it, "scale");
        cJSON *name = cJSON_GetObjectItemCaseSensitive(it, "name");

        if (!cJSON_IsNumber(port) || !cJSON_IsNumber(baudrate) || !cJSON_IsNumber(slave_id) ||
            !cJSON_IsNumber(code) || !cJSON_IsNumber(address) || !cJSON_IsNumber(data_type) ||
            !cJSON_IsNumber(update_address)) {
            error(tag, "Invalid device config entry (missing numeric fields)");
            continue;
        }

        modbus_device_config_t c = {0};
        c.port = (uint8_t)port->valueint;
        c.baudrate = (uint32_t)baudrate->valueint;
        c.slave_id = (uint8_t)slave_id->valueint;
        c.function_code = (uint8_t)code->valueint;
        c.reg_address = (uint16_t)address->valueint;
        c.data_type = (uint8_t)data_type->valueint;
        c.update_address = (uint16_t)update_address->valueint;

        // scale
        if (cJSON_IsNumber(scale)) {
            c.fScale = (float)scale->valuedouble;
        } else {
            c.fScale = 0.0;
        }

        debug(tag, "scale: %f", c.fScale);

        // copy name
        if (name != NULL && name->valuestring != NULL) {
            strncpy(c.name, name->valuestring, sizeof(c.name)-1);
            c.name[sizeof(c.name)-1] = '\0';
        }

        // basic validation
        switch (c.function_code) {
            case MODBUS_FUNC_READ_COILS:
            case MODBUS_FUNC_READ_DISCRETE_INPUTS:
            case MODBUS_FUNC_READ_HOLDING_REGISTERS:
            case MODBUS_FUNC_READ_INPUT_REGISTERS:
            case MODBUS_FUNC_WRITE_SINGLE_REGISTER:
            case MODBUS_FUNC_WRITE_MULTIPLE_REGISTERS:
                break;
            default:
                error(tag, "Unsupported function code: %d", c.function_code);
                continue;
        }

        // append to array
        arr[idx++] = c;
    }

    // free json root
    cJSON_Delete(root);

    // clean config
    _modbus_device_configs_clean();
    
    // set config
    _modbus_device_config = arr;
    _modbus_device_config_count = idx;
    debug(tag, "Loaded %d Modbus device configs from string", _modbus_device_config_count);

    return SUCCESS;
}

static int _modbus_device_configs_load_from_file(const char *path)
{
    int ret = SUCCESS;

    long json_len = 0;
    char *json_text = control_logic_read_entire_file(path, &json_len);
    if (!json_text) {
        error(tag, "Failed to open/read device config file: %s", path);
        return FAIL;
    }

    // load from string
    if (_modbus_device_configs_load_from_string(json_text) != SUCCESS) {
        error(tag, "load modbus device configs from string failed");
        ret = FAIL;
    }

    free(json_text);

    return ret;
}

static int _modbus_device_configs_init(void)
{
    int ret = SUCCESS;

    // debug(tag, "load modbus device configs from file: %s", CONFIG_MODBUS_DEVICE_CONFIG_PATH);
    if (_modbus_device_configs_load_from_file(CONFIG_MODBUS_DEVICE_CONFIG_PATH) != SUCCESS) {
        // debug(tag, "load modbus device configs from file failed, try to load from file: %s", CONFIG_MODBUS_DEVICE_CONFIG_PATH);
        // if (_modbus_device_config_load_from_file(CONFIG_MODBUS_DEVICE_CONFIG_PATH_DEFAULT) != SUCCESS) {
        //     // error(tag, "load modbus device configs from file failed");
        //     ret = FAIL;
        // }
    }
    
    return ret;
}

int control_logic_modbus_device_configs_set(const char *json_string)
{
    int ret = SUCCESS;
    
    // try to load config from string
    if (_modbus_device_configs_load_from_string(json_string) == SUCCESS) {
        // save to file
        ret = _save_string_to_file(CONFIG_MODBUS_DEVICE_CONFIG_PATH, json_string);
    } else {
        error(tag, "load modbus device configs from string failed");
        ret = FAIL;
    }

    return ret;
}

modbus_device_config_t* control_logic_modbus_device_configs_get(int *config_count)
{
    if (config_count) {
        *config_count = _modbus_device_config_count;
    }

    return _modbus_device_config;
}

/**
 * @brief 初始化控制邏輯配置
 *
 * 功能說明:
 * 初始化所有控制邏輯相關的配置,包括系統配置、設備配置和傳感器配置。
 *
 * @return int 執行結果
 *         - SUCCESS: 初始化成功
 *         - FAIL: 初始化失敗
 *
 * 實現邏輯:
 * 依次初始化以下配置:
 * 1. 系統配置(機器類型)
 * 2. Modbus 設備配置
 * 3. 溫度傳感器配置
 * 4. 模擬量電流輸入配置
 * 5. 模擬量電壓輸入配置
 * 6. 模擬量電壓輸出配置
 * 7. 模擬量電流輸出配置
 */
int control_logic_config_init(void)
{
    int ret = SUCCESS;

    /* 初始化各項配置 */
    _system_configs_init();
    _modbus_device_configs_init();
    _temperature_configs_init();
    _analog_input_current_configs_init();
    _analog_input_voltage_configs_init();
    _analog_output_voltage_configs_init();
    _analog_output_current_configs_init();

    return ret;
}

temperature_config_t* control_logic_temperature_configs_get(int *config_count)
{
    if (config_count) {
        *config_count = _temperature_configs_count;
    }

    return _temperature_configs;
}

static int _temperature_configs_init(void)
{
    return _temperature_configs_load_from_file(CONFIG_TEMPERATURE_CONFIGE_PATH);
}

static int _temperature_configs_load_from_file(const char *path)
{
    int ret = SUCCESS;

    long json_len = 0;
    char *json_text = control_logic_read_entire_file(path, &json_len);
    if (!json_text) {
        error(tag, "Failed to open/read temperature config file: %s", path);
        return FAIL;
    }

    // load from string
    if (_temperature_configs_load_from_string(json_text) != SUCCESS) {
        error(tag, "load temperature configs from string failed");
        ret = FAIL;
    }

    free(json_text);

    return ret;
}

static int _temperature_configs_load_from_string(const char *json_string)
{
    // validate
    cJSON *root = cJSON_Parse(json_string);
    
    if (!root) {
        error(tag, "Failed to parse JSON config");
        return FAIL;
    }

    // check if is array
    cJSON *configs = NULL;
    if (cJSON_IsArray(root)) {
        configs = root;
    } else {
        cJSON_Delete(root);
        error(tag, "JSON config must be an array or contain a 'configs' array");
        return FAIL;
    }

    // check array size
    int total = cJSON_GetArraySize(configs);
    if (total <= 0) {
        // clean config
        _temperature_configs_clean();
        cJSON_Delete(root);
        return SUCCESS;
    }

    // allocate memory
    temperature_config_t *arr = (temperature_config_t*)platform_slow_calloc((size_t)total, sizeof(temperature_config_t));
    if (!arr) {
        cJSON_Delete(root);
        error(tag, "Out of memory allocating temperature configs");
        return FAIL;
    }

    // foreach devices
    int idx = 0;
    cJSON *it = NULL;
    cJSON_ArrayForEach(it, configs) {
        if (!cJSON_IsObject(it)) continue;

        cJSON *port = cJSON_GetObjectItemCaseSensitive(it, "board");
        cJSON *channel = cJSON_GetObjectItemCaseSensitive(it, "channel");
        cJSON *sensor_type = cJSON_GetObjectItemCaseSensitive(it, "sensor_type");
        cJSON *update_address = cJSON_GetObjectItemCaseSensitive(it, "update_address");
        cJSON *name = cJSON_GetObjectItemCaseSensitive(it, "name");

        if (!cJSON_IsNumber(port) || !cJSON_IsNumber(channel) || !cJSON_IsNumber(sensor_type) ||
            !cJSON_IsNumber(update_address)) {
            error(tag, "Invalid temperature config entry (missing numeric fields)");
            continue;
        }

        temperature_config_t c = {0};
        c.port = (uint8_t)port->valueint;
        c.channel = (uint8_t)channel->valueint;
        c.sensor_type = (uint8_t)sensor_type->valueint;
        c.update_address = (uint16_t)update_address->valueint;
        
        // copy name
        if (name != NULL && name->valuestring != NULL) {
            strncpy(c.name, name->valuestring, sizeof(c.name)-1);
            c.name[sizeof(c.name)-1] = '\0';
        }

        // append to array
        arr[idx++] = c;
    }

    // free json root
    cJSON_Delete(root);

    // clean config
    _temperature_configs_clean();
    
    // set config
    _temperature_configs = arr;
    _temperature_configs_count = idx;

    debug(tag, "Loaded %d temperature configs from string", _temperature_configs_count);

    return SUCCESS;
}

int control_logic_temperature_configs_set(const char *json_string)
{
    int ret = SUCCESS;

    // load from string
    if (_temperature_configs_load_from_string(json_string) != SUCCESS) {
        error(tag, "load temperature configs from string failed");
        return FAIL;
    }

    // save to file
    if (_save_string_to_file(CONFIG_TEMPERATURE_CONFIGE_PATH, json_string) != SUCCESS) {
        ret = FAIL;
    }

    return ret;
}

analog_config_t* control_logic_analog_input_current_configs_get(int *config_count)
{
    if (config_count) {
        *config_count = _analog_input_current_configs_count;
    }

    return _analog_input_current_configs;
}

static int _analog_input_current_configs_init(void)
{
    if (_analog_input_current_configs_load_from_file(CONFIG_ANALOG_INPUT_CURRENT_CONFIGE_PATH) != SUCCESS) {
        error(tag, "load %s file failed", CONFIG_ANALOG_INPUT_CURRENT_CONFIGE_PATH);

        // load from default file
        if (_analog_input_current_configs_load_from_file(CONFIG_ANALOG_INPUT_CURRENT_CONFIGE_DEFAULT_PATH) != SUCCESS) {
            error(tag, "load %s default file failed", CONFIG_ANALOG_INPUT_CURRENT_CONFIGE_DEFAULT_PATH);
            return FAIL;
        }
    
        return FAIL;
    }


    return SUCCESS;
}

static int _analog_input_current_configs_load_from_file(const char *path)
{
    int ret = SUCCESS;

    long json_len = 0;
    char *json_text = control_logic_read_entire_file(path, &json_len);
    if (!json_text) {
        error(tag, "Failed to open/read analog current input config file: %s", path);
        return FAIL;
    }

    // load from string
    if (_analog_input_current_configs_load_from_string(json_text) != SUCCESS) {
        error(tag, "load analog current input configs from string failed");
        ret = FAIL;
    }

    free(json_text);

    return ret;
}

static int _analog_input_current_configs_load_from_string(const char *json_string)
{
    // validate
    cJSON *root = cJSON_Parse(json_string);
    
    if (!root) {
        error(tag, "Failed to parse JSON config");
        return FAIL;
    }

    // check if is array
    cJSON *configs = NULL;
    if (cJSON_IsArray(root)) {
        configs = root;
    } else {
        cJSON_Delete(root);
        error(tag, "JSON config must be an array or contain a 'configs' array");
        return FAIL;
    }

    // check array size
    int total = cJSON_GetArraySize(configs);
    if (total <= 0) {
        // clean config
        _analog_input_current_configs_clean();
        cJSON_Delete(root);
        return SUCCESS;
    }

    // allocate memory
    analog_config_t *arr = (analog_config_t*)platform_slow_calloc((size_t)total, sizeof(analog_config_t));
    if (!arr) {
        cJSON_Delete(root);
        error(tag, "Out of memory allocating analog current input configs");
        return FAIL;
    }

    // foreach devices
    int idx = 0;
    cJSON *it = NULL;
    cJSON_ArrayForEach(it, configs) {
        if (!cJSON_IsObject(it)) continue;

        cJSON *port = cJSON_GetObjectItemCaseSensitive(it, "board");
        cJSON *channel = cJSON_GetObjectItemCaseSensitive(it, "channel");
        cJSON *sensor_type = cJSON_GetObjectItemCaseSensitive(it, "sensor_type");
        cJSON *update_address = cJSON_GetObjectItemCaseSensitive(it, "update_address");
        cJSON *name = cJSON_GetObjectItemCaseSensitive(it, "name");

        if (!cJSON_IsNumber(port) || !cJSON_IsNumber(channel) || !cJSON_IsNumber(sensor_type) || 
            !cJSON_IsNumber(update_address)) {
            error(tag, "Invalid analog current input config entry (missing numeric fields)");
            continue;
        }

        analog_config_t c = {0};
        c.port = (uint8_t)port->valueint;
        c.channel = (uint8_t)channel->valueint;
        c.sensor_type = (uint8_t)sensor_type->valueint;
        c.update_address = (uint16_t)update_address->valueint;
        
        // copy name
        if (name != NULL && name->valuestring != NULL) {
            strncpy(c.name, name->valuestring, sizeof(c.name)-1);
            c.name[sizeof(c.name)-1] = '\0';
        }

        // append to array
        arr[idx++] = c;
    }

    // free json root
    cJSON_Delete(root);

    // clean config
    _analog_input_current_configs_clean();
    
    // set config
    _analog_input_current_configs = arr;
    _analog_input_current_configs_count = idx;

    debug(tag, "Loaded %d analog current input configs from string", _analog_input_current_configs_count);

    return SUCCESS;
}

int control_logic_analog_input_current_configs_set(const char *json_string)
{
    int ret = SUCCESS;

    // load from string
    if (_analog_input_current_configs_load_from_string(json_string) != SUCCESS) {
        error(tag, "load analog current input configs from string failed");
        return FAIL;
    }

    // Save the provided json_string to the analog current input configs file
    if (_save_string_to_file(CONFIG_ANALOG_INPUT_CURRENT_CONFIGE_PATH, json_string) != SUCCESS) {
        ret = FAIL;
    }

    return ret;
}

analog_config_t* control_logic_analog_input_voltage_configs_get(int *config_count)
{
    if (config_count) {
        *config_count = _analog_input_voltage_configs_count;
    }

    return _analog_input_voltage_configs;
}

static int _analog_input_voltage_configs_init(void)
{
    return _analog_input_voltage_configs_load_from_file(CONFIG_ANALOG_INPUT_VOLTAGE_CONFIGE_PATH);
}

static int _analog_input_voltage_configs_load_from_file(const char *path)
{
    int ret = SUCCESS;

    long json_len = 0;
    char *json_text = control_logic_read_entire_file(path, &json_len);
    if (!json_text) {
        error(tag, "Failed to open/read analog voltage input config file: %s", path);
        return FAIL;
    }

    // load from string
    if (_analog_input_voltage_configs_load_from_string(json_text) != SUCCESS) {
        error(tag, "load analog voltage input configs from string failed");
        ret = FAIL;
    }
    
    free(json_text);

    return ret;
}

static int _analog_input_voltage_configs_load_from_string(const char *json_string)
{
    // validate
    cJSON *root = cJSON_Parse(json_string);
    
    if (!root) {
        error(tag, "Failed to parse JSON config");
        return FAIL;
    }

    // check if is array
    cJSON *configs = NULL;
    if (cJSON_IsArray(root)) {
        configs = root;
    } else {
        cJSON_Delete(root);
        error(tag, "JSON config must be an array or contain a 'configs' array");
        return FAIL;
    }

    // check array size
    int total = cJSON_GetArraySize(configs);
    if (total <= 0) {
        // clean config
        _analog_input_voltage_configs_clean();
        cJSON_Delete(root);
        return SUCCESS;
    }

    // allocate memory
    analog_config_t *arr = (analog_config_t*)platform_slow_calloc((size_t)total, sizeof(analog_config_t));
    if (!arr) {
        cJSON_Delete(root);
        error(tag, "Out of memory allocating analog current input configs");
        return FAIL;
    }

    // foreach devices
    int idx = 0;
    cJSON *it = NULL;
    cJSON_ArrayForEach(it, configs) {
        if (!cJSON_IsObject(it)) continue;

        cJSON *port = cJSON_GetObjectItemCaseSensitive(it, "board");
        cJSON *channel = cJSON_GetObjectItemCaseSensitive(it, "channel");
        cJSON *sensor_type = cJSON_GetObjectItemCaseSensitive(it, "sensor_type");
        cJSON *update_address = cJSON_GetObjectItemCaseSensitive(it, "update_address");
        cJSON *name = cJSON_GetObjectItemCaseSensitive(it, "name");

        if (!cJSON_IsNumber(port) || !cJSON_IsNumber(channel) || !cJSON_IsNumber(sensor_type) || !cJSON_IsNumber(update_address)) {
            error(tag, "Invalid analog current input config entry (missing numeric fields)");
            continue;
        }

        analog_config_t c = {0};
        c.port = (uint8_t)port->valueint;
        c.channel = (uint8_t)channel->valueint;
        c.sensor_type = (uint8_t)sensor_type->valueint;
        c.update_address = (uint16_t)update_address->valueint;
        
        // copy name
        if (name != NULL && name->valuestring != NULL) {
            strncpy(c.name, name->valuestring, sizeof(c.name)-1);
            c.name[sizeof(c.name)-1] = '\0';
        }

        arr[idx++] = c;
    }

    // free json root
    cJSON_Delete(root);

    // clean config
    _analog_input_voltage_configs_clean();
    
    // set config
    _analog_input_voltage_configs = arr;
    _analog_input_voltage_configs_count = idx;

    debug(tag, "Loaded %d analog voltage input configs from string", _analog_input_voltage_configs_count);

    return SUCCESS;
}

int control_logic_analog_input_voltage_configs_set(const char *json_string)
{
    int ret = SUCCESS;

    // load from string
    if (_analog_input_voltage_configs_load_from_string(json_string) != SUCCESS) {
        error(tag, "load analog input voltage configs from string failed");
        return FAIL;
    }

    // Save the provided json_string to the analog voltage input configs file
    if (_save_string_to_file(CONFIG_ANALOG_INPUT_VOLTAGE_CONFIGE_PATH, json_string) != SUCCESS) {
        ret = FAIL;
    }

    return ret;
}

analog_config_t* control_logic_analog_output_voltage_configs_get(int *config_count)
{
    if (config_count) {
        *config_count = _analog_output_voltage_configs_count;
    }

    return _analog_output_voltage_configs;
}

static int _analog_output_voltage_configs_init(void)
{
    return _analog_output_voltage_configs_load_from_file(CONFIG_ANALOG_OUTPUT_VOLTAGE_CONFIGE_PATH);
}

static int _analog_output_voltage_configs_load_from_file(const char *path)
{
    int ret = SUCCESS;

    long json_len = 0;
    char *json_text = control_logic_read_entire_file(path, &json_len);
    if (!json_text) {
        error(tag, "Failed to open/read analog voltage input config file: %s", path);
        return FAIL;
    }

    // load from string
    if (_analog_output_voltage_configs_load_from_string(json_text) != SUCCESS) {
        error(tag, "load analog voltage input configs from string failed");
        ret = FAIL;
    }

    free(json_text);

    return ret;
}

static int _analog_output_voltage_configs_load_from_string(const char *json_string)
{
    // validate
    cJSON *root = cJSON_Parse(json_string);
    
    if (!root) {
        error(tag, "Failed to parse JSON config");
        return FAIL;
    }

    // check if is array
    cJSON *configs = NULL;
    if (cJSON_IsArray(root)) {
        configs = root;
    } else {
        cJSON_Delete(root);
        error(tag, "JSON config must be an array or contain a 'configs' array");
        return FAIL;
    }

    // check array size
    int total = cJSON_GetArraySize(configs);
    if (total <= 0) {
        // clean config
        _analog_output_voltage_configs_clean();
        cJSON_Delete(root);
        return SUCCESS;
    }

    // allocate memory
    analog_config_t *arr = (analog_config_t*)platform_slow_calloc((size_t)total, sizeof(analog_config_t));
    if (!arr) {
        cJSON_Delete(root);
        error(tag, "Out of memory allocating analog current input configs");
        return FAIL;
    }

    // foreach devices
    int idx = 0;
    cJSON *it = NULL;
    cJSON_ArrayForEach(it, configs) {
        if (!cJSON_IsObject(it)) continue;

        cJSON *port = cJSON_GetObjectItemCaseSensitive(it, "board");
        cJSON *channel = cJSON_GetObjectItemCaseSensitive(it, "channel");
        cJSON *sensor_type = cJSON_GetObjectItemCaseSensitive(it, "sensor_type");
        cJSON *name = cJSON_GetObjectItemCaseSensitive(it, "name");

        if (!cJSON_IsNumber(port) || !cJSON_IsNumber(channel) || !cJSON_IsNumber(sensor_type)) {
            error(tag, "Invalid analog current input config entry (missing numeric fields)");
            continue;
        }

        analog_config_t c = {0};
        c.port = (uint8_t)port->valueint;
        c.channel = (uint8_t)channel->valueint;
        c.sensor_type = (uint8_t)sensor_type->valueint;
        
        // copy name
        if (name != NULL && name->valuestring != NULL) {
            strncpy(c.name, name->valuestring, sizeof(c.name)-1);
            c.name[sizeof(c.name)-1] = '\0';
        }

        arr[idx++] = c;
    }

    // free json root
    cJSON_Delete(root);

    // clean config
    _analog_output_voltage_configs_clean();
    
    // set config
    _analog_output_voltage_configs = arr;
    _analog_output_voltage_configs_count = idx;

    debug(tag, "Loaded %d analog output voltage configs from string", _analog_output_voltage_configs_count);

    return SUCCESS;
}

int control_logic_analog_output_voltage_configs_set(const char *json_string)
{
    int ret = SUCCESS;

    // load from string
    if (_analog_output_voltage_configs_load_from_string(json_string) != SUCCESS) {
        error(tag, "load analog input voltage configs from string failed");
        return FAIL;
    }

    // Save the provided json_string to the analog voltage input configs file
    if (_save_string_to_file(CONFIG_ANALOG_OUTPUT_VOLTAGE_CONFIGE_PATH, json_string) != SUCCESS) {
        ret = FAIL;
    }

    return ret;
}

analog_config_t* control_logic_analog_output_current_configs_get(int *config_count)
{
    if (config_count) {
        *config_count = _analog_output_current_configs_count;
    }

    return _analog_output_current_configs;
}

static int _analog_output_current_configs_init(void)
{
    return _analog_output_current_configs_load_from_file(CONFIG_ANALOG_OUTPUT_CURRENT_CONFIGE_PATH);
}

static int _analog_output_current_configs_load_from_file(const char *path)
{
    int ret = SUCCESS;
    
    long json_len = 0;
    char *json_text = control_logic_read_entire_file(path, &json_len);
    if (!json_text) {
        error(tag, "Failed to open/read analog voltage input config file: %s", path);
        return FAIL;
    }

    // load from string
    if (_analog_output_current_configs_load_from_string(json_text) != SUCCESS) {
        error(tag, "load analog output current configs from string failed");
        ret = FAIL;
    }

    free(json_text);

    return ret;
}

static int _analog_output_current_configs_load_from_string(const char *json_string)
{
    // validate
    cJSON *root = cJSON_Parse(json_string);
    
    if (!root) {
        error(tag, "Failed to parse JSON config");
        return FAIL;
    }

    // check if is array
    cJSON *configs = NULL;
    if (cJSON_IsArray(root)) {
        configs = root;
    } else {
        cJSON_Delete(root);
        error(tag, "JSON config must be an array or contain a 'configs' array");
        return FAIL;
    }

    // check array size
    int total = cJSON_GetArraySize(configs);
    if (total <= 0) {
        // clean config
        _analog_output_current_configs_clean();
        cJSON_Delete(root);
        return SUCCESS;
    }

    // allocate memory
    analog_config_t *arr = (analog_config_t*)platform_slow_calloc((size_t)total, sizeof(analog_config_t));
    if (!arr) {
        cJSON_Delete(root);
        error(tag, "Out of memory allocating analog current input configs");
        return FAIL;
    }

    // foreach devices
    int idx = 0;
    cJSON *it = NULL;
    cJSON_ArrayForEach(it, configs) {
        if (!cJSON_IsObject(it)) continue;

        cJSON *port = cJSON_GetObjectItemCaseSensitive(it, "board");
        cJSON *channel = cJSON_GetObjectItemCaseSensitive(it, "channel");
        cJSON *sensor_type = cJSON_GetObjectItemCaseSensitive(it, "sensor_type");
        cJSON *name = cJSON_GetObjectItemCaseSensitive(it, "name");

        if (!cJSON_IsNumber(port) || !cJSON_IsNumber(channel) || !cJSON_IsNumber(sensor_type)) {
            error(tag, "Invalid analog current input config entry (missing numeric fields)");
            continue;
        }

        analog_config_t c = {0};
        c.port = (uint8_t)port->valueint;
        c.channel = (uint8_t)channel->valueint;
        c.sensor_type = (uint8_t)sensor_type->valueint;
        
        // copy name
        if (name != NULL && name->valuestring != NULL) {
            strncpy(c.name, name->valuestring, sizeof(c.name)-1);
            c.name[sizeof(c.name)-1] = '\0';
        }

        arr[idx++] = c;
    }

    // free json root
    cJSON_Delete(root);

    // clean config
    _analog_output_current_configs_clean();
    
    // set config
    _analog_output_current_configs = arr;
    _analog_output_current_configs_count = idx;

    debug(tag, "Loaded %d analog output current configs from string", _analog_output_current_configs_count);

    return SUCCESS;
}

int control_logic_analog_output_current_configs_set(const char *json_string)
{
    int ret = SUCCESS;

    // load from string
    if (_analog_output_current_configs_load_from_string(json_string) != SUCCESS) {
        error(tag, "load analog output current configs from string failed");
        return FAIL;
    }

    // Save the provided json_string to the analog voltage input configs file
    if (_save_string_to_file(CONFIG_ANALOG_OUTPUT_CURRENT_CONFIGE_PATH, json_string) != SUCCESS) {
        ret = FAIL;
    }

    return ret;
}

enum SYSTEM_CONFIGS_SUPPORTED_FIELD {
    SYSTEM_CONFIGS_SUPPORTED_FIELD_NOT_SUPPORTED = 0,
    SYSTEM_CONFIGS_SUPPORTED_FIELD_MACHINE_TYPE,
};

static int _system_configs_get_supported_field(cJSON* p_object) 
{
    if (strcmp("machine_type", p_object->string) == 0 ) {
        if (p_object->type == cJSON_String) {
            return SYSTEM_CONFIGS_SUPPORTED_FIELD_MACHINE_TYPE;
        }
        return SYSTEM_CONFIGS_SUPPORTED_FIELD_NOT_SUPPORTED;
    }

    return SYSTEM_CONFIGS_SUPPORTED_FIELD_NOT_SUPPORTED;
}

static int _system_configs_load_from_string(const char *json_string)
{    
    // validate
    cJSON *root = cJSON_Parse(json_string);
    if (!root) {
        error(tag, "Failed to parse JSON config");
        return FAIL;
    }
    
    // allocate memory
    system_config_t *arr = (system_config_t*)platform_slow_calloc((size_t)1, sizeof(system_config_t));
    if (!arr) {
        cJSON_Delete(root);
        _system_configs_clean();
        error(tag, "Out of memory allocating system configs");
        return FAIL;
    }

    // Parse the "SystemConfigs" object from the root
    cJSON *jsonObject = root->child;
    for ( ; jsonObject != NULL; jsonObject = jsonObject->next) {
        int supported_field = _system_configs_get_supported_field(jsonObject);
        switch (supported_field) {
            case SYSTEM_CONFIGS_SUPPORTED_FIELD_MACHINE_TYPE:
                strncpy(arr->machine_type, jsonObject->valuestring, sizeof(arr->machine_type)-1);
                arr->machine_type[sizeof(arr->machine_type)-1] = '\0';
                break;
            default:
                break;
        }
    }

    // free json root
    cJSON_Delete(root);

    // clean config
    _system_configs_clean();

    // set config
    _system_configs = arr;

    return SUCCESS;
}

static int _system_configs_dump(void)
{
    if (_system_configs != NULL) {
        debug(tag, "machine_type: %s", _system_configs->machine_type);
    }

    return SUCCESS;
}

static int _system_configs_init(void)
{
    int ret = SUCCESS;

    if (_system_configs_load_from_file(CONFIG_SYSTEM_CONFIGS_PATH) != SUCCESS) {
        // debug(tag, "load system configs from file failed, try to load from file: %s", CONFIG_SYSTEM_CONFIGS_PATH);
        // if (_system_configs_load_from_file(CONFIG_SYSTEM_CONFIGS_PATH_DEFAULT) != SUCCESS) {
        //     // error(tag, "load system configs from file failed");
        //     return FAIL;
        // }
    }

    // dump config
    _system_configs_dump();

    return ret;
}

static int _system_configs_load_from_file(const char *path)
{
    int ret = SUCCESS;
    
    long json_len = 0;
    char *json_text = control_logic_read_entire_file(path, &json_len);
    if (!json_text) {
        error(tag, "Failed to open/read system configs file: %s", path);
        return FAIL;
    }

    // load from string
    if (_system_configs_load_from_string(json_text) != SUCCESS) {
        error(tag, "load system configs from string failed");
        ret = FAIL;
    }

    free(json_text);

    return ret;
}

int control_logic_system_configs_set(const char *json_string)
{
    int ret = SUCCESS;
    
    // load from string
    if (_system_configs_load_from_string(json_string) == SUCCESS) {
        control_logic_manager_reinit();
        ret = _save_string_to_file(CONFIG_SYSTEM_CONFIGS_PATH, json_string);
    } else {
        error(tag, "load system configs from string failed");
        return FAIL;
    }

   return ret;
}

system_config_t* control_logic_system_configs_get(void)
{
    return _system_configs;
}

/**
 * @brief 獲取機器類型配置
 *
 * 功能說明:
 * 從系統配置中獲取當前的機器類型。
 *
 * @return control_logic_machine_type_t 機器類型
 *         - CONTROL_LOGIC_MACHINE_TYPE_LS80: LS80機型
 *         - CONTROL_LOGIC_MACHINE_TYPE_LX1400: LX1400機型
 *         - CONTROL_LOGIC_MACHINE_TYPE_DEFAULT: 預設機型(未配置時)
 *
 * 實現邏輯:
 * 1. 檢查系統配置是否存在
 * 2. 比對配置中的機器類型字符串
 * 3. 返回對應的機器類型枚舉值
 */
control_logic_machine_type_t control_logic_config_get_machine_type(void)
{
    control_logic_machine_type_t machine_type = CONTROL_LOGIC_MACHINE_TYPE_DEFAULT;

    if (_system_configs != NULL) {
        /* 檢查是否為 LS80 機型 */
        if (strncmp(_system_configs->machine_type, "LS80", sizeof(_system_configs->machine_type)) == 0) {
            machine_type = CONTROL_LOGIC_MACHINE_TYPE_LS80;
        }
        /* 檢查是否為 LX1400 機型 */
        else if (strncmp(_system_configs->machine_type, "LX1400", sizeof(_system_configs->machine_type)) == 0) {
            machine_type = CONTROL_LOGIC_MACHINE_TYPE_LX1400;
        }
        /* 檢查是否為 LS300D 機型 */
        else if (strncmp(_system_configs->machine_type, "LS300D", sizeof(_system_configs->machine_type)) == 0) {
            machine_type = CONTROL_LOGIC_MACHINE_TYPE_LS300D;
        }
    }

    return machine_type;
}

int control_logic_register_save_to_file(const char *file_path, const char *jsonPayload)
{
    int ret = SUCCESS;

    // save to json payload to file
    if (_save_string_to_file(file_path, jsonPayload) != SUCCESS) {
        ret = FAIL;
    }

    return ret;
}

int control_logic_register_load_from_file(const char *file_path, control_logic_register_t *register_list, 
                                          uint32_t list_size) 
{
    int ret = SUCCESS;

    long json_len = 0;

    // read entire file
    char *json_text = control_logic_read_entire_file(file_path, &json_len);
    if (json_text == NULL) {
        error(tag, "Failed to read register array from file %s", file_path);
        return FAIL;
    }

    // load from json
    ret = control_logic_register_load_from_json(json_text, register_list, list_size);

    // free json text
    free(json_text);

    return ret;
}

int control_logic_register_load_from_json(const char *jsonPayload, control_logic_register_t *register_list, 
                                          uint32_t list_size) 
{
    int ret = SUCCESS;
    
    // validate json payload
    cJSON *registers_array = cJSON_Parse(jsonPayload);
    if (registers_array == NULL) {
        error(tag, "Failed to parse JSON payload");
        return FAIL;
    }

    // check each register in registers_array
    int registers_array_size = cJSON_GetArraySize(registers_array);
    if (registers_array_size <= 0) {
        error(tag, "Invalid registers array size");
        cJSON_Delete(registers_array);
        return FAIL;
    }

    // foreach register in registers_array
    for (int i = 0; i < registers_array_size; i++) {
        // get json item
        cJSON *jsonItem = cJSON_GetArrayItem(registers_array, i);
        if (cJSON_IsObject(jsonItem)) {
            // get name object
            cJSON *jsonName = cJSON_GetObjectItemCaseSensitive(jsonItem, "name");
            if (cJSON_IsString(jsonName)) {
                // check name matches in register_list
                for (uint32_t i = 0; i < list_size; i++) {
                    // check name matches current jsonName
                    if (register_list[i].name != NULL && 
                        strcmp(jsonName->valuestring, register_list[i].name) == 0) {
                        // get the address value
                        cJSON *jsonAddress = cJSON_GetObjectItemCaseSensitive(jsonItem, "address");
                        if (cJSON_IsNumber(jsonAddress)) {
                            // set the new address
                            if (register_list[i].address_ptr != NULL) {
                                *(register_list[i].address_ptr) = (int32_t)jsonAddress->valueint;
                            }
                        }
                        break; // found, no need to check further
                    }
                }
            }
        }
    }

    // free json root
    cJSON_Delete(registers_array);

    return ret;
}

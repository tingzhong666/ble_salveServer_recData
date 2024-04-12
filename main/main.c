#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatts_api.h"
#include <esp_bt.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <stdio.h>
#include <string.h>

#define DEVICE_NAME "esp32测试"
#define APP_ID 0

static esp_ble_adv_data_t adv_data_cfg = {
    .set_scan_rsp = true,
    .include_name = true,
    .include_txpower = false,
    .min_interval = 20,
    .max_interval = 40,
    .appearance = 0,
    .manufacturer_len = 0,
    .p_manufacturer_data = 0,
    .service_data_len = 0,
    .p_service_data = 0,
    .service_uuid_len = 0,
    .p_service_uuid = 0,
    .flag = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT
};

static esp_ble_adv_params_t adv_cfg = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY
};

typedef enum {
    // 服务 test
    test_service,

    // 特性 nb1
    nb1,
    // 值
    nb1_val
} attr_index;

uint16_t uuid_type_service = 0x2800;
uint16_t uuid_type_character = 0x2803;

uint16_t test_service_uuid = 0xAAAA;

#define nb1_character_val_length 512
uint16_t nb1_character_perm = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE;
uint16_t nb1_character_val_uuid = 0xAAAB;
uint8_t nb1_character_val[nb1_character_val_length] = { 0 };

const esp_gatts_attr_db_t attr_table[] = {
    [test_service] = { .attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
        .att_desc.uuid_length = 2,
        .att_desc.uuid_p = (uint8_t*)&uuid_type_service,
        .att_desc.perm = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
        .att_desc.value = (uint8_t*)&test_service_uuid,
        .att_desc.length = 2,
        .att_desc.max_length = 2 },
    [nb1] = { .attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
        .att_desc.uuid_length = 2,
        .att_desc.uuid_p = (uint8_t*)&uuid_type_character,
        .att_desc.perm = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
        .att_desc.value = (uint8_t*)&nb1_character_perm,
        .att_desc.max_length = 2,
        .att_desc.length = 2 },
    [nb1_val] = { .attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
        .att_desc.uuid_length = 2,
        .att_desc.uuid_p = (uint8_t*)&nb1_character_val_uuid,
        .att_desc.perm = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
        .att_desc.value = nb1_character_val,
        .att_desc.max_length = nb1_character_val_length,
        .att_desc.length = nb1_character_val_length }
};

uint8_t test_service1 = 0;
uint16_t test_service1_handle[sizeof(attr_table)];

static void
gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param)
{
}
void gatts_cb(esp_gatts_cb_event_t event,
    esp_gatt_if_t gatts_if,
    esp_ble_gatts_cb_param_t* param)
{
    switch (event) {
    // 连接时
    case ESP_GATTS_CONNECT_EVT:
        // 停止广播
        esp_ble_gap_stop_advertising();
        // 创建属性表
        esp_ble_gatts_create_attr_tab(attr_table, gatts_if, sizeof(attr_table), test_service1);
        // 创建服务
        // esp_ble_gatts_create_service(gatts_if, test_service1, );
        break;
    // 断连时
    case ESP_GATTS_DISCONNECT_EVT:
        esp_ble_gap_start_advertising(&adv_cfg);
        break;

    // 创建属性表后
    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
        // 启动服务
        memcpy(test_service1_handle, param->add_attr_tab.handles, sizeof(attr_table));
        esp_ble_gatts_start_service(test_service1_handle[test_service]);
        break;
    // 写入事件
    case ESP_GATTS_WRITE_EVT:
        if (param->write.handle == test_service1_handle[nb1_val]) {
            ESP_EARLY_LOGI("dbeug", "数据变化: %s", param->write.value);
        }
        break;

    default:
        break;
    }
}
void app_main(void)
{
    esp_err_t ret;
    // FALSH
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    // 控制器
    esp_bt_controller_config_t controller_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&controller_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);

    // bluedroid
    // esp_bluedroid_init();
    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    esp_bluedroid_init_with_cfg(&bluedroid_cfg);
    esp_bluedroid_enable();

    // gap回调
    esp_ble_gap_register_callback(gap_cb);
    // gatts回调
    esp_ble_gatts_register_callback(gatts_cb);
    // MTU
    esp_ble_gatt_set_local_mtu(500);

    // 注册应用
    esp_ble_gatts_app_register(APP_ID);
    esp_ble_gap_set_device_name(DEVICE_NAME);
    // 配置广播数据包
    esp_ble_gap_config_adv_data(&adv_data_cfg);
    // 开启广播
    esp_ble_gap_start_advertising(&adv_cfg);
    ESP_EARLY_LOGI("dbeug", "esp32运行.......................");

    // == 这几个没必要用事件 先留着 后序有问题时再调试
    // 配置广播数据包时 开启广播
    // 开启广播时

    // 创建属性表后 启动服务
    // 启动服务后 注册应用
    // ==

    // 连接创建后 保存连接实例
    // 属性设置事件 发送消息队列

    // 接收任务 调试串口打印接收的消息
}
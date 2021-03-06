/* FreeModbus Slave Example ESP32

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "esp_err.h"
#include "sdkconfig.h"
#include "esp_log.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_task_wdt.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_timer.h"

#include "mdns.h"
#include "esp_netif.h"
//#include "protocol_examples_common.h"

#include "mbcontroller.h"  // for mbcontroller defines and api
#include "modbus_params.h" // for modbus parameters structures

#include "enc28j60ethernet.h"

#define MB_TCP_PORT_NUMBER (CONFIG_FMB_TCP_PORT_DEFAULT)
#define MB_MDNS_PORT (502)

// Defines below are used to define register start address for each type of Modbus registers
#define MB_REG_DISCRETE_INPUT_START (0x0000)
#define MB_REG_INPUT_START (0x0000)
#define MB_REG_HOLDING_START (0x0000)
#define MB_REG_COILS_START (0x0000)

#define MB_PAR_INFO_GET_TOUT (10) // Timeout for get parameter info
#define MB_CHAN_DATA_MAX_VAL (10)
#define MB_CHAN_DATA_OFFSET (1.1f)

#define MB_READ_MASK (MB_EVENT_INPUT_REG_RD | MB_EVENT_HOLDING_REG_RD | MB_EVENT_DISCRETE_RD | MB_EVENT_COILS_RD)
#define MB_WRITE_MASK (MB_EVENT_HOLDING_REG_WR | MB_EVENT_COILS_WR)
#define MB_READ_WRITE_MASK (MB_READ_MASK | MB_WRITE_MASK)

#define SLAVE_TAG "SLAVE_TEST"

static portMUX_TYPE param_lock = portMUX_INITIALIZER_UNLOCKED;

#if CONFIG_MB_MDNS_IP_RESOLVER

#define MB_ID_BYTE0(id) ((uint8_t)(id))
#define MB_ID_BYTE1(id) ((uint8_t)(((uint16_t)(id) >> 8) & 0xFF))
#define MB_ID_BYTE2(id) ((uint8_t)(((uint32_t)(id) >> 16) & 0xFF))
#define MB_ID_BYTE3(id) ((uint8_t)(((uint32_t)(id) >> 24) & 0xFF))

#define MB_ID2STR(id) MB_ID_BYTE0(id), MB_ID_BYTE1(id), MB_ID_BYTE2(id), MB_ID_BYTE3(id)

#if CONFIG_FMB_CONTROLLER_SLAVE_ID_SUPPORT
#define MB_DEVICE_ID (uint32_t) CONFIG_FMB_CONTROLLER_SLAVE_ID
#endif

#define MB_SLAVE_ADDR (CONFIG_MB_SLAVE_ADDR)

#define MB_MDNS_INSTANCE(pref) pref "mb_slave_tcp"

// convert mac from binary format to string
static inline char *gen_mac_str(const uint8_t *mac, char *pref, char *mac_str)
{
    sprintf(mac_str, "%s%02X%02X%02X%02X%02X%02X", pref, MAC2STR(mac));
    return mac_str;
}

static inline char *gen_id_str(char *service_name, char *slave_id_str)
{
    sprintf(slave_id_str, "%s%02X%02X%02X%02X", service_name, MB_ID2STR(MB_DEVICE_ID));
    return slave_id_str;
}

static inline char *gen_host_name_str(char *service_name, char *name)
{
    sprintf(name, "%s_%02X", service_name, MB_SLAVE_ADDR);
    return name;
}

static void start_mdns_service()
{
    char temp_str[32] = {0};
    uint8_t sta_mac[6] = {0};
    bool b = ethConnected();
    if (b == false)
    {
        ESP_ERROR_CHECK(esp_read_mac(sta_mac, ESP_MAC_WIFI_STA);
    }
    else
    {
        ESP_ERROR_CHECK(esp_read_mac(sta_mac, ESP_MAC_ETH);
    }
    char *hostname = gen_host_name_str(MB_MDNS_INSTANCE(""), temp_str);
    //initialize mDNS
    ESP_ERROR_CHECK(mdns_init());
    //set mDNS hostname (required if you want to advertise services)
    ESP_ERROR_CHECK(mdns_hostname_set(hostname));
    ESP_LOGI(SLAVE_TAG, "mdns hostname set to: [%s]", hostname);

    //set default mDNS instance name
    ESP_ERROR_CHECK(mdns_instance_name_set(MB_MDNS_INSTANCE("esp32_")));

    //structure with TXT records
    mdns_txt_item_t serviceTxtData[] = {
        {"board", "esp32"}};

    //initialize service
    ESP_ERROR_CHECK(mdns_service_add(hostname, "_modbus", "_tcp", MB_MDNS_PORT, serviceTxtData, 1));
    //add mac key string text item
    ESP_ERROR_CHECK(mdns_service_txt_item_set("_modbus", "_tcp", "mac", gen_mac_str(sta_mac, "\0", temp_str)));
    //add slave id key txt item
    ESP_ERROR_CHECK(mdns_service_txt_item_set("_modbus", "_tcp", "mb_id", gen_id_str("\0", temp_str)));
}

#endif

void mb_setup_input_data(float Temp, float Wet, short int pressure, short int noise, short int dust0,
                         short int dust1, short int dust2, short int light, short int light_2, short int blink,
                         short int CO2, float VOC, short int voc_accur, float EMnoise, float EMnoise_last,
                         float acceleration, float co, float no2, float nh3, float c2h5oh, float h2,
                         float ch4, float c3h8, float c4h10)
{
    input_reg_params.Temp = Temp;
    input_reg_params.Wet = Wet;
    input_reg_params.pressure = pressure;
    input_reg_params.noise = noise;
    input_reg_params.dust0 = dust0;
    input_reg_params.dust1 = dust1;
    input_reg_params.dust2 = dust2;
    input_reg_params.light = light;
    input_reg_params.light_2 = light_2;
    input_reg_params.blink = blink;
    input_reg_params.CO2 = CO2;
    input_reg_params.VOC = VOC;
    input_reg_params.voc_accur = voc_accur;
    input_reg_params.EMnoise = EMnoise;
    input_reg_params.EMnoise_last = EMnoise_last;
    input_reg_params.acceleration = acceleration;
    input_reg_params.co = co;
    input_reg_params.no2 = no2;
    input_reg_params.nh3 = nh3;
    input_reg_params.c2h5oh = c2h5oh;
    input_reg_params.h2 = h2;
    input_reg_params.ch4 = ch4;
    input_reg_params.c3h8 = c3h8;
    input_reg_params.c4h10 = c4h10;
}

// Set register values into known state
static void setup_reg_data(void)
{
    mb_setup_input_data(23.4, 3.3, 4, 23, 2, 2, 8, 43, 252, 1300, 32, 88.2, 1, 2, 2, 1.4, 52.2, 99.2, 32.2, 42.3, 32.1, 4.5, 32.3, 2.3);
}
// An example application of Modbus slave. It is based on freemodbus stack.
// See deviceparams.h file for more information about assigned Modbus parameters.
// These parameters can be accessed from main application and also can be changed
// by external Modbus master host.
void app_main(void)
{

    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(result);
    //esp_netif_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

#if CONFIG_MB_MDNS_IP_RESOLVER
    start_mdns_service();
#endif

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    // ESP_ERROR_CHECK(example_connect());
    ethernetConnect();
    long time = esp_timer_get_time() / 1000;
    bool b = ethConnected();
    int dx = esp_timer_get_time() / 1000 - time;
    while (b != true)
    {
        vTaskDelay(1);
        dx = esp_timer_get_time() / 1000 - time;
        printf("%i\n", dx);
        b = ethConnected();
        if (dx > 5000)
        {
            break;
        }
    }
    if (b == false)
    {
        ethernetDisconnect();
        wifi_start();
    }
    time = esp_timer_get_time() / 1000;
    dx = esp_timer_get_time() / 1000 - time;
    while (dx < 10000){
        vTaskDelay(1);
        dx = esp_timer_get_time() / 1000 - time;
    }

    // ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    // Set UART log level
    esp_log_level_set(SLAVE_TAG, ESP_LOG_INFO);
    void *mbc_slave_handler = NULL;

    ESP_ERROR_CHECK(mbc_slave_init_tcp(&mbc_slave_handler)); // Initialization of Modbus controller

    mb_param_info_t reg_info;               // keeps the Modbus registers access information
    mb_register_area_descriptor_t reg_area; // Modbus register area descriptor structure

    mb_communication_info_t comm_info = {0};
    comm_info.ip_port = MB_TCP_PORT_NUMBER;
#if !CONFIG_EXAMPLE_CONNECT_IPV6
    comm_info.ip_addr_type = MB_IPV4;
#else
    comm_info.ip_addr_type = MB_IPV6;
#endif
    comm_info.ip_mode = MB_MODE_TCP;
    comm_info.ip_addr = NULL;
    comm_info.ip_netif_ptr = (void *)get_netif(); //(void*)get_example_netif();
    // Setup communication parameters and start stack
    ESP_ERROR_CHECK(mbc_slave_setup((void *)&comm_info));

    // The code below initializes Modbus register area descriptors
    // for Modbus Holding Registers, Input Registers, Coils and Discrete Inputs
    // Initialization should be done for each supported Modbus register area according to register map.
    // When external master trying to access the register in the area that is not initialized
    // by mbc_slave_set_descriptor() API call then Modbus stack
    // will send exception response for this register area.
    reg_area.type = MB_PARAM_HOLDING;               // Set type of register area
    reg_area.start_offset = MB_REG_HOLDING_START;   // Offset of register area in Modbus protocol
    reg_area.address = (void *)&holding_reg_params; // Set pointer to storage instance
    reg_area.size = sizeof(holding_reg_params);     // Set the size of register storage instance
    ESP_ERROR_CHECK(mbc_slave_set_descriptor(reg_area));

    // Initialization of Input Registers area
    reg_area.type = MB_PARAM_INPUT;
    reg_area.start_offset = MB_REG_INPUT_START;
    reg_area.address = (void *)&input_reg_params;
    reg_area.size = sizeof(input_reg_params);
    ESP_ERROR_CHECK(mbc_slave_set_descriptor(reg_area));

    // Initialization of Coils register area
    reg_area.type = MB_PARAM_COIL;
    reg_area.start_offset = MB_REG_COILS_START;
    reg_area.address = (void *)&coil_reg_params;
    reg_area.size = sizeof(coil_reg_params);
    ESP_ERROR_CHECK(mbc_slave_set_descriptor(reg_area));

    // Initialization of Discrete Inputs register area
    reg_area.type = MB_PARAM_DISCRETE;
    reg_area.start_offset = MB_REG_DISCRETE_INPUT_START;
    reg_area.address = (void *)&discrete_reg_params;
    reg_area.size = sizeof(discrete_reg_params);
    ESP_ERROR_CHECK(mbc_slave_set_descriptor(reg_area));

    setup_reg_data(); // Set values into known state

    // Starts of modbus controller and stack
    ESP_ERROR_CHECK(mbc_slave_start());

    ESP_LOGI(SLAVE_TAG, "Modbus slave stack initialized.");
    ESP_LOGI(SLAVE_TAG, "Start modbus test...");
    // The cycle below will be terminated when parameter holding_data0
    // incremented each access cycle reaches the CHAN_DATA_MAX_VAL value.
    int i = 0;
    for (; holding_reg_params.holding_data0 < MB_CHAN_DATA_MAX_VAL;)
    {
        mb_setup_input_data(23.4, 3.3, 4, 23, 100, i, 100, 43, 252, 1300, 32, 88.2, 1, 2, 2, 1.4, 52.2, 99.2, 32.2, 42.3, 32.1, 4.5, 32.3, 2.3);
        // Check for read/write events of Modbus master for certain events
        mb_event_group_t event = mbc_slave_check_event(MB_READ_WRITE_MASK);
        const char *rw_str = (event & MB_READ_MASK) ? "READ" : "WRITE";
        // Filter events and process them accordingly
        if (event & (MB_EVENT_HOLDING_REG_WR | MB_EVENT_HOLDING_REG_RD))
        {
            // Get parameter information from parameter queue
            ESP_ERROR_CHECK(mbc_slave_get_param_info(&reg_info, MB_PAR_INFO_GET_TOUT));
            ESP_LOGI(SLAVE_TAG, "HOLDING %s (%u us), ADDR:%u, TYPE:%u, INST_ADDR:0x%.4x, SIZE:%u",
                     rw_str,
                     (uint32_t)reg_info.time_stamp,
                     (uint32_t)reg_info.mb_offset,
                     (uint32_t)reg_info.type,
                     (uint32_t)reg_info.address,
                     (uint32_t)reg_info.size);
            if (reg_info.address == (uint8_t *)&holding_reg_params.holding_data0)
            {
                portENTER_CRITICAL(&param_lock);
                holding_reg_params.holding_data0 += MB_CHAN_DATA_OFFSET;
                if (holding_reg_params.holding_data0 >= (MB_CHAN_DATA_MAX_VAL - MB_CHAN_DATA_OFFSET))
                {
                    coil_reg_params.coils_port1 = 0xFF;
                }
                portEXIT_CRITICAL(&param_lock);
            }
        }
        else if (event & MB_EVENT_INPUT_REG_RD)
        {
            //coil_reg_params.coils_port1 = 0xFF;
            i++;
            ESP_ERROR_CHECK(mbc_slave_get_param_info(&reg_info, MB_PAR_INFO_GET_TOUT));
            ESP_LOGI(SLAVE_TAG, "INPUT READ (%u us), ADDR:%u, TYPE:%u, INST_ADDR:0x%.4x, SIZE:%u",
                     (uint32_t)reg_info.time_stamp,
                     (uint32_t)reg_info.mb_offset,
                     (uint32_t)reg_info.type,
                     (uint32_t)reg_info.address,
                     (uint32_t)reg_info.size);
        }
        else if (event & MB_EVENT_DISCRETE_RD)
        {
            ESP_ERROR_CHECK(mbc_slave_get_param_info(&reg_info, MB_PAR_INFO_GET_TOUT));
            ESP_LOGI(SLAVE_TAG, "DISCRETE READ (%u us): ADDR:%u, TYPE:%u, INST_ADDR:0x%.4x, SIZE:%u",
                     (uint32_t)reg_info.time_stamp,
                     (uint32_t)reg_info.mb_offset,
                     (uint32_t)reg_info.type,
                     (uint32_t)reg_info.address,
                     (uint32_t)reg_info.size);
        }
        else if (event & (MB_EVENT_COILS_RD | MB_EVENT_COILS_WR))
        {
            ESP_ERROR_CHECK(mbc_slave_get_param_info(&reg_info, MB_PAR_INFO_GET_TOUT));
            ESP_LOGI(SLAVE_TAG, "COILS %s (%u us), ADDR:%u, TYPE:%u, INST_ADDR:0x%.4x, SIZE:%u",
                     rw_str,
                     (uint32_t)reg_info.time_stamp,
                     (uint32_t)reg_info.mb_offset,
                     (uint32_t)reg_info.type,
                     (uint32_t)reg_info.address,
                     (uint32_t)reg_info.size);
            if (coil_reg_params.coils_port1 == 0xFF)
                break;
        }
    }
    // Destroy of Modbus controller on alarm
    ESP_LOGI(SLAVE_TAG, "Modbus controller destroyed.");
    vTaskDelay(100);
    ESP_ERROR_CHECK(mbc_slave_destroy());
#if CONFIG_MB_MDNS_IP_RESOLVER
    mdns_free();
#endif
}

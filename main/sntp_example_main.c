/* LwIP SNTP example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "soc/rtc_wdt.h"
#include "esp_clk.h"

#include <esp_partition.h>
#include <soc/rtc.h>
#include <rom/rtc.h>
#include <soc/soc.h>
#include "lwip/err.h"
#include "esp_ota_ops.h"
#include "time.h"
#include "bootloader_common.h"
#include "esp_system.h"
#include "rom/rtc.h"

/* The examples use simple WiFi configuration that you can set via
   'make menuconfig'.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_WIFI_SSID CONFIG_WIFI_SSID
#define EXAMPLE_WIFI_PASS CONFIG_WIFI_PASSWORD

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

static const char *TAG = "example";

/* Variable holding number of times ESP32 restarted since first boot.
 * It is placed into RTC memory using RTC_DATA_ATTR and
 * maintains its value when ESP32 wakes from deep sleep.
 */
RTC_DATA_ATTR static int boot_count;

/*static void obtain_time(void);
static void initialize_sntp(void);*/
static void initialise_wifi(void);
static esp_err_t event_handler(void *ctx, system_event_t *event);


void esp_task_wdt_isr_user_handler(void)
{
    ets_printf("ssssssssssss esp_task_wdt_int_user_func User Message\n");
}

void adjtimeTask1 ()
{
    typedef int64_t timestamp_t;
    timestamp_t esp_timestamp = 0;
    timestamp_t esp_timestamp_lastframe = 0;
    ESP_LOGI(TAG, "adjtimeTask1 _ ");
    for(;;) { // frame start
      esp_timestamp_lastframe = esp_timestamp;
      esp_timestamp = esp_timer_get_time();

      if (esp_timestamp < esp_timestamp_lastframe) {
        timestamp_t shift_us = esp_timestamp - esp_timestamp_lastframe;

        ESP_LOGE(TAG, "ESP system clock SHIFT: %lli usecs", shift_us);
      }

      if(esp_timestamp%40 == 0) {
          vTaskDelay((esp_timestamp%300 + 1) / portTICK_PERIOD_MS);
      }
    }
}
#include "driver/rtc_io.h"
#include "driver/gpio.h"

#define POWER_DETECT_GPIO 4

static void IRAM_ATTR pwr_detect_isr_handler(void* arg)   //IRAM_ATTR defined in task.h
{
    uint32_t gpio_num = (uint32_t) arg;
    ets_printf("gpio %d triggered]\n",gpio_num);
    ets_delay_us(100000);
}

void set_gpio()
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_NEGEDGE;//GPIO_INTR_ANYEDGE;//GPIO_PIN_INTR_POSEDGE;
    io_conf.pin_bit_mask = (1ULL<<POWER_DETECT_GPIO);
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;

    //enable pull up for NEG EDGE interrupt detection, enable pull down for POS EDGE interrupt detection
    //for POWER DETECT PIN both can be disabled?
    io_conf.pull_up_en = 0;
    io_conf.pull_down_en = 0;

    gpio_config(&io_conf);

    //install gpio isr service
    gpio_install_isr_service(0);
    gpio_isr_handler_add(POWER_DETECT_GPIO, pwr_detect_isr_handler, (void*) POWER_DETECT_GPIO);
}
#include "esp_image_format.h"
//extern esp_app_desc_t esp_app_desc;
// __attribute__((section(".rodata_custom_desc")))
void app_main()
{
    ++boot_count;
    ESP_LOGI(TAG, "Boot count: %d", boot_count);
/*
    ESP_EARLY_LOGI(TAG, "Application information");
    ESP_EARLY_LOGI(TAG, "Firmware version: %s", esp_app_desc.version);
    ESP_EARLY_LOGI(TAG, "Secure version:   %x", esp_app_desc.secure_version);
    ESP_EARLY_LOGI(TAG, "Compile time:     %s", esp_app_desc.time);
    ESP_EARLY_LOGI(TAG, "Compile date:     %s", esp_app_desc.date);
    ESP_EARLY_LOGI(TAG, "ESP-IDF:          %s", esp_app_desc.idf_ver);
*/
    struct timeval t_set = {
            .tv_sec = 1538643600,
            .tv_usec = 0,
    };
    settimeofday(&t_set, NULL);

    set_gpio();
    xTaskCreate(adjtimeTask1, "adjtimeTask1", 2048, NULL, 5, NULL);
    xTaskCreate(adjtimeTask1, "adjtimeTask1", 2048, NULL, 4, NULL);
    xTaskCreate(adjtimeTask1, "adjtimeTask1", 2048, NULL, 5, NULL);
    xTaskCreate(adjtimeTask1, "adjtimeTask1", 2048, NULL, 4, NULL);
    xTaskCreate(adjtimeTask1, "adjtimeTask1", 2048, NULL, 5, NULL);
    xTaskCreate(adjtimeTask1, "adjtimeTask1", 2048, NULL, 4, NULL);
    initialise_wifi();

/*
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900)) {
        ESP_LOGI(TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
        obtain_time();
        // update 'now' variable with current time
        time(&now);
    }
    char strftime_buf[64];

    // Set timezone to Eastern Standard Time and print local time
    setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in New York is: %s", strftime_buf);

    // Set timezone to China Standard Time
    setenv("TZ", "CST-8", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);

    const int deep_sleep_sec = 10;
    ESP_LOGI(TAG, "Entering deep sleep for %d seconds", deep_sleep_sec);
    //esp_deep_sleep(1000000LL * deep_sleep_sec);
*/
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    struct timeval tv_delta;
    struct timeval tv_outdelta;
    struct timeval now = {0};
    gettimeofday(&now, NULL);
    ESP_LOGI(TAG, "Time gettimeofday  [%ld sec,  %ld us]", now.tv_sec, now.tv_usec);
    /*if (now.tv_sec < 1000000L) {
        printf("app_main: Set current time \n");
        now.tv_sec = 1527876000L;
        settimeofday(&now, NULL);
    }*/
    typedef struct {
        uint32_t ota_seq;
        uint8_t  seq_label[24];
        uint32_t crc;                /* CRC32 of ota_seq field only */
    } ota_select;
    static ota_select otadata[2];

    /*{
        xSemaphoreHandle exit_sema;
        TaskHandle_t th;
        xTaskCreatePinnedToCore(task1_func, "task1_func", 2048, &exit_sema, 5, &th, 0);
        intr_handler_t handler;
        intr_handle_t *ret_handle;

        esp_err_t err =  esp_intr_alloc(ETS_GPIO_INTR_SOURCE, ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_IRAM , gpio_isr_handler, NULL, &ret_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_intr_alloc err=%d", err);
        }
    }*/
    tv_delta.tv_sec = 15;
    tv_delta.tv_usec = 0;
    adjtime(&tv_delta, NULL);
    int i = 0;
    ESP_LOGI(TAG, "RTC_WDT is on = %d (%d, %d)___ %d", rtc_wdt_is_on(), rtc_get_reset_reason(0), rtc_get_reset_reason(1), 0/*esp_reset_reason()*/);
    while(1) {
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        i++;
        adjtime(NULL, &tv_outdelta);
        ESP_LOGI(TAG, "Waiting ... tv_outdelta.sec %ld, %d", tv_outdelta.tv_sec, i);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        if(i == 10) {
            //esp_restart();
            //esp_sleep_enable_timer_wakeup(10000000);
            //esp_light_sleep_start();
        }
        if(i < 8) {
            //rtc_wdt_feed();
        } else {
            ESP_LOGI(TAG, "Waiting for restart due to rtc_wdt");
        }
        if(0 /*i == 30*/) {
            static spi_flash_mmap_memory_t ota_data_map;
            const void *result = NULL;

            const esp_partition_t *find_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);

            esp_partition_mmap(find_partition, 0, find_partition->size, SPI_FLASH_MMAP_DATA, &result, &ota_data_map);
            memcpy(&otadata[0], result, sizeof(ota_select));
            memcpy(&otadata[1], result + SPI_FLASH_SEC_SIZE, sizeof(ota_select));
            spi_flash_munmap(ota_data_map);

            int sec_id = 0;
            if (otadata[sec_id].ota_seq == 0xFFFFFFFF) {
                otadata[sec_id].ota_seq = 1;
                otadata[sec_id].crc = bootloader_common_ota_select_crc(&otadata[sec_id]);
                esp_partition_erase_range(find_partition, sec_id * SPI_FLASH_SEC_SIZE, SPI_FLASH_SEC_SIZE);
                esp_partition_write(find_partition, SPI_FLASH_SEC_SIZE * sec_id, &otadata[sec_id].ota_seq, sizeof(ota_select));
            }
        }
    }
}


/*
static void obtain_time(void)
{
    ESP_ERROR_CHECK( nvs_flash_init() );
    initialise_wifi();
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);
    initialize_sntp();

    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;
    while(timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    ESP_ERROR_CHECK( esp_wifi_stop() );
}
*//*
static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}
*/
static void initialise_wifi(void)
{
    ESP_ERROR_CHECK( nvs_flash_init() );
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_WIFI_SSID,
            .password = EXAMPLE_WIFI_PASS,
        },
    };
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                            false, true, portMAX_DELAY);
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        // This is a workaround as ESP32 WiFi libs don't currently
        //   auto-reassociate.
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

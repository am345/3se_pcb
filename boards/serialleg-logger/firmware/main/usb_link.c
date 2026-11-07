#include "usb_link.h"

#include "esp_log.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "usb/usb_host.h"
#include "usb/cdc_acm_host.h"
#include "sdkconfig.h"
#include "soc/usb_dwc_struct.h"

#define USB_LIB_TASK_PRIO   20
#define USB_LINK_TASK_PRIO  6
#define USB_IN_BUFFER_SIZE  512

static const char *TAG = "usb";

static RingbufHandle_t s_ring;
static SemaphoreHandle_t s_disconnected;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static usb_link_status_t s_status;

void usb_link_get_status(usb_link_status_t *out)
{
    portENTER_CRITICAL(&s_lock);
    *out = s_status;
    portEXIT_CRITICAL(&s_lock);
}

void usb_link_get_port(usb_link_port_t *out)
{
    /* 只读寄存器；PrtLnSts 位 0 为 D+、位 1 为 D- 的逻辑电平 */
    usb_dwc_hprt_reg_t hprt = {.val = USB_DWC.hprt_reg.val};

    out->connected = hprt.prtconnsts;
    out->powered = hprt.prtpwr;
    out->dp = hprt.prtlnsts & 1U;
    out->dm = (hprt.prtlnsts >> 1) & 1U;
}

/* 在 CDC 驱动任务中调用，只做非阻塞入队，不能等待写卡 */
static bool on_rx(const uint8_t *data, size_t len, void *arg)
{
    bool ok = xRingbufferSend(s_ring, data, len, 0) == pdTRUE;

    portENTER_CRITICAL(&s_lock);
    s_status.rx_bytes += len;
    s_status.last_rx_tick = xTaskGetTickCount();
    if (!ok) {
        s_status.drop_bytes += len;
        s_status.drop_events++;
    }
    portEXIT_CRITICAL(&s_lock);
    return true;
}

static void on_event(const cdc_acm_host_dev_event_data_t *event, void *ctx)
{
    switch (event->type) {
    case CDC_ACM_HOST_DEVICE_DISCONNECTED:
        ESP_LOGW(TAG, "设备断开");
        portENTER_CRITICAL(&s_lock);
        s_status.connected = false;
        portEXIT_CRITICAL(&s_lock);
        cdc_acm_host_close(event->data.cdc_hdl);
        xSemaphoreGive(s_disconnected);
        break;
    case CDC_ACM_HOST_ERROR:
        ESP_LOGE(TAG, "CDC 传输错误 %d", event->data.error);
        break;
    case CDC_ACM_HOST_SERIAL_STATE:
        ESP_LOGD(TAG, "串口状态 0x%04X", event->data.serial_state.val);
        break;
    default:
        break;
    }
}

/* 新设备枚举后记录实际 VID/PID，便于在元数据里核对接的是哪块板 */
static void on_new_dev(usb_device_handle_t dev)
{
    const usb_device_desc_t *desc;

    if (usb_host_get_device_descriptor(dev, &desc) == ESP_OK) {
        portENTER_CRITICAL(&s_lock);
        s_status.vid = desc->idVendor;
        s_status.pid = desc->idProduct;
        portEXIT_CRITICAL(&s_lock);
        ESP_LOGI(TAG, "发现设备 %04X:%04X", desc->idVendor, desc->idProduct);
    }
}

static void usb_lib_task(void *arg)
{
    while (true) {
        uint32_t flags;
        usb_host_lib_handle_events(portMAX_DELAY, &flags);
        if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            usb_host_device_free_all();
        }
    }
}

static void usb_link_task(void *arg)
{
    const cdc_acm_host_device_config_t dev_cfg = {
        .connection_timeout_ms = 1000,
        .out_buffer_size = 0,       /* 只读：不向主控发送数据 */
        .in_buffer_size = USB_IN_BUFFER_SIZE,
        .event_cb = on_event,
        .data_cb = on_rx,
        .user_arg = NULL,
    };

    ESP_LOGI(TAG, "等待 CDC 设备 %04X:%04X（0 表示任意）",
             CONFIG_LOGGER_USB_VID, CONFIG_LOGGER_USB_PID);

    while (true) {
        cdc_acm_dev_hdl_t dev = NULL;

        /* 打开前清掉旧信号，避免打开后立即断开的通知被误清 */
        xSemaphoreTake(s_disconnected, 0);
        esp_err_t err = cdc_acm_host_open(CONFIG_LOGGER_USB_VID, CONFIG_LOGGER_USB_PID, 0, &dev_cfg, &dev);
        if (err != ESP_OK) {
            if (err != ESP_ERR_NOT_FOUND) {
                ESP_LOGW(TAG, "打开设备失败：%s", esp_err_to_name(err));
                vTaskDelay(pdMS_TO_TICKS(500));
            }
            continue;
        }

        /* 打开后接收传输已提交，立即标记连接，记录任务才不会丢开头的数据 */
        portENTER_CRITICAL(&s_lock);
        s_status.connected = true;
        uint32_t session = ++s_status.session;
        portEXIT_CRITICAL(&s_lock);
        ESP_LOGI(TAG, "CDC 已连接，会话 %lu", (unsigned long)session);

        /* 部分 CDC 设备在 DTR 置位后才发送；ST 虚拟串口不依赖它，失败不影响接收 */
        err = cdc_acm_host_set_control_line_state(dev, true, false);
        if (err != ESP_OK) {
            ESP_LOGD(TAG, "设置 DTR 失败：%s", esp_err_to_name(err));
        }

        xSemaphoreTake(s_disconnected, portMAX_DELAY);
    }
}

esp_err_t usb_link_start(RingbufHandle_t ring)
{
    s_ring = ring;
    s_disconnected = xSemaphoreCreateBinary();
    if (s_disconnected == NULL) {
        return ESP_ERR_NO_MEM;
    }

    const usb_host_config_t host_cfg = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    esp_err_t err = usb_host_install(&host_cfg);
    if (err != ESP_OK) {
        return err;
    }
    if (xTaskCreate(usb_lib_task, "usb_lib", 4096, NULL, USB_LIB_TASK_PRIO, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    const cdc_acm_host_driver_config_t drv_cfg = {
        .driver_task_stack_size = 4096,
        .driver_task_priority = 10,
        .xCoreID = 0,
        .new_dev_cb = on_new_dev,
    };
    err = cdc_acm_host_install(&drv_cfg);
    if (err != ESP_OK) {
        return err;
    }

    if (xTaskCreate(usb_link_task, "usb_link", 4096, NULL, USB_LINK_TASK_PRIO, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

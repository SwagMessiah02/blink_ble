#include <stdio.h>
#include "btstack.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/btstack_cyw43.h"
#include "blink_ble.h"

#define LED_PIN 12
#define HEARTBEAT_PERIOD_MS 500
#define HCI_TRUE_CONNECTION_COMPLETE 0x3E // Magic number. Don't ask why

// Advertizing data must be 31 bytes long at top
static uint8_t adv_data[] = {                                      
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, 0x06,  
    0xA, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'B', 'l', 'i', 'n', 'k', '_', 'b', 'l', 'e',  
    0x03, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, 0x1a, 0x18,
};

static const uint8_t adv_data_len = sizeof(adv_data);
static btstack_timer_source_t heartbeat;
static btstack_packet_callback_registration_t bstack_callback;

bool le_connected = false;
bool led_state = false;
bool cyw_led_state = false;
bool le_notification_enabled;

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(size);
    UNUSED(channel);
    bd_addr_t local_addr;

    if (packet_type != HCI_EVENT_PACKET) return;

    uint8_t event_type = hci_event_packet_get_type(packet);

    switch (event_type) {
        case BTSTACK_EVENT_STATE:
            if(btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;

            gap_local_bd_addr(local_addr);
            printf("Local address: %s.\n", bd_addr_to_str(local_addr));

            uint16_t adv_interval_min = 800; // 800 * 0,625 = 500 ms
            uint16_t adv_interval_max = 800;
            bd_addr_t null_addr; 

            memset(null_addr, 0, 6);

            gap_advertisements_set_params(adv_interval_min, adv_interval_max, 0, 0, null_addr, 0x07, 0x00); // 0x07 = 0b0111 --- 3 adv channels enabled (37, 38, 39)

            assert(adv_data_len <= 31); 

            gap_advertisements_set_data(adv_data_len, (uint8_t*)adv_data); // Send advertizing data to client
            gap_advertisements_enable(1);
            
            break;
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            if(!le_connected) break;
            le_connected = false; printf("DISCONNECTED\n"); 
            le_notification_enabled = false;
            break;
        case HCI_TRUE_CONNECTION_COMPLETE:
            if(le_connected) break;
            le_connected = true; printf("CONNECTED\n"); 
            break;
        default:
            break;
    }
}

static uint16_t read_callback(hci_con_handle_t con_handler, uint16_t att_handler, uint16_t offset, uint8_t *buffer, uint16_t size) {
    UNUSED(con_handler);

    if(att_handler == ATT_CHARACTERISTIC_12345678_1234_5678_1234_56789ABCFFFF_01_VALUE_HANDLE) {
        uint8_t data = led_state == true ? 0x01 : 0x00;
        return att_read_callback_handle_blob((uint8_t*)&led_state, sizeof(led_state), offset, buffer, size);
    }

    return 0;
}

static int write_callback(hci_con_handle_t con_handler, uint16_t att_handler, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t size) {
    UNUSED(offset);
    UNUSED(transaction_mode);
    UNUSED(size);

    if(att_handler != ATT_CHARACTERISTIC_12345678_1234_5678_1234_56789ABCDEF1_01_VALUE_HANDLE) return 0;

    led_state = (little_endian_read_16(buffer, 0) & 1) == 1 ? true : false;
    gpio_put(LED_PIN, led_state);

    return 0;
}

static void hearthbeat_handler(struct btstack_timer_source *ts) {
    if(le_connected || cyw_led_state) {
        cyw_led_state = !cyw_led_state;
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, cyw_led_state);
    }

    btstack_run_loop_set_timer(ts, HEARTBEAT_PERIOD_MS);
    btstack_run_loop_add_timer(ts);
}   

void setup() {
    if (cyw43_arch_init()) {
        printf("failed to initialise cyw43_arch\n");
    }

    printf("cyw43_arch initialized\n");

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    l2cap_init();
    sm_init();

    att_server_init(profile_data, read_callback, write_callback);

    bstack_callback.callback = &packet_handler;
    hci_add_event_handler(&bstack_callback);
    att_server_register_packet_handler(packet_handler);

    heartbeat.process = &hearthbeat_handler;
    btstack_run_loop_set_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
    btstack_run_loop_add_timer(&heartbeat);

    hci_power_control(HCI_POWER_ON);
}

int main()
{
    stdio_init_all();

    setup();

    while (true) {
        if(getchar() == 'E') break;
        async_context_poll(cyw43_arch_async_context());
        async_context_wait_for_work_until(cyw43_arch_async_context(), at_the_end_of_time);
    }

    printf("Finishing the program\n");

    att_server_deinit();

    sm_deinit();
    l2cap_deinit();

    cyw43_arch_deinit();
}
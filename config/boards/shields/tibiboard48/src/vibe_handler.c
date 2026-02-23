/* config/src/vibe_handler.c - マルチポイント対応版 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zmk/event_manager.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zmk/ble.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_NODE_EXISTS(DT_ALIAS(vib0))

    #define VIB_NODE DT_ALIAS(vib0)
    static const struct gpio_dt_spec motor = GPIO_DT_SPEC_GET(VIB_NODE, gpios);

    static bool is_layer_vibe_enabled = true;

    /* --- バイブレーション制御 --- */
    static void vib_off_handler(struct k_work *work) {
        gpio_pin_set_dt(&motor, 0);
    }
    K_WORK_DELAYABLE_DEFINE(vib_off_work, vib_off_handler);

    static void vib_start(int ms) {
        k_work_cancel_delayable(&vib_off_work);
        gpio_pin_set_dt(&motor, 1);
        k_work_schedule(&vib_off_work, K_MSEC(ms));
    }

    /* --- 接続完了バイブ用の遅延実行ワーカー --- */
    /* これを作ることで、プロファイル切替時の「0.3秒」と重ならないようにします */
    static void connected_vibe_handler(struct k_work *work) {
        vib_start(1000); /* 1.0秒 */
    }
    K_WORK_DELAYABLE_DEFINE(connected_vibe_work, connected_vibe_handler);


    /* --- イベントハンドラ --- */

    /* 1. 新規接続時 (実際にリンクが確立した時) */
    static void on_connected(struct bt_conn *conn, uint8_t err) {
        if (err) return;
        /* 新しく繋がった時は、即座に1秒振動 */
        vib_start(1000);
    }

    BT_CONN_CB_DEFINE(conn_callbacks) = {
        .connected = on_connected,
    };

    /* 2. 各種イベントリスナー */
    #if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
        #include <zmk/events/layer_state_changed.h>
        #include <zmk/events/ble_active_profile_changed.h>
        #include <zmk/events/keycode_state_changed.h>

        static int vibration_listener(const zmk_event_t *eh) {
            
            /* A. キーコード (F19など) */
            const struct zmk_keycode_state_changed *keycode_ev = as_zmk_keycode_state_changed(eh);
            if (keycode_ev && keycode_ev->state) { 
                if (keycode_ev->usage_page == 0x07) { 
                    switch (keycode_ev->keycode) {
                        case 0x6E: /* F19 */
                            is_layer_vibe_enabled = !is_layer_vibe_enabled;
                            vib_start(1000); 
                            break;
                        /* テスト用 */
                        case 0x6F: vib_start(300); break;
                        case 0x70: vib_start(600); break;
                        case 0x71: vib_start(1000); break;
                    }
                }
                return ZMK_EV_EVENT_BUBBLE;
            }

            /* B. レイヤー変更 */
            const struct zmk_layer_state_changed *layer_ev = as_zmk_layer_state_changed(eh);
            if (layer_ev) {
                if (is_layer_vibe_enabled && layer_ev->state) { 
                    vib_start(250);
                }
                return ZMK_EV_EVENT_BUBBLE;
            }

            /* C. プロファイル変更 (ここが今回のキモ！) */
            const struct zmk_ble_active_profile_changed *profile_ev = as_zmk_ble_active_profile_changed(eh);
            if (profile_ev) {
                
                /* まずは切り替え合図：0.3秒 */
                vib_start(300);

                /* もし切り替え先が「すでに繋がっている(マルチポイント)」場合 */
                if (zmk_ble_active_profile_is_connected()) {
                    /* 0.3秒の振動が終わった頃(500ms後)に、追撃で1.0秒の振動を予約する */
                    k_work_schedule(&connected_vibe_work, K_MSEC(500));
                }
                
                return ZMK_EV_EVENT_BUBBLE;
            }
            return ZMK_EV_EVENT_BUBBLE;
        }

        ZMK_LISTENER(vibration_listener, vibration_listener);
        ZMK_SUBSCRIPTION(vibration_listener, zmk_layer_state_changed);
        ZMK_SUBSCRIPTION(vibration_listener, zmk_ble_active_profile_changed);
        ZMK_SUBSCRIPTION(vibration_listener, zmk_keycode_state_changed);

    #endif

    /* 初期化 */
    static int vibration_init(const struct device *dev) {
        if (!gpio_is_ready_dt(&motor)) { return -ENODEV; }
        gpio_pin_configure_dt(&motor, GPIO_OUTPUT_INACTIVE);
        
        vib_start(250); 
        return 0;
    }
    SYS_INIT(vibration_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);

#endif

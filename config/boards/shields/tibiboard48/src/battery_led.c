#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
#include <zmk/battery.h>

/* XIAO BLEのLED定義 */
static const struct gpio_dt_spec led_red   = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec led_blue  = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);

/* 表示時間 (5秒) */
#define LED_SHOW_TIME 5000

static struct k_work_delayable led_off_work;
static struct k_work_delayable boot_show_work;
static struct k_work_delayable rainbow_work; /* 追加: 7色アニメーション用 */

static uint8_t rainbow_step = 0; /* アニメーションの現在のステップ */

/* LEDをすべて消す関数 */
static void all_leds_off(void) {
    if (device_is_ready(led_red.port))   gpio_pin_set_dt(&led_red, 0);
    if (device_is_ready(led_green.port)) gpio_pin_set_dt(&led_green, 0);
    if (device_is_ready(led_blue.port))  gpio_pin_set_dt(&led_blue, 0);
}

/* 7色アニメーションを処理する関数 */
static void rainbow_handler(struct k_work *work) {
    all_leds_off();
    
    /* ステップに応じて7色を切り替える */
    switch (rainbow_step % 7) {
        case 0: /* 赤 */
            gpio_pin_set_dt(&led_red, 1); 
            break;
        case 1: /* 黄色 (赤+緑) */
            gpio_pin_set_dt(&led_red, 1); 
            gpio_pin_set_dt(&led_green, 1); 
            break;
        case 2: /* 緑 */
            gpio_pin_set_dt(&led_green, 1); 
            break;
        case 3: /* シアン/水色 (緑+青) */
            gpio_pin_set_dt(&led_green, 1); 
            gpio_pin_set_dt(&led_blue, 1); 
            break;
        case 4: /* 青 */
            gpio_pin_set_dt(&led_blue, 1); 
            break;
        case 5: /* マゼンタ/紫 (赤+青) */
            gpio_pin_set_dt(&led_red, 1); 
            gpio_pin_set_dt(&led_blue, 1); 
            break;
        case 6: /* 白 (全部) */
            gpio_pin_set_dt(&led_red, 1); 
            gpio_pin_set_dt(&led_green, 1); 
            gpio_pin_set_dt(&led_blue, 1); 
            break;
    }
    
    rainbow_step++;
    
    /* 200ミリ秒後に次の色へ変更する予約 */
    k_work_schedule(&rainbow_work, K_MSEC(200));
}

/* タイマー経過後にLEDを消す処理 */
static void led_off_handler(struct k_work *work) {
    /* 5秒経ったらアニメーションも強制終了させる */
    k_work_cancel_delayable(&rainbow_work);
    all_leds_off();
}

/* メイン処理：バッテリー残量に応じて色を変える */
static void show_battery_level(void) {
    uint8_t level = zmk_battery_state_of_charge();
    
    /* 既存のタイマーやアニメーションをリセットして一旦消灯 */
    k_work_cancel_delayable(&led_off_work);
    k_work_cancel_delayable(&rainbow_work);
    all_leds_off();

    /* ==============================================
     * 色分けロジック (4段階)
     * ============================================== */
    if (level >= 75) {
        /* 100% 〜 75%: 7色に光るアニメーションを開始 */
        rainbow_step = 0;
        k_work_schedule(&rainbow_work, K_NO_WAIT); /* すぐにアニメーション開始 */

    } else if (level >= 50) {
        /* 74% 〜 50%: 青 */
        gpio_pin_set_dt(&led_blue, 1);

    } else if (level >= 25) {
        /* 49% 〜 25%: 黄色 (赤 + 緑) */
        gpio_pin_set_dt(&led_red, 1);
        gpio_pin_set_dt(&led_green, 1);

    } else {
        /* 24% 〜 0%: 赤 */
        gpio_pin_set_dt(&led_red, 1);
    }

    /* 指定時間(5秒)後にすべて消す予約 */
    k_work_schedule(&led_off_work, K_MSEC(LED_SHOW_TIME));
}

static void boot_show_handler(struct k_work *work) {
    show_battery_level();
}

static int battery_led_init(void) {
    if (!device_is_ready(led_red.port)) return -ENODEV;
    
    /* 初期状態の設定 */
    gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_blue, GPIO_OUTPUT_INACTIVE);

    /* 念のため消灯 */
    all_leds_off();
    
    k_work_init_delayable(&led_off_work, led_off_handler);
    k_work_init_delayable(&boot_show_work, boot_show_handler);
    k_work_init_delayable(&rainbow_work, rainbow_handler); /* 初期化を追加 */

    /* 起動1秒後に自動表示 */
    k_work_schedule(&boot_show_work, K_MSEC(1000));

    return 0;
}

SYS_INIT(battery_led_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

// SkyRizz E32 — Mic + Speaker bring-up test (standalone).
// Schematic-confirmed pins (page 6): NS4168 CTRL=3V3 always-on,
//   BCLK=GPIO0(BOOT), LRCLK=GPIO38, SDATA=GPIO45(I2S_SDI), MCLK=GPIO3.
//   ES7243E mic SDOUT=GPIO39(I2S_SDO). Speaker on SP1 (VOP/VON).
// Raw Serial is intentional: no Kairo runtime/logger here.
#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s_std.h>
#include <driver/gpio.h>
#include <esp_rom_sys.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>
#include <cstdio>

static constexpr int PIN_SDA = 47, PIN_SCL = 48;
static constexpr int PIN_MCLK = 3, PIN_BCLK = 0, PIN_WS = 38, PIN_DIN = 39, PIN_DOUT = 45;
static constexpr uint8_t ES7243E_ADDR = 0x11;
static constexpr uint32_t SAMPLE_RATE = 16000;

static i2s_chan_handle_t txCh_ = nullptr;
static i2s_chan_handle_t rxCh_ = nullptr;

static const uint8_t ES7243E_INIT[][2] = {
    {0x01,0x3A},{0x00,0x80},{0xF9,0x00},{0x04,0x02},{0x04,0x01},{0xF9,0x01},{0x00,0x1E},
    {0x01,0x00},{0x02,0x00},{0x03,0x20},{0x04,0x01},{0x0D,0x00},{0x05,0x00},{0x06,0x03},
    {0x07,0x00},{0x08,0xFF},{0x09,0xCA},{0x0A,0x85},{0x0B,0x00},{0x0E,0xBF},{0x0F,0x80},
    {0x14,0x0C},{0x15,0x0C},{0x17,0x02},{0x18,0x26},{0x19,0x77},{0x1A,0xF4},{0x1B,0x66},
    {0x1C,0x44},{0x1E,0x00},{0x1F,0x0C},{0x20,0x18},{0x21,0x18},{0x00,0x80},{0x01,0x3A},
    {0x16,0x3F},{0x16,0x00},{0x20,0x18},{0x21,0x18},{0x00,0x80},{0x01,0x3A},{0x16,0x3F},{0x16,0x00},
};

#define CHK(call) do { esp_err_t e = (call); \
    printf("  %-40s -> %d (%s)\n", #call, (int)e, esp_err_to_name(e)); } while(0)

static void i2sInit() {
    i2s_chan_config_t ch = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ch.auto_clear = false;   // EXPERIMENT: was true — repeat last buffer on underrun
    CHK(i2s_new_channel(&ch, &txCh_, &rxCh_));

    i2s_std_clk_config_t clk = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE);
    clk.mclk_multiple = I2S_MCLK_MULTIPLE_256;

    i2s_std_config_t cfg = {
        .clk_cfg  = clk,
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = (gpio_num_t)PIN_MCLK,
            .bclk = (gpio_num_t)PIN_BCLK,
            .ws   = (gpio_num_t)PIN_WS,
            .dout = (gpio_num_t)PIN_DOUT,
            .din  = (gpio_num_t)PIN_DIN,
            .invert_flags = {false, false, false},
        },
    };
    printf("[I2S] init returns (watch for non-zero = silent failure):\n");
    CHK(i2s_channel_init_std_mode(txCh_, &cfg));
    CHK(i2s_channel_init_std_mode(rxCh_, &cfg));
    CHK(i2s_channel_enable(txCh_));
    CHK(i2s_channel_enable(rxCh_));
}

static void playTone(uint16_t freqHz, uint16_t ms, int32_t amplitude) {
    const uint32_t spc = SAMPLE_RATE / freqHz;
    const uint32_t totalFrames = (uint32_t)SAMPLE_RATE * ms / 1000;
    static int32_t buf[256 * 2];
    uint32_t written = 0; size_t totalOut = 0; esp_err_t lastErr = ESP_OK;
    while (written < totalFrames) {
        uint32_t n = 0;
        while (n < 256 && written < totalFrames) {
            int32_t s = ((written % spc) < spc / 2) ? amplitude : -amplitude;
            buf[n * 2] = s; buf[n * 2 + 1] = s;
            n++; written++;
        }
        size_t out = 0;
        lastErr = i2s_channel_write(txCh_, buf, n * 2 * sizeof(int32_t), &out, pdMS_TO_TICKS(300));
        totalOut += out;
        if (lastErr != ESP_OK) break;
    }
    printf("[SPK] write err=%d (%s) bytesOut=%u/%u\n", (int)lastErr, esp_err_to_name(lastErr),
           (unsigned)totalOut, (unsigned)(totalFrames * 2 * sizeof(int32_t)));
}

void setup() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    delay(400);
    printf("\n\n===== SkyRizz E32 AUDIO bring-up (instrumented) =====\n");

    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(100000);

    i2sInit();
    printf("[I2S] master: MCLK=%d BCLK=%d WS=%d DIN=%d DOUT=%d\n",
           PIN_MCLK, PIN_BCLK, PIN_WS, PIN_DIN, PIN_DOUT);
    delay(20);

    Wire.beginTransmission(ES7243E_ADDR);
    printf("[ES7243E] probe @0x11 ACK=%d\n", (Wire.endTransmission() == 0));
    int fails = 0;
    const size_t n = sizeof(ES7243E_INIT) / sizeof(ES7243E_INIT[0]);
    for (size_t i = 0; i < n; i++) {
        Wire.beginTransmission(ES7243E_ADDR);
        Wire.write(ES7243E_INIT[i][0]); Wire.write(ES7243E_INIT[i][1]);
        if (Wire.endTransmission() != 0) fails++;
    }
    printf("[ES7243E] init: %u writes, %d failed\n", (unsigned)n, fails);

    // ===== A/B ISOLATION TEST =====================================
    // RX (mic) stays enabled the whole time → BCLK/WS keep running, so NS4168
    // is clocked in BOTH phases. The ONLY thing that changes is what's on
    // GPIO45 (SDATA). This splits "physical chain dead" from "I2S-TX broken".

    // ---- PHASE A: I2S-TX drives GPIO45 with a real 1 kHz tone (6 s) --------
    printf("\n########## PHASE A: I2S-TX TONE on GPIO45 (6s) — LISTEN ##########\n");
    for (int i = 0; i < 8; i++) {
        printf("[A] i2s tone burst %d/8\n", i + 1);
        playTone(1000, 700, 0x40000000);   // ~0.7s each → ~6s total
    }
    printf("[A] done. Heard a clean tone? (we expect SILENCE here)\n");

    // ---- PHASE B: GPIO45 FLOATING + hammer GPIO46 (WS2812) crosstalk (6s) --
    // Reproduces the reference RX-only firmware's condition with OUR toolchain.
    printf("\n########## PHASE B: GPIO45 FLOAT + GPIO46 crosstalk (6s) — LISTEN ##########\n");
    i2s_channel_disable(txCh_);                       // stop I2S driving GPIO45
    gpio_reset_pin((gpio_num_t)PIN_DOUT);             // release pad → GPIO func
    gpio_set_direction((gpio_num_t)PIN_DOUT, GPIO_MODE_INPUT);  // high-Z / float
    gpio_reset_pin((gpio_num_t)46);
    gpio_set_direction((gpio_num_t)46, GPIO_MODE_OUTPUT);       // WS2812 line
    uint32_t start = millis();
    while (millis() - start < 6000) {
        gpio_set_level((gpio_num_t)46, 1);
        esp_rom_delay_us(150);
        gpio_set_level((gpio_num_t)46, 0);
        esp_rom_delay_us(150);
    }
    printf("[B] done. Heard NOISE/hiss/buzz? (this proves speaker+NS4168 are alive)\n");

    printf("\n===== VERDICT GUIDE =====\n");
    printf("A silent + B noise  -> physical chain OK, I2S-TX routing is the bug (software-fixable)\n");
    printf("A silent + B silent -> speaker/NS4168/trace dead (hardware)\n");
    printf("Power-cycle to repeat.\n");
}

void loop() {}

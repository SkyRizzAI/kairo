// SkyRizz E32 — Mic + Speaker bring-up test (standalone).
//
// Isolates the audio subsystem from the full firmware. Answers:
//   1. Does ES7243E ACK on I2C? (each init write reported)
//   2. Does the mic produce non-zero samples? (live peak printed)
//   3. Does the NS4168 speaker actually emit sound? (periodic loud tone)
//
// Hardware (from board pinout + schematic):
//   ES7243E mic ADC @ I2C 0x11, I2S RX on GPIO39 (SDO)
//   NS4168 speaker amp (CTRL tied high = always on), I2S TX on GPIO45 (SDI)
//   Shared I2S0 master clocks: MCLK=GPIO3, BCLK=GPIO0, LRCK/WS=GPIO38
//
// Raw Serial is intentional: no Kairo runtime/logger in this target.
#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s_std.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cmath>
#include <cstring>
#include <cstdio>

static constexpr int PIN_SDA = 47, PIN_SCL = 48;
static constexpr int PIN_MCLK = 3, PIN_BCLK = 0, PIN_WS = 38, PIN_DIN = 39, PIN_DOUT = 45;
static constexpr uint8_t ES7243E_ADDR = 0x11;
static constexpr uint32_t SAMPLE_RATE = 16000;

static i2s_chan_handle_t txCh_ = nullptr;
static i2s_chan_handle_t rxCh_ = nullptr;

// ES7243E init — verbatim from SkyRizz Rust reference es7243_init(), 24 dB gain.
static const uint8_t ES7243E_INIT[][2] = {
    {0x01,0x3A},{0x00,0x80},{0xF9,0x00},{0x04,0x02},{0x04,0x01},{0xF9,0x01},{0x00,0x1E},
    {0x01,0x00},{0x02,0x00},{0x03,0x20},{0x04,0x01},{0x0D,0x00},{0x05,0x00},{0x06,0x03},
    {0x07,0x00},{0x08,0xFF},{0x09,0xCA},{0x0A,0x85},{0x0B,0x00},{0x0E,0xBF},{0x0F,0x80},
    {0x14,0x0C},{0x15,0x0C},{0x17,0x02},{0x18,0x26},{0x19,0x77},{0x1A,0xF4},{0x1B,0x66},
    {0x1C,0x44},{0x1E,0x00},{0x1F,0x0C},{0x20,0x18},{0x21,0x18},{0x00,0x80},{0x01,0x3A},
    {0x16,0x3F},{0x16,0x00},{0x20,0x18},{0x21,0x18},{0x00,0x80},{0x01,0x3A},{0x16,0x3F},{0x16,0x00},
};

static void i2sInit() {
    i2s_chan_config_t ch = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ch.auto_clear = true;
    i2s_new_channel(&ch, &txCh_, &rxCh_);

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
    // Full-duplex: same config on both handles; IDF routes dout(TX)/din(RX).
    i2s_channel_init_std_mode(txCh_, &cfg);
    i2s_channel_init_std_mode(rxCh_, &cfg);
    i2s_channel_enable(txCh_);
    i2s_channel_enable(rxCh_);
}

static void playTone(uint16_t freqHz, uint16_t ms, int32_t amplitude) {
    const uint32_t spc = SAMPLE_RATE / freqHz;       // samples per cycle
    const uint32_t totalFrames = (uint32_t)SAMPLE_RATE * ms / 1000;
    static int32_t buf[256 * 2];                      // stereo frames
    // Build one chunk of square wave
    uint32_t written = 0;
    while (written < totalFrames) {
        uint32_t n = 0;
        while (n < 256 && written < totalFrames) {
            int32_t s = ((written % spc) < spc / 2) ? amplitude : -amplitude;
            buf[n * 2] = s; buf[n * 2 + 1] = s;
            n++; written++;
        }
        size_t out = 0;
        i2s_channel_write(txCh_, buf, n * 2 * sizeof(int32_t), &out, pdMS_TO_TICKS(300));
    }
}

void setup() {
    setvbuf(stdout, nullptr, _IONBF, 0);   // unbuffered → printf appears immediately
    delay(400);
    printf("\n\n===== SkyRizz E32 AUDIO bring-up test =====\n");

    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(100000);

    // I2S FIRST — ES7243E needs MCLK running before it ACKs on I2C
    i2sInit();
    printf("[I2S] full-duplex master started: MCLK=%d BCLK=%d WS=%d DIN=%d DOUT=%d\n",
                  PIN_MCLK, PIN_BCLK, PIN_WS, PIN_DIN, PIN_DOUT);
    delay(20);

    // I2C scan
    printf("[I2C] scan:");
    for (uint8_t a = 8; a < 0x78; a++) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) printf(" 0x%02X", a);
    }
    printf("\n");

    // ES7243E probe + init
    Wire.beginTransmission(ES7243E_ADDR);
    bool ack = (Wire.endTransmission() == 0);
    printf("[ES7243E] probe @0x11 ACK=%d\n", ack);

    int fails = 0;
    const size_t n = sizeof(ES7243E_INIT) / sizeof(ES7243E_INIT[0]);
    for (size_t i = 0; i < n; i++) {
        Wire.beginTransmission(ES7243E_ADDR);
        Wire.write(ES7243E_INIT[i][0]); Wire.write(ES7243E_INIT[i][1]);
        if (Wire.endTransmission() != 0) fails++;
    }
    printf("[ES7243E] init done: %u writes, %d failed\n", (unsigned)n, fails);

    // Startup beep — loud, so we KNOW if the speaker works at boot
    printf("[SPK] startup beep 880Hz 400ms (LOUD)...\n");
    playTone(880, 400, 0x40000000);  // ~50% full scale
    printf("[SPK] beep done. If silent → speaker/I2S TX problem.\n");
    printf("[MIC] now printing live levels. Make noise!\n");
}

void loop() {
    static int32_t buf[512];
    static uint32_t lastBeep = 0;

    // Read mic
    size_t bytesRead = 0;
    esp_err_t err = i2s_channel_read(rxCh_, buf, sizeof(buf), &bytesRead, pdMS_TO_TICKS(50));
    if (err == ESP_OK && bytesRead > 0) {
        size_t samples = bytesRead / sizeof(int32_t);
        int64_t peak = 0, sum = 0;
        for (size_t i = 0; i < samples; i++) {
            int32_t v24 = buf[i] >> 8;       // 24-bit MSB-justified → 24-bit
            int32_t a = v24 < 0 ? -v24 : v24;
            if (a > peak) peak = a;
            sum += a;
        }
        int avg = (int)(sum / (samples ? samples : 1));
        // Normalize peak to 0..100 against 24-bit full scale
        int pct = (int)(peak * 100 / 0x7FFFFF);
        // ASCII level bar
        char bar[41]; int fill = pct * 40 / 100; if (fill > 40) fill = 40;
        for (int i = 0; i < 40; i++) bar[i] = (i < fill) ? '#' : '.';
        bar[40] = 0;
        printf("[MIC] peak=%7ld avg=%6d %3d%% [%s]\n",
                      (long)peak, avg, pct, bar);
    } else {
        printf("[MIC] read err=%d bytes=%u\n", err, (unsigned)bytesRead);
    }

    // Periodic beep every 4s so we can correlate speaker output
    uint32_t now = millis();
    if (now - lastBeep >= 4000) {
        lastBeep = now;
        printf("[SPK] beep 440Hz 200ms\n");
        playTone(440, 200, 0x40000000);
    }
}

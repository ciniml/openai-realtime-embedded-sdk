#include <driver/i2s_std.h>
#include <opus.h>

#include "main.h"

#include <esp_log.h>
#include <M5Unified.h>

#include <cstdint>
#include <vector>
#include <sys/socket.h>

#define OPUS_OUT_BUFFER_SIZE 1276  // 1276 bytes is recommended by opus_encode
#define SAMPLE_RATE 8000
#define BUFFER_SAMPLES 320

static i2s_chan_handle_t s_i2s_tx_handle = nullptr;
static i2s_chan_handle_t s_i2s_rx_handle = nullptr;
static constexpr i2s_chan_handle_t get_i2s_tx_handle() { return s_i2s_tx_handle; }
static constexpr i2s_chan_handle_t get_i2s_rx_handle() { return s_i2s_rx_handle; }

#define RX_MCLK_PIN  CONFIG_MEDIA_I2S_RX_MCLK_PIN
#define RX_BCLK_PIN  CONFIG_MEDIA_I2S_RX_BCLK_PIN
#define RX_LRCLK_PIN CONFIG_MEDIA_I2S_RX_LRCLK_PIN
#define RX_DATA_PIN  CONFIG_MEDIA_I2S_RX_DATA_PIN

#define TX_MCLK_PIN  CONFIG_MEDIA_I2S_TX_MCLK_PIN
#define TX_BCLK_PIN  CONFIG_MEDIA_I2S_TX_BCLK_PIN
#define TX_LRCLK_PIN CONFIG_MEDIA_I2S_TX_LRCLK_PIN
#define TX_DATA_PIN  CONFIG_MEDIA_I2S_TX_DATA_PIN

#define OPUS_ENCODER_BITRATE 30000
#define OPUS_ENCODER_COMPLEXITY 0

constexpr const char *TAG = "media";

// UDP socket for audio data debugging
#ifdef CONFIG_MEDIA_ENABLE_DEBUG_AUDIO_UDP_CLIENT
static struct sockaddr_in s_debug_audio_in_dest_addr;
static struct sockaddr_in s_debug_audio_out_dest_addr;
static ssize_t s_debug_audio_sock;
#endif // CONFIG_MEDIA_ENABLE_DEBUG_AUDIO_UDP_CLIENT

// Initialization of AW88298 and ES7210 from M5Unified implementation.
constexpr std::uint8_t aw88298_i2c_addr = 0x36;
constexpr std::uint8_t es7210_i2c_addr = 0x40;
constexpr std::uint8_t aw9523_i2c_addr = 0x58;
static void aw88298_write_reg(std::uint8_t reg, std::uint16_t value)
{
  value = __builtin_bswap16(value);
  M5.In_I2C.writeRegister(aw88298_i2c_addr, reg, (const std::uint8_t*)&value, 2, 400000);
}

static void es7210_write_reg(std::uint8_t reg, std::uint8_t value)
{
  M5.In_I2C.writeRegister(es7210_i2c_addr, reg, &value, 1, 400000);
}

static void initialize_speaker_cores3()
{
  M5.In_I2C.bitOn(aw9523_i2c_addr, 0x02, 0b00000100, 400000);

  aw88298_write_reg( 0x61, 0x0673 );  // boost mode disabled 
  aw88298_write_reg( 0x04, 0x4040 );  // I2SEN=1 AMPPD=0 PWDN=0
  aw88298_write_reg( 0x05, 0x0008 );  // RMSE=0 HAGCE=0 HDCCE=0 HMUTE=0
  aw88298_write_reg( 0x06, 0x14C0 );  // INPLEV=0 (not attenuated), I2SRXEN=1 (enable), CHSEL=01 (left), I2SMD=00 (Philips Standard I2S), I2SFS=00 (16bit), I2SBCK=00 (32*fs), I2SSR=0000 (8kHz)
  aw88298_write_reg( 0x0C, 0x0064 );  // volume setting (full volume)
}

static void initialize_microphone_cores3()
{
  es7210_write_reg(0x00, 0xFF); // RESET_CTL
  struct __attribute__((packed)) reg_data_t
  {
    uint8_t reg;
    uint8_t value;
  };
  
  static constexpr reg_data_t data[] =
  {
    { 0x00, 0x41 }, // RESET_CTL
    { 0x01, 0x1f }, // CLK_ON_OFF
    { 0x06, 0x00 }, // DIGITAL_PDN
    { 0x07, 0x20 }, // ADC_OSR
    { 0x08, 0x10 }, // MODE_CFG
    { 0x09, 0x30 }, // TCT0_CHPINI
    { 0x0A, 0x30 }, // TCT1_CHPINI
    { 0x20, 0x0a }, // ADC34_HPF2
    { 0x21, 0x2a }, // ADC34_HPF1
    { 0x22, 0x0a }, // ADC12_HPF2
    { 0x23, 0x2a }, // ADC12_HPF1
    { 0x02, 0xC1 },
    { 0x04, 0x01 },
    { 0x05, 0x00 },
    { 0x11, 0x60 },
    { 0x40, 0x42 }, // ANALOG_SYS
    { 0x41, 0x70 }, // MICBIAS12
    { 0x42, 0x70 }, // MICBIAS34
    { 0x43, 0x1B }, // MIC1_GAIN
    { 0x44, 0x1B }, // MIC2_GAIN
    { 0x45, 0x00 }, // MIC3_GAIN
    { 0x46, 0x00 }, // MIC4_GAIN
    { 0x47, 0x00 }, // MIC1_LP
    { 0x48, 0x00 }, // MIC2_LP
    { 0x49, 0x00 }, // MIC3_LP
    { 0x4A, 0x00 }, // MIC4_LP
    { 0x4B, 0x00 }, // MIC12_PDN
    { 0x4C, 0xFF }, // MIC34_PDN
    { 0x01, 0x14 }, // CLK_ON_OFF
  };
  for (auto& d: data)
  {
    es7210_write_reg(d.reg, d.value);
  }
}

void oai_init_audio_capture() {
#ifdef CONFIG_MEDIA_INIT_MICROPHONE_AND_SPEAKER
  ESP_LOGI(TAG, "Initializing microphone");
  initialize_microphone_cores3();
  ESP_LOGI(TAG, "Initializing speaker");
  initialize_speaker_cores3();
#endif

#ifdef CONFIG_MEDIA_ENABLE_DEBUG_AUDIO_UDP_CLIENT
  // Initialize UDP socket for debug.
  s_debug_audio_sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (s_debug_audio_sock < 0) {
    ESP_LOGE(TAG, "Failed to create socket");
    return;
  }
  
  s_debug_audio_in_dest_addr.sin_addr.s_addr = inet_addr(CONFIG_MEDIA_DEBUG_AUDIO_HOST);
  s_debug_audio_in_dest_addr.sin_family = AF_INET;
  s_debug_audio_in_dest_addr.sin_port = htons(CONFIG_MEDIA_DEBUG_AUDIO_IN_PORT);
  s_debug_audio_out_dest_addr.sin_addr.s_addr = inet_addr(CONFIG_MEDIA_DEBUG_AUDIO_HOST);
  s_debug_audio_out_dest_addr.sin_family = AF_INET;
  s_debug_audio_out_dest_addr.sin_port = htons(CONFIG_MEDIA_DEBUG_AUDIO_OUT_PORT);
#endif // CONFIG_MEDIA_ENABLE_DEBUG_AUDIO_UDP_CLIENT

  ESP_LOGI(TAG, "Initializing I2S for audio input/output");
  {
    i2s_chan_config_t chan_config = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    chan_config.auto_clear = true;
#ifdef CONFIG_MEDIA_I2S_RX_TX_SHARED
    ESP_ERROR_CHECK(i2s_new_channel(&chan_config, &s_i2s_tx_handle, &s_i2s_rx_handle));
#else // CONFIG_MEDIA_I2S_RX_TX_SHARED
    ESP_ERROR_CHECK(i2s_new_channel(&chan_config, &s_i2s_tx_handle, nullptr));
#endif // CONFIG_MEDIA_I2S_RX_TX_SHARED
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO ),
        .gpio_cfg = {
            .mclk = gpio_num_t(CONFIG_MEDIA_I2S_TX_MCLK_PIN),
            .bclk = gpio_num_t(CONFIG_MEDIA_I2S_TX_BCLK_PIN),
            .ws = gpio_num_t(CONFIG_MEDIA_I2S_TX_LRCLK_PIN),
            .dout = gpio_num_t(CONFIG_MEDIA_I2S_TX_DATA_PIN),
            .din = gpio_num_t(CONFIG_MEDIA_I2S_RX_DATA_PIN),
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    std_cfg.slot_cfg.data_bit_width = i2s_data_bit_width_t::I2S_DATA_BIT_WIDTH_16BIT;
    std_cfg.slot_cfg.slot_bit_width = i2s_slot_bit_width_t::I2S_SLOT_BIT_WIDTH_16BIT;
    std_cfg.slot_cfg.slot_mode = i2s_slot_mode_t::I2S_SLOT_MODE_MONO;
#ifdef CONFIG_MEDIA_I2S_TX_SLOT_LEFT_ONLY
    std_cfg.slot_cfg.slot_mask = i2s_std_slot_mask_t::I2S_STD_SLOT_LEFT;
#else // CONFIG_MEDIA_I2S_TX_SLOT_LEFT_ONLY
    std_cfg.slot_cfg.slot_mask = i2s_std_slot_mask_t::I2S_STD_SLOT_BOTH;
#endif // CONFIG_MEDIA_I2S_TX_SLOT_LEFT_ONLY
    std_cfg.slot_cfg.ws_width = 16;
    std_cfg.slot_cfg.ws_pol = false;
    std_cfg.slot_cfg.bit_shift = true;
#if SOC_I2S_HW_VERSION_1
    std_cfg.slot_cfg.msb_right = false;
#else
    std_cfg.slot_cfg.left_align = true;
    std_cfg.slot_cfg.big_endian = false;
    std_cfg.slot_cfg.bit_order_lsb = false;
#endif // SOC_I2S_HW_VERSION_1
    i2s_channel_init_std_mode(s_i2s_tx_handle, &std_cfg);
    i2s_channel_enable(s_i2s_tx_handle);
#ifdef CONFIG_MEDIA_I2S_RX_TX_SHARED
    i2s_channel_init_std_mode(s_i2s_rx_handle, &std_cfg);
    i2s_channel_enable(s_i2s_rx_handle);
#endif // CONFIG_MEDIA_I2S_RX_TX_SHARED    
  }

#ifndef CONFIG_MEDIA_I2S_RX_TX_SHARED
  {
    i2s_chan_config_t chan_config = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_config.auto_clear = true;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_config, nullptr, &s_i2s_rx_handle));
#ifdef CONFIG_MEDIA_I2S_RX_PDM
    i2s_pdm_rx_config_t pdm_rx_cfg = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = gpio_num_t(CONFIG_MEDIA_I2S_RX_LRCLK_PIN),
            .din = gpio_num_t(CONFIG_MEDIA_I2S_RX_DATA_PIN),
            .invert_flags = {
                .clk_inv = false,
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_pdm_rx_mode(s_i2s_rx_handle, &pdm_rx_cfg));
#else // CONFIG_MEDIA_I2S_RX_PDM
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO ),
        .gpio_cfg = {
            .mclk = gpio_num_t(CONFIG_MEDIA_I2S_RX_MCLK_PIN),
            .bclk = gpio_num_t(CONFIG_MEDIA_I2S_RX_BCLK_PIN),
            .ws = gpio_num_t(CONFIG_MEDIA_I2S_RX_LRCLK_PIN),
            .dout = gpio_num_t(I2S_PIN_NO_CHANGE),
            .din = gpio_num_t(CONFIG_MEDIA_I2S_RX_DATA_PIN),
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    i2s_channel_init_std_mode(s_i2s_rx_handle, &std_cfg);
#endif // CONFIG_MEDIA_I2S_RX_PDM
    i2s_channel_enable(s_i2s_rx_handle);
  }
#endif // CONFIG_MEDIA_I2S_RX_TX_SHARED
}

opus_int16 *output_buffer = NULL;
OpusDecoder *opus_decoder = NULL;

void oai_init_audio_decoder() {
  int decoder_error = 0;
  opus_decoder = opus_decoder_create(SAMPLE_RATE, 1, &decoder_error);
  if (decoder_error != OPUS_OK) {
    ESP_LOGE(TAG, "Failed to create OPUS decoder");
    return;
  }

  output_buffer = (opus_int16 *)malloc(BUFFER_SAMPLES * sizeof(opus_int16));
}

void oai_audio_decode(uint8_t *data, size_t size) {
  int decoded_size =
      opus_decode(opus_decoder, data, size, output_buffer, BUFFER_SAMPLES, 0);

  if (decoded_size > 0) {
#ifdef CONFIG_IDF_TARGET_ESP32
    for(size_t i = 0; i < decoded_size * sizeof(opus_int16)/4; i++) {
      const auto value = reinterpret_cast<std::uint32_t*>(output_buffer)[i];
      const auto high_word = value >> 16;
      const auto low_word = value & 0xFFFF;
      reinterpret_cast<std::uint32_t*>(output_buffer)[i] = (low_word << 16) | high_word;
    }
#endif // CONFIG_IDF_TARGET_ESP32
    std::size_t bytes_written = 0;
    if( esp_err_t err = i2s_channel_write(get_i2s_tx_handle(), output_buffer, decoded_size * sizeof(opus_int16),
              &bytes_written, portMAX_DELAY); err != ESP_OK ) {
      ESP_LOGE(TAG, "Failed to write audio data to I2S: %s", esp_err_to_name(err));
    }
#ifdef CONFIG_MEDIA_ENABLE_DEBUG_AUDIO_UDP_CLIENT
    sendto(s_debug_audio_sock, output_buffer, decoded_size * sizeof(opus_int16), 0, (struct sockaddr *)&s_debug_audio_out_dest_addr, sizeof(s_debug_audio_out_dest_addr));
#endif // CONFIG_MEDIA_ENABLE_DEBUG_AUDIO_UDP_CLIENT
  }
}

OpusEncoder *opus_encoder = NULL;
opus_int16 *encoder_input_buffer = NULL;
uint8_t *encoder_output_buffer = NULL;

void oai_init_audio_encoder() {
  int encoder_error;
  opus_encoder = opus_encoder_create(SAMPLE_RATE, 1, OPUS_APPLICATION_VOIP,
                                     &encoder_error);
  if (encoder_error != OPUS_OK) {
    ESP_LOGE(TAG, "Failed to create OPUS encoder");
    return;
  }

  if (opus_encoder_init(opus_encoder, SAMPLE_RATE, 1, OPUS_APPLICATION_VOIP) !=
      OPUS_OK) {
    ESP_LOGE(TAG, "Failed to initialize OPUS encoder");
    return;
  }

  opus_encoder_ctl(opus_encoder, OPUS_SET_BITRATE(OPUS_ENCODER_BITRATE));
  opus_encoder_ctl(opus_encoder, OPUS_SET_COMPLEXITY(OPUS_ENCODER_COMPLEXITY));
  opus_encoder_ctl(opus_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
  encoder_input_buffer = (opus_int16 *)malloc(BUFFER_SAMPLES*sizeof(opus_int16));
  encoder_output_buffer = (uint8_t *)malloc(OPUS_OUT_BUFFER_SIZE);
}

void oai_send_audio(PeerConnection *peer_connection) {
  size_t bytes_read = 0;
  if( esp_err_t err = i2s_channel_read(get_i2s_rx_handle(), encoder_input_buffer, BUFFER_SAMPLES*sizeof(opus_int16), &bytes_read,
           portMAX_DELAY) ; err != ESP_OK ) {
    ESP_LOGE(TAG, "Failed to read audio data from I2S: %s", esp_err_to_name(err));
  }
#ifdef CONFIG_IDF_TARGET_ESP32
  for(size_t i = 0; i < bytes_read/4; i++) {
    const auto value = reinterpret_cast<std::uint32_t*>(encoder_input_buffer)[i];
    const auto high_word = value >> 16;
    const auto low_word = value & 0xFFFF;
    reinterpret_cast<std::uint32_t*>(encoder_input_buffer)[i] = (low_word << 16) | high_word;
  }
#endif // CONFIG_IDF_TARGET_ESP32

#ifdef CONFIG_MEDIA_ENABLE_DEBUG_AUDIO_UDP_CLIENT
  sendto(s_debug_audio_sock, encoder_input_buffer, bytes_read, 0, (struct sockaddr *)&s_debug_audio_in_dest_addr, sizeof(s_debug_audio_in_dest_addr));
#endif // CONFIG_MEDIA_ENABLE_DEBUG_AUDIO_UDP_CLIENT

  auto encoded_size =
      opus_encode(opus_encoder, encoder_input_buffer, BUFFER_SAMPLES,
                  encoder_output_buffer, OPUS_OUT_BUFFER_SIZE);

  peer_connection_send_audio(peer_connection, encoder_output_buffer,
                             encoded_size);
}

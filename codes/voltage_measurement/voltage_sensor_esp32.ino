/*
 * Voltage Sensor Monitoring — ESP32 (WROOM-32 / DevKitC)
 * ---------------------------------------------------------
 * Sensor:  Generic resistive-divider DC voltage sensor module
 *          (e.g. 0-25V module, divider ratio 5:1 -> 0-5V module output)
 * MCU pin: GPIO35 (ADC1_CH7, input-only, ADC-capable on classic ESP32)
 *          Alt:   GPIO33 (ADC1_CH5) — change ADC_PIN below if used instead
 *
 * IMPORTANT — VERIFY BEFORE POWERING UP:
 *  - ESP32 ADC absolute max input = 3.3V (destructive beyond ~3.6V).
 *  - A 5V-output module driven at its full rated input WILL exceed 3.3V
 *    unless you add a secondary divider or confirm your module's actual
 *    output swing stays under 3.3V. Measure module output with a multimeter
 *    at expected max input voltage BEFORE connecting to GPIO35.
 *  - ESP32 ADC1 is non-linear near 0V and near full-scale (Espressif TRM);
 *    this sketch uses esp_adc_cal for a calibrated, more accurate result
 *    instead of a naive analogRead() -> linear scale conversion.
 *
 * Update SENSOR_DIVIDER_RATIO and MAX_INPUT_VOLTAGE to match your module's
 * datasheet — these values are NOT universal across vendors.
 */

#include <Arduino.h>
#include "esp_adc_cal.h"

// ---------------- User-configurable parameters ----------------
#define ADC_PIN               GPIO_NUM_35   // change to GPIO_NUM_33 if using D33
#define ADC_CHANNEL           ADC1_CHANNEL_7 // must match ADC_PIN (CH7 = GPIO35, CH5 = GPIO33)
#define ADC_ATTEN             ADC_ATTEN_DB_11 // full range, ~0-3.3V nominal (non-linear near rails)
#define ADC_WIDTH_BITS        ADC_WIDTH_BIT_12 // 12-bit, 0-4095
#define DEFAULT_VREF_MV       1100           // fallback reference if eFuse Vref not burned
#define SENSOR_DIVIDER_RATIO  5.0f           // module divider ratio (Vin_sensor / Vout_module)
                                              // VERIFY against your module's actual resistor values
#define OVERSAMPLE_COUNT      64             // averaging to reduce ADC quantization noise

// -----------------------------------------------------------------

static esp_adc_cal_characteristics_t adc_chars;

void setup() {
  Serial.begin(115200);
  delay(200);

  adc1_config_width(ADC_WIDTH_BITS);
  adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);

  esp_adc_cal_value_t cal_type = esp_adc_cal_characterize(
      ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH_BITS, DEFAULT_VREF_MV, &adc_chars);

  if (cal_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
    Serial.println("[ADC] Calibration source: eFuse Vref (best accuracy)");
  } else if (cal_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
    Serial.println("[ADC] Calibration source: eFuse Two Point (best accuracy)");
  } else {
    Serial.println("[ADC] Calibration source: Default Vref (least accurate, "
                    "consider burning eFuse or manual per-chip calibration)");
  }
}

float readModuleVoltageCalibrated() {
  uint32_t acc_mv = 0;
  for (int i = 0; i < OVERSAMPLE_COUNT; i++) {
    int raw = adc1_get_raw(ADC_CHANNEL);
    acc_mv += esp_adc_cal_raw_to_voltage(raw, &adc_chars); // returns mV, applies calibration curve
  }
  float avg_mv = (float)acc_mv / OVERSAMPLE_COUNT;
  return avg_mv / 1000.0f; // module output voltage, in volts, at the GPIO pin
}

void loop() {
  float v_module = readModuleVoltageCalibrated();   // voltage actually seen at GPIO35
  float v_sensed  = v_module * SENSOR_DIVIDER_RATIO; // scaled back to original measured voltage

  Serial.print("ADC pin voltage: ");
  Serial.print(v_module, 4);
  Serial.print(" V  |  Sensed input voltage: ");
  Serial.print(v_sensed, 3);
  Serial.println(" V");

  delay(500);
}

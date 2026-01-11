#include <Arduino.h>
#include <U8g2lib.h>        // U8g2 könyvtár az OLED kijelzőhöz
#include <Wire.h>           // I2C kommunikáció
#include "driver/pcnt.h"    // ESP32 belső impulzusszámláló driver

// --- OLED kijelző konfiguráció ---
// Az ESP32 alapértelmezett I2C lábai: SDA=21, SCL=22. 
// U8G2_R0: nincs elforgatás, F: teljes framebuffer (gyorsabb, de több RAM-ot használ)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// --- Mérési paraméterek ---
#define PCNT_INPUT_PIN 15   // A komparátor jele erre a lábra érkezik
#define PCNT_UNIT PCNT_UNIT_0

// --- Fizikai állandók ---
const double C_REF = 100e-9;           // 100 nF referencia kondenzátor
const double PI_SQR_4 = 4.0 * PI * PI; // Előre kiszámolt konstans (4 * PI^2)

void setup() {
  Serial.begin(115200);

  // U8g2 inicializálása
  u8g2.begin();
  u8g2.clearDisplay();

  // PCNT (impulzusszámláló) beállítása
  pcnt_config_t pcnt_config = {};              
  pcnt_config.pulse_gpio_num = PCNT_INPUT_PIN;
  pcnt_config.ctrl_gpio_num = -1;              
  pcnt_config.channel = PCNT_CHANNEL_0;
  pcnt_config.unit = PCNT_UNIT;
  pcnt_config.pos_mode = PCNT_COUNT_INC;       // Csak az emelkedő éleket számoljuk
  pcnt_config.neg_mode = PCNT_COUNT_DIS;
  pcnt_config.counter_h_lim = 32767;           // 16 bites hardveres limit
  pcnt_config.counter_l_lim = -32767;

  pcnt_unit_config(&pcnt_config);

  // Hardveres zajszűrő beállítása (100 órajel ciklusnál rövidebb tüskék eldobása)
  pcnt_set_filter_value(PCNT_UNIT, 100);
  pcnt_filter_enable(PCNT_UNIT);

  pcnt_counter_pause(PCNT_UNIT);
  pcnt_counter_clear(PCNT_UNIT);
}

void loop() {
  // --- Mérés indítása ---
  pcnt_counter_clear(PCNT_UNIT);
  pcnt_counter_resume(PCNT_UNIT);
  
  unsigned long startTime = millis();
  delay(200); // 200 ms kapuidő (Gate time)
  
  pcnt_counter_pause(PCNT_UNIT);
  unsigned long duration = millis() - startTime;

  int16_t pulses = 0;
  pcnt_get_counter_value(PCNT_UNIT, &pulses);

  // Frekvencia kiszámítása (Hz)
  double frequency = (double)pulses / (double)duration * 1000.0;

  // --- Kijelzés és Számítás ---
  u8g2.clearBuffer();					// Belső memória törlése

  // Frekvencia kiírása
  u8g2.setFont(u8g2_font_6x10_tf);      // Kisméretű betűtípus
  u8g2.drawStr(0, 12, "FREKVENCIA:");
  u8g2.setFont(u8g2_font_helvB14_tf);   // Nagyobb, félkövér betűtípus
  u8g2.setCursor(0, 30);
  u8g2.print((int)frequency);
  u8g2.print(" Hz");

  // Induktivitás számítása, ha van értelmezhető rezgés
  if (frequency > 50) {
    // Thomson-képlet átrendezve: L = 1 / (4 * PI^2 * f^2 * C)
    double inductance = 1.0 / (PI_SQR_4 * frequency * frequency * C_REF);
    
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 45, "INDUKTIVITAS:");
    u8g2.setFont(u8g2_font_helvB14_tf);
    u8g2.setCursor(0, 62);

    if (inductance >= 0.001) {
      u8g2.print(inductance * 1000.0, 2); // mH formátum
      u8g2.print(" mH");
    } else {
      u8g2.print(inductance * 1000000.0, 1); // uH formátum
      u8g2.print(" uH");
    }
  } else {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 55, "Nincs jel vagy ures L...");
  }

  u8g2.sendBuffer();					// Adatok küldése a kijelzőre
  delay(500); 
}

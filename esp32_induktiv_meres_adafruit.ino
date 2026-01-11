#include <Wire.h>               // I2C kommunikációhoz (OLED kijelző)
#include <Adafruit_GFX.h>       // Grafikus könyvtár a kijelzőhöz
#include <Adafruit_SSD1306.h>   // SSD1306 OLED driver
#include "driver/pcnt.h"        // ESP32 belső impulzusszámláló (Pulse Counter) driver

// --- Kijelző beállítások ---
#define SCREEN_WIDTH 128        // OLED szélessége pixelben
#define SCREEN_HEIGHT 64        // OLED magassága pixelben
#define OLED_RESET -1           // Reset láb (ha nincs, -1)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Mérési beállítások ---
#define PCNT_INPUT_PIN 15       // Ide érkezik az LC oszcillátor négyzethullám jele
#define PCNT_UNIT PCNT_UNIT_0   // Az ESP32 8 számláló egységéből a 0. használata

// --- Fizikai állandók ---
const double C_REF = 100e-9;    // Referencia kondenzátor értéke: 100 nF (Faradban kifejezve)
const double PI_SQR_4 = 4.0 * PI * PI; // Előre kiszámolt konstans a Thomson-képlethez

void setup() {
  Serial.begin(115200);

  // OLED inicializálása (0x3C az általános I2C cím)
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Kijelzo nem talalhato"));
    for(;;); // Hiba esetén megáll a program
  }
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(10, 20);
  display.println("L-Mero Inicializalas...");
  display.display();

  // --- PCNT (Impulzusszámláló) konfigurálása ---
  pcnt_config_t pcnt_config = {};              // Struktúra nullázása
  pcnt_config.pulse_gpio_num = PCNT_INPUT_PIN; // Bemeneti láb kijelölése
  pcnt_config.ctrl_gpio_num = -1;              // Nincs szükség irányítási lábra
  pcnt_config.channel = PCNT_CHANNEL_0;        // 0. csatorna használata
  pcnt_config.unit = PCNT_UNIT;                // 0. egység használata
  pcnt_config.pos_mode = PCNT_COUNT_INC;       // Számlálás emelkedő élnél (Low -> High)
  pcnt_config.neg_mode = PCNT_COUNT_DIS;       // Eső élnél ne csináljon semmit
  pcnt_config.counter_h_lim = 32767;           // 16-bites hardveres maximum limit
  pcnt_config.counter_l_lim = -32767;          // Alsó limit

  pcnt_unit_config(&pcnt_config);              // Beállítások alkalmazása

  // Szűrő beállítása: kiszűri a túl rövid tüskéket (zajvédelem)
  // Az érték az APB órajel ciklusait jelenti (100 = kb. 1.25 mikroszekundumnál rövidebb zaj kizárva)
  pcnt_set_filter_value(PCNT_UNIT, 100);
  pcnt_filter_enable(PCNT_UNIT);

  pcnt_counter_pause(PCNT_UNIT);               // Számláló megállítása az indításig
  pcnt_counter_clear(PCNT_UNIT);               // Nullázás
}

void loop() {
  // --- Mérés folyamata ---
  pcnt_counter_clear(PCNT_UNIT);               // Számláló törlése
  pcnt_counter_resume(PCNT_UNIT);              // Számlálás indítása
  
  unsigned long startTime = millis();          // Időbélyeg mentése
  delay(200);                                  // 200 ms várakozás (kapuidő)
  
  pcnt_counter_pause(PCNT_UNIT);               // Számlálás leállítása
  unsigned long duration = millis() - startTime; // Ténylegesen eltelt idő kiszámítása

  int16_t pulses = 0;
  pcnt_get_counter_value(PCNT_UNIT, &pulses);  // Kiolvassuk, hány él érkezett

  // Frekvencia számítása: (impulzusok száma / eltelt idő másodpercben)
  // A duration ms-ban van, ezért osztjuk 1000-rel (vagy szorozzuk a számlálót 1000-rel)
  double frequency = (double)pulses / (double)duration * 1000.0;

  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("MERT FREKVENCIA:");
  display.setTextSize(2);
  display.print((int)frequency); 
  display.println(" Hz");

  // --- Induktivitás kiszámítása ---
  // Képlet: L = 1 / (4 * PI^2 * f^2 * C)
  if (frequency > 50) { // Csak akkor számolunk, ha van értékelhető jel (50 Hz felett)
    double inductance = 1.0 / (PI_SQR_4 * frequency * frequency * C_REF);
    
    display.setTextSize(1);
    display.println("\nINDUKTIVITAS:");
    display.setTextSize(2);

    // Mértékegység automatikus választása
    if (inductance >= 0.001) {
      // Millihenry (mH) kijelzése
      display.print(inductance * 1000.0, 2);
      display.println(" mH");
    } else {
      // Mikrohenry (uH) kijelzése
      display.print(inductance * 1000000.0, 1);
      display.println(" uH");
    }
  } else {
    // Ha nincs jel vagy túl alacsony a frekvencia
    display.setTextSize(1);
    display.println("\n[ Nincs jel ]");
    display.setCursor(0, 55);
    display.print("Ellenorizze az LC kort!");
  }

  display.display(); // Kép frissítése az OLED-en
  delay(800);        // Rövid várakozás a következő frissítés előtt
}

// =============================================
// SISTEM MONITORING LEVEL AIR TAMBAK BANDENG
// Sensor : Ultrasonik HC-SR04 + Rain Sensor
// Aktuator: Pompa Air (via Relay aktif LOW)
// Board   : ESP32
// =============================================

#define TRIG_PIN   2
#define ECHO_PIN   15
#define RAIN_AO    35
#define RAIN_DO    23
#define PUMP_PIN   22

// --- Kalibrasi Jarak Ultrasonik (cm) ---
const float JARAK_AMAN_SENSOR = 0.5;  // ← Dead zone, air tidak boleh menyentuh sensor
const float LEVEL_PENUH       = 3.5;  // cm dari sensor → pompa NYALA
const float LEVEL_NORMAL      = 5.0;  // cm dari sensor → pompa MATI
const float LEVEL_MAKS_SENSOR = 30.0; // batas wajar pembacaan sensor

// LEVEL_PENUH harus selalu > JARAK_AMAN_SENSOR
// Visualisasi jarak (diukur dari sensor ke bawah):
//
//  [== SENSOR ULTRASONIK ==]   ← 0 cm
//  |   0.5 cm                  ← JARAK_AMAN_SENSOR (dead zone)
//  |   3.5 cm                  ← LEVEL_PENUH  → pompa ON
//  |   5.0 cm                  ← LEVEL_NORMAL → pompa OFF
//  |   ...
//  [~~~~~~~~ PERMUKAAN AIR ~~~~~~~~]

// --- Kalibrasi Rain Sensor Analog ---
const int BATAS_HUJAN_LEBAT  = 1000;
const int BATAS_HUJAN_RINGAN = 2500;

// --- State ---
bool pumpState  = false;

typedef enum {
  CUACA_KERING  = 0,
  HUJAN_RINGAN  = 1,
  HUJAN_LEBAT   = 2
} StatusCuaca;

StatusCuaca statusCuaca = CUACA_KERING;

// =============================================
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RAIN_DO,  INPUT);
  pinMode(PUMP_PIN, OUTPUT);

  digitalWrite(PUMP_PIN, HIGH); // Relay OFF saat startup
  pumpState = false;

  Serial.println("======================================");
  Serial.println("  SISTEM MONITORING TAMBAK BANDENG   ");
  Serial.println("======================================");
}

// =============================================
float bacaJarak() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration == 0) return -1.0;

  float jarak = duration * 0.0343 / 2.0;

  if (jarak > LEVEL_MAKS_SENSOR) return -1.0;

  // ← DEAD ZONE: jika air terlalu dekat ke sensor, kembalikan -2
  //   sebagai kode khusus "BAHAYA - air menyentuh sensor"
  if (jarak < JARAK_AMAN_SENSOR) return -2.0;

  return jarak;
}

// =============================================
StatusCuaca bacaStatusCuaca(int analogVal, int digitalVal) {
  if (digitalVal == LOW) {
    if (analogVal < BATAS_HUJAN_LEBAT) {
      return HUJAN_LEBAT;
    } else {
      return HUJAN_RINGAN;
    }
  } else {
    if (analogVal < BATAS_HUJAN_RINGAN) {
      return HUJAN_RINGAN;
    }
    return CUACA_KERING;
  }
}

// =============================================
void kontrolPompa(float distance, StatusCuaca cuaca) {

  // Sensor error biasa → pompa mati (safe default)
  if (distance == -1.0) {
    pumpState = false;
    return;
  }

  // ← KONDISI KRITIS: air menyentuh area dead zone sensor
  //   Paksa pompa ON darurat untuk selamatkan sensor
  if (distance == -2.0) {
    pumpState = true;
    return;
  }

  // Nyalakan pompa: air penuh
  if (distance <= LEVEL_PENUH) {
    pumpState = true;
  }

  // Nyalakan pompa lebih awal saat hujan lebat
  if (cuaca == HUJAN_LEBAT && distance <= (LEVEL_PENUH + 1.0)) {
    pumpState = true;
  }

  // Matikan pompa: air sudah turun ke level normal
  if (distance >= LEVEL_NORMAL) {
    pumpState = false;
  }
}

// =============================================
void printStatus(float distance, int rainAO, int rainDO, StatusCuaca cuaca) {
  Serial.println("================================");

  Serial.print("Jarak Air    : ");
  if (distance == -1.0) {
    Serial.println("ERROR (sensor gagal baca)");
  } else if (distance == -2.0) {
    Serial.println("⛔ KRITIS! Air terlalu dekat ke sensor!");
  } else {
    Serial.print(distance);
    Serial.print(" cm  →  Level: ");
    if (distance <= LEVEL_PENUH) {
      Serial.println("PENUH ⚠");
    } else if (distance <= LEVEL_NORMAL) {
      Serial.println("SEDANG");
    } else {
      Serial.println("RENDAH");
    }
  }

  Serial.print("Rain Analog  : ");
  Serial.println(rainAO);

  Serial.print("Rain Digital : ");
  Serial.println(rainDO == LOW ? "LOW (Hujan Terdeteksi)" : "HIGH (Kering)");

  Serial.print("Status Cuaca : ");
  switch (cuaca) {
    case CUACA_KERING:  Serial.println("KERING");       break;
    case HUJAN_RINGAN:  Serial.println("HUJAN RINGAN"); break;
    case HUJAN_LEBAT:   Serial.println("HUJAN LEBAT");  break;
  }

  Serial.print("Pompa        : ");
  Serial.println(pumpState ? "ON  (Memompa keluar)" : "OFF");

  Serial.println("================================");
  Serial.println();
}

// =============================================
void loop() {
  float distance = bacaJarak();
  int   rainAO   = analogRead(RAIN_AO);
  int   rainDO   = digitalRead(RAIN_DO);

  statusCuaca = bacaStatusCuaca(rainAO, rainDO);

  kontrolPompa(distance, statusCuaca);

  digitalWrite(PUMP_PIN, pumpState ? LOW : HIGH);

  printStatus(distance, rainAO, rainDO, statusCuaca);

  delay(1000);
}
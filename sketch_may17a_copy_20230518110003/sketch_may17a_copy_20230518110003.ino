
/*************************************************************************************************
Pascal-Emmanuel Lachance, lacp3102
Philippe Gauthier, gaup1302
*************************************************************************************************/
/*************************************************************************************************/
/* File includes ------------------------------------------------------------------------------- */
#include <math.h>
#include <Wire.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <string>

/*************************************************************************************************/
/* Type definitions ---------------------------------------------------------------------------- */
typedef struct __attribute__((__packed__))
{
  float humidity;
  float pressure;
  float temperature;
  float windSpeed;
  float light;
  float waterLevel;
  const char* windDirection;
} sensors_t;

#define SERVICE_UUID           "6751b732-f992-11ed-be56-0242ac120002" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6751b733-f992-11ed-be56-0242ac120002"
#define CHARACTERISTIC_UUID_TX "6751b734-f992-11ed-be56-0242ac120002"

/*************************************************************************************************/
/* Function declarations ----------------------------------------------------------------------- */
static inline int32_t complement_two(int32_t value, int8_t bits);
static inline void    display();
static void pressure_temperature_setup();
static void pressure();
static void light();
static void humidity();
static void wind_speed();
static void direction();
static void water_level();


/*************************************************************************************************/
/* Global Variables ---------------------------------------------------------------------------- */
int16_t c0;
int16_t c1;
int32_t c00;
int32_t c10;
int16_t c01;
int16_t c11;
int16_t c20;
int16_t c21;
int16_t c30;

static bool newDataFlag = false;
static char displayTable[512] = {'\0'};
static std::string displayString;
BLEServer *pServer = NULL;
BLECharacteristic * pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;

sensors_t sensors = {0};


/*************************************************************************************************/
/* Function definitions ------------------------------------------------------------------------ */

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        Serial.println("*********");
        Serial.print("Received Value: ");
        for (int i = 0; i < rxValue.length(); i++)
          Serial.print(rxValue[i]);

        Serial.println();
        Serial.println("*********");
      }
    }
};

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Bon matin!");
  
  // Create the BLE Device
  BLEDevice::init("La chip a fil");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(
                    CHARACTERISTIC_UUID_TX,
                    BLECharacteristic::PROPERTY_NOTIFY
                  );
                      
  pTxCharacteristic->addDescriptor(new BLE2902());
  
  BLECharacteristic * pRxCharacteristic = pService->createCharacteristic(
                       CHARACTERISTIC_UUID_RX,
                      BLECharacteristic::PROPERTY_WRITE
                    );  

  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->addServiceUUID(pService->getUUID());
  pServer->getAdvertising()->start();
  
  Serial.println("Waiting a client connection to notify...");
  pressure_temperature_setup();
}

void loop()
{
  static long timer_2hz   = millis();
  static long timer_1hz   = millis();
  static long timer_0_1hz = millis();

  if (millis() - timer_2hz > 500)
  {
    timer_2hz = millis();
    pressure();
    light();
    direction();
    display();
  }
  
  if (millis() - timer_1hz > 1000)
  {
    timer_1hz = millis();
  }

  if (millis() - timer_0_1hz > 10000)
  {
    timer_0_1hz = millis();
    humidity();
  }

  wind_speed();
  water_level();

  
  if (deviceConnected and newDataFlag) {
        pTxCharacteristic->setValue(displayString);
        pTxCharacteristic->notify();
        newDataFlag = false;
  delay(10); // bluetooth stack will go into congestion, if too many packets are sent
  }

    // disconnecting
  if (!deviceConnected && oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("start advertising");
        oldDeviceConnected = deviceConnected;
    }
    // connecting
  if (deviceConnected && !oldDeviceConnected) {
    // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }
}

static void direction()
{
  static const char* directions[]     = {"N",   "NNE", "NE",  "NEE", "E",   "SEE", "SE",  "SSE", "S",   "SSW", "SW",  "SWW", "W",   "NWW", "NW",  "NNW"};
  static const float directionVals[]  = {0.81f, 0.67f, 1.94f, 1.84f, 3.18f, 2.62f, 2.87f, 2.18f, 2.47f, 1.20f, 1.39f, 0.13f, 0.17f, 0.08f, 0.47f, 0.28f};
  static const float epsilon = 0.02;

  float val = analogRead(35) / 4096.0 * 3.3f;

  for (int i = 0; i < sizeof(directions) / sizeof(char*); i++)
  {
    if (val >= directionVals[i] - epsilon && val <= directionVals[i] + epsilon)
    {
      //Serial.println(directions[i]);
      if (directions[i] != sensors.windDirection)
        newDataFlag = true;
      sensors.windDirection = directions[i];
      return;
    }
  }
  Serial.println("Unknown direction");
}

static void water_level()
{
  static long timer = millis();
  if (millis() - timer > 250)
  {
    int val = digitalRead(23);
    if (val == 0)
    {
      newDataFlag = true;
      sensors.waterLevel += 0.2794f;
      timer = millis();
    }
  }
}

static void pressure()
{
  uint8_t data[6];
  int i = 0;

  Wire.beginTransmission(0x77);
  Wire.write(byte(0x00));
  Wire.endTransmission();

  Wire.requestFrom(0x77, sizeof(data));
  while(Wire.available())
  {
    data[i++] = Wire.read();
    if (i >= sizeof(data))
      break;
  }

  int32_t pressure    = ((uint32_t)(data[0]) << 16) + ((uint32_t)(data[1]) << 8) + data[2];
  int32_t temperature = ((uint32_t)(data[3]) << 16) + ((uint32_t)(data[4]) << 8) + data[5];

  pressure = complement_two(pressure, 24);

  float pRaw = pressure / 524288.f;
  float tRaw = temperature / 524288.f;
  float temp = (float)c0 * 0.5f + (float)c1 * tRaw;
  float pres = (float)c00 + pRaw * ((float)c10 + pRaw * ((float)c20 + pRaw * (float)c30)) + tRaw * (float)c01 + tRaw * pRaw * ((float)c1 + pRaw * (float)c21);
  if(temp != sensors.temperature or  pres != sensors.pressure)
    newDataFlag = true;
  
  sensors.pressure = pres;
  sensors.temperature = temp;

  //Serial.printf("Pression = %f  Temperature = %f\n", fPressure, fTemperature);
}

static void humidity()
{
  const static int broche = 16;

  pinMode(broche, OUTPUT_OPEN_DRAIN);
  digitalWrite(broche, HIGH);
  delay(250);
  digitalWrite(broche, LOW);
  delay(20);
  digitalWrite(broche, HIGH);
  delayMicroseconds(40);
  pinMode(broche, INPUT_PULLUP);

  while (digitalRead(broche) == HIGH)
    ;

  int count = 0;
  int duree[42];
  unsigned long pulse = 0;
  do {
    pulse = pulseIn(broche, HIGH);
    duree[count++] = pulse;
  } while (pulse != 0);

  if (count != 42)
    Serial.printf("Erreur timing\n");


  byte data[5];
  for (int i = 0; i < 5; i++) {
    data[i] = 0;
    for (int j = ((8 * i) + 1); j < ((8 * i) + 9); j++) {
      data[i] = data[i] * 2;
      if (duree[j] > 50) {
        data[i] = data[i] + 1;
      }
    }
  }

  if ((data[0] + data[1] + data[2] + data[3]) != data[4]) {
    Serial.println(" Erreur checksum");
  }
  float humidity = data[0] + (data[1] / 256.0);
  if(humidity != sensors.humidity)
    newDataFlag = true;
  sensors.humidity = humidity;
}

static void light()
{
  float light = analogRead(A6);
  if(light != sensors.light)
    newDataFlag = true;
  sensors.light = light;
}

static void wind_speed()
{
  static bool oldClick = false;
  static long start = millis();
  static int clicks = 0;

  float val = analogRead(27);
  if (oldClick)
  {
    if (val == 0)
    {
      clicks++;
      oldClick = false;
    }
  }
  else
  {
    if (val > 0)
    {
      oldClick = true;
    }
  }
  float windSpeed = clicks * 1000.0 / (millis() - start);
  if(windSpeed != sensors.windSpeed)
    newDataFlag = true;
  sensors.windSpeed = windSpeed;
  
  if (millis() - start > 10000)
  {
    start = millis();
    clicks = 0;
  }
}


static inline void display()
{
  sprintf(displayTable,"\n==============================\n"
                "Pression = %5.2fkPa\n"
                "Temperature = %4.2f degreC\n"
                "Vitesse = %2.2f\n"
                "Humidite = %2.1f %%\n"
                "Lumiere = %4.0f\n"
                "Direction = %s\n"
                "Niveau d'eau = %2.4f mm\n",
                sensors.pressure / 1000.f, sensors.temperature, sensors.windSpeed, sensors.humidity, sensors.light, sensors.windDirection, sensors.waterLevel);
                displayString = displayTable;
}


static void pressure_temperature_setup()
{
  Wire.begin();

  Wire.beginTransmission(0x77);
  Wire.write(byte(0x06));
  Wire.write(byte(0b00100000));
  Wire.endTransmission();

  Wire.beginTransmission(0x77);
  Wire.write(byte(0x07));
  Wire.write(byte(0b10100000));
  Wire.endTransmission();

  Wire.beginTransmission(0x77);
  Wire.write(byte(0x08));
  Wire.write(byte(0b00000111));
  Wire.endTransmission();

  // Read coefficients
  uint8_t coefficients[18];
  int i = 0;
  Wire.beginTransmission(0x77);
  Wire.write(byte(0x10));
  Wire.endTransmission();

  Wire.requestFrom(0x77, sizeof(coefficients));
  while(Wire.available())
  {
    coefficients[i++] = Wire.read();
    if (i >= sizeof(coefficients))
      break;
  }

  c0  = (coefficients[0] << 4) + ((coefficients[1] >> 4) & 0x0F);
  c1  = ((coefficients[1] & 0x0F) << 8) + coefficients[2];
  c00 = (coefficients[3] << 12) + (coefficients[4] << 4) + (coefficients[5] >> 4);
  c10 = ((coefficients[5] & 0x0F) << 16) + (coefficients[6] << 8) + coefficients[7];
  c01 = (coefficients[8] << 8) + coefficients[9];
  c11 = (coefficients[10] << 8) + coefficients[11];
  c20 = (coefficients[12] << 8) + coefficients[13];
  c21 = (coefficients[14] << 8) + coefficients[15];
  c30 = (coefficients[16] << 8) + coefficients[17];
  

  c0  = complement_two(c0,  12);
  c1  = complement_two(c1,  12);
  c00 = complement_two(c00, 20);
  c10 = complement_two(c10, 20);
  c01 = complement_two(c01, 16);
  c11 = complement_two(c11, 16);
  c20 = complement_two(c20, 16);
  c21 = complement_two(c21, 16);
  c30 = complement_two(c30, 16);

  Serial.println(c0);
  Serial.println(c1);
  Serial.println(c00);
  Serial.println(c10);
  Serial.println(c01);
  Serial.println(c11);
  Serial.println(c20);
  Serial.println(c21);
  Serial.println(c30);
}

static inline int32_t complement_two(int32_t value, int8_t bits)
{
  if (value > pow(2, bits - 1) - 1)
  {
    return value - pow(2, bits);
  }
  return value;
}

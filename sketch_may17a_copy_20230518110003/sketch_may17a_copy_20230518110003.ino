#include <math.h>
#include <Wire.h>

int16_t c0;
int16_t c1;
int32_t c00;
int32_t c10;
int16_t c01;
int16_t c11;
int16_t c20;
int16_t c21;
int16_t c30;

typedef enum
{
  NOT_READY,
  SETUP,
  READY,
} humidityStep_t;
humidityStep_t humidityStep;

typedef struct
{
  float humidity;
  float pressure;
  float temperature;
  float windSpeed;
  float light;
} sensors_t;
sensors_t sensors = {0};


int32_t complement_two(int32_t value, int8_t bits)
{
  if (value > pow(2, bits - 1) - 1)
  {
    return value - pow(2, bits);
  }
  return value;
}

void pressure_temperature_setup()
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

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Bon matin!");

  pressure_temperature_setup();
}

void pressure()
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
  sensors.pressure = (float)c00 + pRaw * ((float)c10 + pRaw * ((float)c20 + pRaw * (float)c30)) + tRaw * (float)c01 + tRaw * pRaw * ((float)c1 + pRaw * (float)c21);
  sensors.temperature = (float)c0 * 0.5f + (float)c1 * tRaw;

  //Serial.printf("Pression = %f  Temperature = %f\n", fPressure, fTemperature);
}




void start_humidity()
{
  const static int broche = 16;
  static int step = 0;
  static long start = millis();

  if (humidityStep == NOT_READY)
    return;

  if (step == 0)
  {
    pinMode(broche, OUTPUT_OPEN_DRAIN);
    digitalWrite(broche, HIGH);
    step = 1;
    start = millis();
  }
  else if (step == 1)
  {
    if (millis() - start > 250)
    {
      digitalWrite(broche, LOW);
      step = 2;
      start = millis();
    }
  }
  else if (step == 2)
  {
    if (millis() - start > 20)
    {
      digitalWrite(broche, HIGH);
      delayMicroseconds(40);
      pinMode(broche, INPUT_PULLUP);

      humidityStep = READY;
      step = 0;
    }
  }
  else
    step = 0;
}

bool check_humidity()
{
  const static int broche = 16;
  if (digitalRead(broche) == HIGH)
    return false;
  else
    return true;
}


void humidity()
{
  const static int broche = 16;
  static int count = 0;
  static int duree[42];

  static long startTime = millis();

  if (humidityStep != READY)
    return;

  if (!check_humidity())
    return;
  
  /*
  unsigned long pulse = 0;
  do {
    pulse = pulseIn(broche, HIGH);
    duree[count++] = pulse;
  } while (pulse != 0);
  */

  unsigned long pulse = pulseIn(broche, HIGH);
  duree[count++] = pulse;
  if (pulse != 0)
    return;

  if (count != 42)
    Serial.printf("Erreur timing %d \n", count);
  else
    Serial.println(millis() - startTime);
    startTime = millis();

  count = 0;

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

  sensors.humidity = data[0] + (data[1] / 256.0);
  //temperature = data[2] + (data[3] / 256.0);

  humidityStep = NOT_READY;
}

void humidity_base()
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

  sensors.humidity = data[0] + (data[1] / 256.0);
}

void light()
{
  sensor.light = analogRead(A6);
}

void windSpeed()
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

  sensors.windSpeed = clicks * 1000.0 / (millis() - start);

  if (millis() - start > 10000)
  {
    start = millis();
    clicks = 0;
  }
}

void display()
{
  Serial.printf("\n==============================\n"
                "Pression = %5.2fkPa\n"
                "Temperature = %4.2f degreC\n"
                "Vitesse = %2.2f\n"
                "Humidite = %4.0f %%\n",
                sensors.pressure / 1000.f, sensors.temperature, sensors.windSpeed, sensors.humidity);
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
  }
  
  if (millis() - timer_1hz > 1000)
  {
    timer_1hz = millis();
    display();
  }

  if (millis() - timer_0_1hz > 10000)
  {
    timer_0_1hz = millis();
    humidity_base();
  }

  //start_humidity();
  //humidity();
  windSpeed();
}

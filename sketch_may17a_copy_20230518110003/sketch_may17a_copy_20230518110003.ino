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

int32_t complement_two(int32_t value, int8_t bits)
{
  if (value > pow(2, bits - 1) - 1)
  {
    return value - pow(2, bits);
  }
  return value;
}

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Bon matin!");


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
  float fPressure = (float)c00 + pRaw * ((float)c10 + pRaw * ((float)c20 + pRaw * (float)c30)) + tRaw * (float)c01 + tRaw * pRaw * ((float)c1 + pRaw * (float)c21);
  float fTemperature = (float)c0 * 0.5f + (float)c1 * tRaw;

  Serial.printf("Pression = %f  Temperature = %f\n",
                fPressure, fTemperature);
}






void humidity()
{
  int i, j;
  int duree[42];
  unsigned long pulse;
  byte data[5];
  float humidite;
  float temperature;
  int broche = 16;

  delay(1000);

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
  i = 0;

  do {
    pulse = pulseIn(broche, HIGH);
    duree[i] = pulse;
    i++;
  } while (pulse != 0);

  if (i != 42)
    Serial.printf(" Erreur timing \n");

  for (i = 0; i < 5; i++) {
    data[i] = 0;
    for (j = ((8 * i) + 1); j < ((8 * i) + 9); j++) {
      data[i] = data[i] * 2;
      if (duree[j] > 50) {
        data[i] = data[i] + 1;
      }
    }
  }

  if ((data[0] + data[1] + data[2] + data[3]) != data[4]) {
    Serial.println(" Erreur checksum");
  }

  humidite = data[0] + (data[1] / 256.0);
  temperature = data[2] + (data[3] / 256.0);

  Serial.printf(" Humidite = %4.0f \%%  Temperature = %4.2f degreC \n",
                humidite, temperature);
}

void light()
{
  int lightValue = analogRead(A6);
   Serial.println(lightValue);

   delay(500);
}


void loop()
{
  pressure();
  delay(500);
}
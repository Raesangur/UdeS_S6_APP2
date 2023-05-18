#include <Wire.h>

int16_t c0;
int16_t c1;

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

  c0 = (coefficients[0] << 4) + ((coefficients[1] >> 4) & 0x0F);
  c1 = ((coefficients[1] & 0x0F) << 8) + coefficients[2];
  

  if (c0 > (2048 - 1))
  {
    c0 -= 4096;
  }
  if (c1 > (2048 - 1))
  {
    c1 -= 4096;
  }

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

  float fPressure = pressure / 524288.f;
  float fTemperature = (float)c0 * 0.5f + (float)c1 * temperature / 524288.f;

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
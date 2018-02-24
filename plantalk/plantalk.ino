#include <WioLTEforArduino.h>
#include <Wire.h>
#include "SI114X.h"

#include <stdio.h>

#define APN               "soracom.io"
#define USERNAME          "sora"
#define PASSWORD          "sora"

#define INTERVAL          (5000)
#define RECEIVE_TIMEOUT   (10000)

WioLTE Wio;
SI114X SI1145 = SI114X();
HardwareSerial* gas_sensor;

const unsigned char cmd_get_sensor[] =
{
  0xff, 0x01, 0x86, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x79
};

// gas sensor raw data
unsigned char dataRevice[9];

// sensor pin
#define SENSOR_TEMP_HUMIDITY_PIN    (WIOLTE_D38)
#define SENSOR_MOISTURE_PIN         (WIOLTE_A6)
#define SENSOR_ILLUMI_PIN           (WIOLTE_A4)

// sensor value
int value_moisture;
float value_temp;
float value_humi;
int value_sunlight;
int value_illumi;
int value_co2;
int value_temperature;

void setup() {
  delay(200);

  SerialUSB.println("");
  SerialUSB.println("--- LTE Module START ---------------------------------------------------");

  SerialUSB.println("### I/O Initialize.");
  Wio.Init();
  pinMode(SENSOR_MOISTURE_PIN, INPUT_ANALOG);
  pinMode(SENSOR_ILLUMI_PIN, INPUT_ANALOG);
  
  SerialUSB.println("### Power supply ON.");
  Wio.PowerSupplyLTE(true);
  delay(500);

  SerialUSB.println("### Grove supply ON.");
  Wio.PowerSupplyGrove(true);
  delay(500);

  SerialUSB.println("### Turn on or reset.");
  if (!Wio.TurnOnOrReset()) {
    SerialUSB.println("### ERROR! ###");
    return;
  }

  SerialUSB.println("### Connecting to \""APN"\".");
  if (!Wio.Activate(APN, USERNAME, PASSWORD)) {
    SerialUSB.println("### ERROR! ###");
    return;
  }
  SerialUSB.println("### Setup completed.");

  SerialUSB.println("");
  SerialUSB.println("--- ST1145 START--------------------------------------------------------");
  while (!SI1145.Begin()) {
    SerialUSB.println("Si1145 is not ready!");
    delay(1000);
  }

  SerialUSB.println("");
  SerialUSB.println("--- Temp&Humidity START-------------------------------------------------");
  TemperatureAndHumidityBegin(SENSOR_TEMP_HUMIDITY_PIN);

  //SerialUSB.println("");
  //SerialUSB.println("--- GAS START-----------------------------------------------------------");
  //gas_sensor = &Serial;
  //gas_sensor->begin(9600);

  SerialUSB.println("");
  SerialUSB.println("--- Initial Complete----------------------------------------------------");

  value_moisture = 0;
  value_temp = 0.0f;
  value_humi = 0.0f;
  value_sunlight = 0;
  value_illumi = 0;
  value_co2 = 0;
  value_temperature = 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// get sensor value
void get_moisture_sensor() {
  SerialUSB.print("// get_moisture_sensor-------------------------------//\r\n");
  value_moisture = analogRead(SENSOR_MOISTURE_PIN);
  SerialUSB.print("sensor = " );
  SerialUSB.println(value_moisture);
}

void get_sunlight_sensor() {
  SerialUSB.print("// get_sunlight_sensor-------------------------------//\r\n");
  SerialUSB.print("Vis: "); SerialUSB.println(SI1145.ReadVisible());
  SerialUSB.print("IR: "); SerialUSB.println(SI1145.ReadIR());
  //the real UV value must be div 100 from the reg value , datasheet for more information.
  SerialUSB.print("UV: ");  SerialUSB.println((float)SI1145.ReadUV() / 100);
}

void get_temp_humidity_sensor() {
  SerialUSB.print("// get_temp_humidity_sensor---------------------------//\r\n");
  if (!TemperatureAndHumidityRead(&value_temp, &value_humi)) {
    SerialUSB.println("ERROR!");
  } else {
    SerialUSB.print("Current humidity = ");
    SerialUSB.print(value_humi);
    SerialUSB.print("%  ");
    SerialUSB.print("temperature = ");
    SerialUSB.print(value_temp);
    SerialUSB.println("C");
  }
}

void get_gas_sensor() {
  SerialUSB.print("// get_gas_sensor-------------------------------------//\r\n");  
  if (dataRecieve()) {
    SerialUSB.print("Temperature = ");
    SerialUSB.print(value_temperature);
    SerialUSB.print("  CO2 =  ");
    SerialUSB.print(value_co2);
    SerialUSB.println("");
  }
}

void get_illumi_sensor() {
  SerialUSB.print("// get_illumi_sensor----------------------------------//\r\n");  
  SerialUSB.print("illumi = ");
  value_illumi = analogRead(SENSOR_ILLUMI_PIN);
  SerialUSB.println(value_illumi);  
}
////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////
// main function
void loop() {
  get_moisture_sensor();
  get_sunlight_sensor();
  get_temp_humidity_sensor();
  get_illumi_sensor();
  //get_gas_sensor();

  communicate_soracom();
  delay(INTERVAL);
}

////////////////////////////////////////////////////////////////////////////////////////
// soracom
void communicate_soracom() {
  char data[200];
  
  SerialUSB.println("### Open.");
  int connectId;
  connectId = Wio.SocketOpen("funnel.soracom.io", 23080, WIOLTE_UDP);
  if (connectId < 0) {
    SerialUSB.println("### ERROR! ###");
    goto err;
  }

  SerialUSB.println("### Send.");
  //sprintf(data, "{\"app\": 2,\"record\": {\"humidity\":{\"value\":%lu}}}", millis() / 1000);
  sprintf(data, "{\"app\": 3,\"record\": {\"plant_id\":{\"value\":10},\"temp\":{\"value\":%ld},\"illumi\":{\"value\":%lu},\"humi\":{\"value\":%ld},\"soil_hum\":{\"value\":%lu},\"co2\":{\"value\":%lu}}}", (int)value_temp, value_illumi, (int)value_humi, value_moisture, value_co2);
  
  SerialUSB.print("Send:");
  SerialUSB.print(data);
  SerialUSB.println("");
  if (!Wio.SocketSend(connectId, data)) {
    SerialUSB.println("### ERROR! ###");
    goto err_close;
  }
  
  SerialUSB.println("### Receive.");
  int length;
  length = Wio.SocketReceive(connectId, data, sizeof (data), RECEIVE_TIMEOUT);
  if (length < 0) {
    SerialUSB.println("### ERROR! ###");
    goto err_close;
  }
  if (length == 0) {
    SerialUSB.println("### RECEIVE TIMEOUT! ###");
    goto err_close;
  }
  SerialUSB.print("Receive:");
  SerialUSB.print(data);
  SerialUSB.println("");

err_close:
  SerialUSB.println("### Close.");
  if (!Wio.SocketClose(connectId)) {
    SerialUSB.println("### ERROR! ###");
    goto err;
  }

err:
  delay(INTERVAL);
}

////////////////////////////////////////////////////////////////////////////////////////
// Temp&Humidity
int TemperatureAndHumidityPin;

void TemperatureAndHumidityBegin(int pin)
{
  TemperatureAndHumidityPin = pin;
  DHT11Init(TemperatureAndHumidityPin);
}

bool TemperatureAndHumidityRead(float* temperature, float* humidity)
{
  byte data[5];

  DHT11Start(TemperatureAndHumidityPin);
  for (int i = 0; i < 5; i++) data[i] = DHT11ReadByte(TemperatureAndHumidityPin);
  DHT11Finish(TemperatureAndHumidityPin);

  if (!DHT11Check(data, sizeof (data))) return false;
  if (data[1] >= 10) return false;
  if (data[3] >= 10) return false;

  *humidity = (float)data[0] + (float)data[1] / 10.0f;
  *temperature = (float)data[2] + (float)data[3] / 10.0f;

  return true;
}

void DHT11Init(int pin)
{
  digitalWrite(pin, HIGH);
  pinMode(pin, OUTPUT);
}

void DHT11Start(int pin)
{
  // Host the start of signal
  digitalWrite(pin, LOW);
  delay(18);

  // Pulled up to wait for
  pinMode(pin, INPUT);
  while (!digitalRead(pin)) ;

  // Response signal
  while (digitalRead(pin)) ;

  // Pulled ready to output
  while (!digitalRead(pin)) ;
}

byte DHT11ReadByte(int pin)
{
  byte data = 0;

  for (int i = 0; i < 8; i++) {
    while (digitalRead(pin)) ;

    while (!digitalRead(pin)) ;
    unsigned long start = micros();

    while (digitalRead(pin)) ;
    unsigned long finish = micros();

    if ((unsigned long)(finish - start) > 50) data |= 1 << (7 - i);
  }

  return data;
}

void DHT11Finish(int pin)
{
  // Releases the bus
  while (!digitalRead(pin)) ;
  digitalWrite(pin, HIGH);
  pinMode(pin, OUTPUT);
}

bool DHT11Check(const byte* data, int dataSize)
{
  if (dataSize != 5) return false;

  byte sum = 0;
  for (int i = 0; i < dataSize - 1; i++) {
    sum += data[i];
  }

  return data[dataSize - 1] == sum;
}

////////////////////////////////////////////////////////////////////////////////////////
// CO2
bool dataRecieve(void)
{
  byte data[9];
  int i = 0;

  //transmit command data
  for (i = 0; i < sizeof(cmd_get_sensor); i++)
  {
    gas_sensor->write(cmd_get_sensor[i]);
  }
  delay(10);
  //begin reveiceing data
  if (gas_sensor->available())
  {
    while (gas_sensor->available())
    {
      for (int i = 0; i < 9; i++)
      {
        data[i] = gas_sensor->read();
      }
    }
  }

  for (int j = 0; j < 9; j++)
  {
    SerialUSB.print(data[j]);
    SerialUSB.print(" ");
  }
  SerialUSB.println("");

  if ((i != 9) || (1 + (0xFF ^ (byte)(data[1] + data[2] + data[3] + data[4] + data[5] + data[6] + data[7]))) != data[8])
  {
    return false;
  }

  value_co2 = (int)data[2] * 256 + (int)data[3];
  value_temperature = (int)data[4] - 40;

  return true;
}
////////////////////////////////////////////////////////////////////////////////////////


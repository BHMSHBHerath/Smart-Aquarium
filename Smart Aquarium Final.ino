#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL6xMrUf-1X"
#define BLYNK_TEMPLATE_NAME "test1"

#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <EEPROM.h>
#include <SimpleTimer.h>
#include <ESP32Servo.h>

// WiFi & Blynk Auth
char auth[] = "rQ1M8FPkXwtu6aePs2WTTERfVt92aESL";
char ssid[] = "Redmi Note 11 Pro 5G";
char pass[] = "00000000";

// Sensor pins
#define ONE_WIRE_BUS 12
#define TdsSensorPin 34
#define pH_Pin 32

#define VREF 3.3
#define ADC_RES 4095

#define TEMP 25.0

// Pump and LED pins
#define PUMP_IN1 25
#define PUMP_IN2 23
#define LED_PIN 2

// Servo feeder
Servo feederServo;
int servoPin = 26;
bool isFed = false;

// Ultrasonic pins
#define TRIG_PIN 14
#define ECHO_PIN 27
#define TANK_HEIGHT_CM 30.0  // Set this to your actual tank depth in cm

// Sensors
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
SimpleTimer timer;

float tdsValue = 0.0, ph_act = 0.0, calibration_value = 21.34 - 0.7, temperatureC = 0.0;

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  Blynk.begin(auth, ssid, pass);

  sensors.begin();

  pinMode(LED_PIN, OUTPUT);


  pinMode(PUMP_IN1, OUTPUT);
  pinMode(PUMP_IN2, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  digitalWrite(PUMP_IN1, HIGH);  // Pump ON
  digitalWrite(PUMP_IN2, LOW);

  feederServo.attach(servoPin);
  feederServo.write(0);  // Start position

  timer.setInterval(1000L, readSensors);

  Serial.println("System ready");
}

void readSensors() {
  // Temperature
sensors.requestTemperatures();
  if (sensors.getDeviceCount() == 0 || sensors.getTempCByIndex(0) == DEVICE_DISCONNECTED_C) {
    Serial.println("Temperature sensor not found or disconnected!");
    temperatureC = -1.0;  // Or a safe default
  } else {
    temperatureC = sensors.getTempCByIndex(0);
    Serial.print("Water Temperature: ");
    Serial.print(temperatureC);
    Serial.println(" °C");
    Blynk.virtualWrite(V12, temperatureC);
  }

  // TDS
  int rawADC = analogRead(TdsSensorPin);
  float voltage = rawADC * (VREF / ADC_RES);
  float compensationCoefficient = 1.0 + 0.02 * (temperatureC - 25.0);
  float compensationVoltage = voltage / compensationCoefficient;
  tdsValue = (133.42 * compensationVoltage * compensationVoltage * compensationVoltage) - (255.86 * compensationVoltage * compensationVoltage) + (857.39 * compensationVoltage);
  Serial.print("TDS: ");
  Serial.print(tdsValue, 0);
  Serial.println(" ppm");
  Blynk.virtualWrite(V1, tdsValue);

  // pH
  int buffer_arr[10], temp;
  unsigned long int avgval = 0;
  for (int i = 0; i < 10; i++) {
    buffer_arr[i] = analogRead(pH_Pin);
    delay(30);
  }
  for (int i = 0; i < 9; i++) {
    for (int j = i + 1; j < 10; j++) {
      if (buffer_arr[i] > buffer_arr[j]) {
        temp = buffer_arr[i];
        buffer_arr[i] = buffer_arr[j];
        buffer_arr[j] = temp;
      }
    }
  }
  for (int i = 2; i < 8; i++) {
    avgval += buffer_arr[i];
  }
  float volt = (float)avgval * VREF / ADC_RES / 6;
  ph_act = -5.70 * volt + calibration_value;
  Serial.print("pH Value: ");
  Serial.println(ph_act);
  Blynk.virtualWrite(V3, ph_act);



  // Ultrasonic water level reading
  long duration;
  float distanceCM;

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  duration = pulseIn(ECHO_PIN, HIGH, 30000);  // Timeout after 30ms
  distanceCM = duration * 0.034 / 2;

  float waterLevelCM = TANK_HEIGHT_CM - distanceCM;
  float waterPercent = (waterLevelCM / TANK_HEIGHT_CM) * 100.0;

  waterPercent = constrain(waterPercent, 0, 100);

  Serial.print("Distance to Water Surface: ");
  Serial.print(distanceCM);
  Serial.println(" cm");

  Serial.print("Water Depth Percentage: ");
  Serial.print(waterPercent);
  Serial.println(" %");

  Blynk.virtualWrite(V13, distanceCM);    // Raw distance in cm
  Blynk.virtualWrite(V14, waterPercent);  // Water level %
}

// LED toggle from Blynk (V2)
BLYNK_WRITE(V2) {


  int ledState = param.asInt();
  digitalWrite(LED_PIN, ledState);
}

// Feeder switch from Blynk (V4)
// Feeder switch from Blynk (V4)
BLYNK_WRITE(V4) {
  int value = param.asInt();
  if (value == 1 && !isFed) {
    Serial.println("Feeding started");

        // Rotate back to 0°
    for (int angle = 180; angle >= 0; angle--) {
      feederServo.write(angle);
      delay(5);
    }
    // Rotate from 0° to 180°
    for (int angle = 0; angle <= 180; angle++) {
      feederServo.write(angle);
      delay(5);
    }

    delay(1000);  // Hold at 180° for a second



    Serial.println("Feeding done");
    isFed = true;
  } else if (value == 0) {
    isFed = false;
    Serial.println("Reset feed trigger");
  }
}


void loop() {
  Blynk.run();
  timer.run();
}

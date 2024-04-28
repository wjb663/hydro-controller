// --- DHT --- //
#include "DHT.h"
#define DHTPIN 0           // Digital pin connected to the DHT sensor
#define DHTTYPE DHT11   
DHT dht(DHTPIN, DHTTYPE);

// --- Light Sensor --- //
int light;
float normalized_voltage_val;
float max_v = 3.3;
int max_data_input = 4095;
float lux;
float max_lux = 1000;
float normalized_lux_val;
#define LIGHT_PIN 4
#define SAMPLE_TIME 100


void setup() {
  // put your setup code here, to run once:
  // --- DHT --- //
  Serial.begin(9600);
  Serial.println(F("DHTxx test!"));
  dht.begin();
  void read_DHT();
  void read_light();
}

void loop() {
 read_DHT();
 read_light();
}


// ----------------------------------------------- Functions ----------------------------------------------------- //
void read_DHT(){
  // put your main code here, to run repeatedly:
  // Wait a few seconds between measurements.
  delay(2000);

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  // Compute heat index in Fahrenheit (the default)
  float hif = dht.computeHeatIndex(f, h);
  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);

  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("%  Temperature: "));
  Serial.print(t);
  Serial.print(F("°C "));
  Serial.print(f);
  Serial.print(F("°F  Heat index: "));
  Serial.print(hic);
  Serial.print(F("°C "));
  Serial.print(hif);
  Serial.print(F("°F; "));
}

void read_light(){
  static unsigned int lastMillis = 0;
  if ( millis() - lastMillis > SAMPLE_TIME ) {
      light = analogRead(LIGHT_PIN);
      normalized_voltage_val = (max_v*light)/(max_data_input);     // Normalize the range of 0 - 4095 values to 0 - 3.3V, the voltage range of the data input pin
      normalized_lux_val = (max_lux*normalized_voltage_val)/(max_v);      // Normalize the range of 0 - 3.3V to 0 - 1000 lux, the lux range of the sensor
      Serial.print("Light Sensor Voltage: ");      
      Serial.print(normalized_voltage_val);
      Serial.print("v, ");
      if(normalized_lux_val >= max_lux){
        Serial.println(">1000 lux");
      }
      else{
      Serial.print(normalized_lux_val);      
      Serial.println(" lux");          
      }
      lastMillis = millis();
  }
}  

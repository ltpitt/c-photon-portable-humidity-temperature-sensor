// Libraries includes
#include <SparkFunMAX17043.h>
#include <PietteTech_DHT.h> // DHT22 Sensor Library
#include "PietteTech_DHT/PietteTech_DHT.h" // DHT22 (Temperature and Humidity sensor)
#include "math.h" // Math library for sensor values calculation
#include <HttpClient.h> // Http library to handle http requests
#define DHTTYPE  DHT22 // Sensor type for DHT22 library (can be: DHT11/21/22/AM2301/AM2302)
#define DHTPIN   D6 // Digital pin for communications           
#include "Adafruit_GFX.h" // Adafruit GFX library
#include "Adafruit_PCD8544.h" // Adafruit Library for Nokia 5110 LCD display

// This settings will allow the product to work even without Particle Cloud connection
SYSTEM_MODE(SEMI_AUTOMATIC)
SYSTEM_THREAD(ENABLED)
  
// HttpClient initialization
HttpClient http;
// Headers currently need to be set at init, useful for API keys etc.
http_header_t headers[] = {
    //  { "Content-Type", "application/json" },
    //  { "Accept" , "application/json" },
    { "Accept" , "*/*"},
    { NULL, NULL } // NOTE: Always terminate headers will NULL
};
http_request_t request;
http_response_t response;

// HttpClient function variables
unsigned int nextTime = 0; // Next time to contact the server
String method; // Currently only "get" and "post" are implemented
String hostname;
int port;
String path;

// HARDWARE SPI - Wiring for Nokia5110 LCD Display
// pin A3 - Serial clock out (SCLK)
// pin A5 - Serial data out (DIN/MOSI)
// pin D2 - Data/Command select (D/C) - User defined
// pin A2 - LCD chip select (CS/SS) - User defined
// pin D3 - LCD reset (RST) - User defined
// Adafruit_PCD8544(CS, DC, RST)
Adafruit_PCD8544 display = Adafruit_PCD8544(SS, D2, D3);

// Declaring the DHT22 sensor variables
void dht_wrapper(); // Must be declared before the lib initialization
int n;      // Sensor acquisition counter
double temp;  // Temperature value
double umid;  // Humidity value
double t;   // Variable used in values rounding

// DHT library initialization
PietteTech_DHT DHT(DHTPIN, DHTTYPE, dht_wrapper);

void dht_wrapper() {
    DHT.isrCallback();
}

int display_led = D5; // Nokia 5110 LCD Display led will be connected to D5

double voltage = 0; // Variable to keep track of LiPo voltage
double soc = 0; // Variable to keep track of LiPo state-of-charge (SOC)
bool alert; // Variable to keep track of whether alert has been triggered
char batteryDataJsonString[64]; // Json containing battery data to be sent

bool isSplashscreenShowed = false;

void setup()   {
    
    getBatteryData();
    
    Serial.begin(9600); // Serial setup
    
    Time.zone(1); // Set timezone to Amsterdam
    
    // Debug output for Serial monitor
    Serial.println("Device starting...");
    Serial.println("DHT Example program using DHT.acquireAndWait");
    Serial.print("LIB version: ");
    Serial.println(DHTLIB_VERSION);
    Serial.println("---------------");
    pinMode(display_led, OUTPUT); // We are going to tell our device that D0 (which we named display_led) is going to be output

    // Setting up LCD display
    digitalWrite(display_led, HIGH); // Turn on LCD display led
    display.begin(); // LCD display initialization
    display.setContrast(55); // Setting LCD display contrast e.g 200 is darker than 400
    display.clearDisplay(); // Clear LCD display
    display.setTextSize(1); // Set display text size
    display.setTextColor(BLACK); // Set display text color

}


void loop() {
    getBatteryData();
    getDHT22data(); // Get Humidity and Temperature data from DHT22
    displayTempHum(); // Show Humidity and Temperature retrieved on LCD

    Particle.connect();

    if (temp > 5.0) {
        // If sensor returned a good reading we show it on LCD
        delay(30000); // Then we wait for a bit
        if (Particle.connected()) {
        // And then we publish it online
        // Uncomment or comment below rows accoring to the sensor you are going to flash
        Particle.publish("1_floor_temp", String(temp,1), PRIVATE);
        Particle.publish("1_floor_hum", String(umid, 1), PRIVATE);
        //Particle.publish("2_floor_temp", String(temp,1), PRIVATE);
        //Particle.publish("2_floor_hum", String(umid,1), PRIVATE);
        isOtaEnabled(); // Check if OTA is enabled and act accordingly
        } else {
        // Deep sleep for unlimited time (Manual Mode)
        System.sleep(SLEEP_MODE_DEEP);
        // Deep sleep for limited time / Uncomment if needed
        //System.sleep(SLEEP_MODE_DEEP, 3600);
        }
    }
  
}

void getBatteryData() {
 
    // Set up the MAX17043 LiPo fuel gauge:
	lipo.begin(); // Initialize the MAX17043 LiPo fuel gauge
	// Quick start restarts the MAX17043 in hopes of getting a more accurate
	// guess for the SOC.
	lipo.quickStart();

	// We can set an interrupt to alert when the battery SoC gets too low.
	// We can alert at anywhere between 1% - 32%:
	lipo.setThreshold(20); // Set alert threshold to 20%.

    // lipo.getVoltage() returns a voltage value (e.g. 3.93)
	voltage = lipo.getVoltage();
	// lipo.getSOC() returns the estimated state of charge (e.g. 79%)
	soc = lipo.getSOC();
	// lipo.getAlert() returns a 0 or 1 (0=alert not triggered)
	alert = lipo.getAlert();

	// Those variables will be printed them locally over serial for debugging:
	Serial.print("Voltage: ");
	Serial.print(voltage);  // Print the battery voltage
	Serial.println(" V");

	Serial.print("Alert: ");
	Serial.println(alert);

	Serial.print("Percentage: ");
	Serial.print(soc); // Print the battery state of charge
	Serial.println(" %");
	Serial.println();

    if (alert != 0 and Particle.connected()) {
    	sendHttpRequest("post", "192.168.178.25", 8080, "/portable-sensor/battery", String(soc,1));
    }


}

void displayTempHum() {
    
    if (not isSplashscreenShowed) {

        display.clearDisplay(); // Clear LCD display
        display.setCursor(0,0); // Move display cursor to row 0, col 0
        display.println("Termometretto"); // Print current temperature value using a single decimal
        display.println("      by"); // Print current humidity value using a single decimal
        display.println(" Pipisoft ltd"); // Print current humidity value using a single decimal
        display.println(""); // Print current humidity value using a single decimal
        display.println("<3 <3 :* <3 <3"); // Print current humidity value using a single decimal
        display.display(); // Show information on display
        delay(5000);
        isSplashscreenShowed = false;

    }

    display.clearDisplay(); // Clear LCD display
    display.setCursor(0,0); // Move display cursor to row 0, col 0
    Particle.syncTime();  // Request time synchronization from the Particle Cloud
    waitUntil(Particle.syncTimeDone); // Wait until Photon receives time from Particle Cloud (or connection to Particle Cloud is lost)
    display.println(Time.format("  %d-%m-%Y\n    %H:%M\n"));  // Print current date and time using the specified format
    display.println("Temp: " + String(temp,1)); // Print current temperature value using a single decimal
    display.println("Hum : " + String(umid,1)); // Print current humidity value using a single decimal
    display.println("Batt: " + String(soc,1)); // Print current humidity value using a single decimal
    display.display(); // Show information on display
    
}


void getDHT22data() {
    delay(3000);
    // Increase acquisition counter
    n++;

    // Acquire data from sensor
    int result = DHT.acquireAndWait();

    // Error handling
    switch (result) {
        case DHTLIB_OK:
            Serial.println("OK");
            break;
        case DHTLIB_ERROR_CHECKSUM:
            Serial.println("Error\n\r\tChecksum error");
            break;
        case DHTLIB_ERROR_ISR_TIMEOUT:
            Serial.println("Error\n\r\tISR time out error");
            break;
        case DHTLIB_ERROR_RESPONSE_TIMEOUT:
            Serial.println("Error\n\r\tResponse time out error");
            break;
        case DHTLIB_ERROR_DATA_TIMEOUT:
            Serial.println("Error\n\r\tData time out error");
            break;
        case DHTLIB_ERROR_ACQUIRING:
            Serial.println("Error\n\r\tAcquiring");
            break;
        case DHTLIB_ERROR_DELTA:
            Serial.println("Error\n\r\tDelta time to small");
            break;
        case DHTLIB_ERROR_NOTSTARTED:
            Serial.println("Error\n\r\tNot started");
            break;
        default:
            Serial.println("Unknown error");
            break;
    }

    // Humidity value retrieval and rounding
    umid=DHT.getHumidity();
    t=umid-floor(umid);  // Get only decimal part
    t=t*100; // Multiply by 100 decimal part, get xx.xxxxxxxxx
    t=floor(t); // Get only last two characters of decimal part, get yy
    umid=floor(umid)+(t/100);

    // Temperature value retrieval and rounding
    temp=DHT.getCelsius();
    t=temp-floor(temp); // Get only decimal part
    t=t*100; // Multiply by 100 decimal part, get xx.xxxxxxxxx
    t=floor(t); // Get only last two characters of decimal part, get yy
    temp=floor(temp)+(t/100);

    // Temperature and Humidity 
    Serial.print("\n");
    Serial.print(n);
    Serial.print(": Retrieving information from sensor: ");
    Serial.print("Read sensor: ");
    // Print sensor data to serial
    Serial.print("Humidity (%): ");
    Serial.println(DHT.getHumidity(), 2);
    Serial.print("Temperature (oC): ");
    Serial.println(DHT.getCelsius(), 2);
    Serial.print("Temperature (oF): ");
    Serial.println(DHT.getFahrenheit(), 2);
    Serial.print("Temperature (K): ");
    Serial.println(DHT.getKelvin(), 2);
    Serial.print("Dew Point (oC): ");
    Serial.println(DHT.getDewPoint());
    Serial.print("Dew Point Slow (oC): ");
    Serial.println(DHT.getDewPointSlow());

}

// sendHttpRequest function
void sendHttpRequest(String method, String hostname, int port, String path, String body)
{
    
    request.hostname = hostname;
    request.port = port;
    request.path = path;
    request.body = body;

    Serial.print("Method: " + method + "\n");
    Serial.print("Hostname: " + hostname + "\n");
    Serial.print("Port: " + String(port) + "\n");
    Serial.print("Path: " + path + "\n");
    Serial.print("Body: " + body + "\n");

    if (method == "post") {
        
        Serial.print("\nSending http request\n");
        http.post(request, response, headers);
        Serial.print("Application>\tResponse status: ");
        Serial.println(response.status);
        Serial.print("Application>\tHTTP Response Body: ");
        Serial.println(response.body);
        
      } else if (method == "get") {
          
        Serial.print("\nSending http request\n");
        http.get(request, response, headers);
        Serial.print("Application>\tResponse status: ");
        Serial.println(response.status);
        Serial.print("Application>\tHTTP Response Body: ");
        Serial.println(response.body);
        
      } else {
          
        Serial.print("Please check your method: only post and get are currently supported");
        
    }
}




void isOtaEnabled() {
    // Check if OTA is enabled via http
    sendHttpRequest("get", "192.168.178.25", 8080, "/ota", "");
    Serial.print("Application>\tResponse status: ");
    Serial.println(response.status);
    Serial.print("Application>\tHTTP Response Body: ");
    Serial.println(response.body);

    // Check if OTA update is enabled
    if (response.body=="off") {
        digitalWrite(display_led, LOW); // Turn oFF LCD display led
        // Put Particle Photon in deep sleep for 900 seconds (15 minutes)
        System.sleep(SLEEP_MODE_DEEP, 3600);
    } else {
        // Turn on onboard led
        digitalWrite(D7, HIGH);
        // Send out notification
        sendHttpRequest("get", "192.168.178.25", 8080, "/ota/notification", "");
        Serial.print("Application>\tResponse status: ");
        Serial.println(response.status);
        Serial.print("Application>\tHTTP Response Body: ");
        Serial.println(response.body);
        // Delay 900000 milliseconds (15 minutes)
        delay(900000);
    }

}

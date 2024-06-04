/**
 * BasicHTTPClient.ino
 *
 *  Created on: 24.05.2015
 *
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "esp_wpa2.h"
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#define PIN D0 
#define NUMPIXELS 60
#define DELAY 1000 // 1s Delay

uint16_t time_limit = 10;
int time_elapsed = 0;
int timeInSeconds;
bool start_timer = false;
bool new_timer = false;

// Webpage to show the timer has stopped with button to return to main screen
char webpage3[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Timer Stop</title>
    <style>
        body {
            display: flex;
            flex-direction: column;
            justify-content: center;
            align-items: center;
            height: 100vh;
            margin: 0;
            font-family: Arial, sans-serif;
            background-color: #f0f0f0;
        }
        .container {
            text-align: center;
            background-color: #ffffff;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);
        }
        h1 {
            color: #333333;
        }
        button {
            padding: 10px 20px;
            font-size: 16px;
            color: #ffffff;
            background-color: #007bff;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            margin-top: 20px;
        }
        button:hover {
            background-color: #0056b3;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Timer Stopped</h1>
        <p>The timer has been successfully stopped.</p>
        <button onclick="window.location = 'http://' + location.hostname">New Timer</button>
    </div>
</body>
</html>

)=====";


//Webpage which shows that the timer has started
char webpage2[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Timer Started</title>
    <style>
        body {
            display: flex;
            flex-direction: column;
            justify-content: center;
            align-items: center;
            height: 100vh;
            margin: 0;
            font-family: Arial, sans-serif;
            background-color: #f0f0f0;
        }
        .container {
            text-align: center;
            background-color: #ffffff;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);
        }
        h1 {
            color: #333333;
        }
        button {
            padding: 10px 20px;
            font-size: 16px;
            color: #ffffff;
            background-color: #007bff;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            margin-top: 20px;
        }
        button:hover {
            background-color: #0056b3;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Timer Started</h1>
        <p>The timer has been successfully started.</p>
        <button onclick="window.location = 'http://' + location.hostname + '/off'">Stop</button>
        <button onclick="window.location = 'http://' + location.hostname">New Timer</button>
    </div>
</body>
</html>

)=====";

// Main Webpage with timer inputs
char webpage[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Time Input with Start and Restart</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            text-align: center;
            margin-top: 50px;
        }
        .container {
            display: inline-block;
            padding: 20px;
            border: 1px solid #ccc;
            border-radius: 5px;
        }
        input[type="number"] {
            font-size: 1.2em;
            padding: 5px;
            margin: 10px 0;
            width: 80px;
        }
        button {
            font-size: 1em;
            padding: 10px 20px;
            margin: 5px;
            cursor: pointer;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Time Input Example</h1>
        <input type="number" id="minutesInput" placeholder="Minutes" min="0">
        <input type="number" id="secondsInput" placeholder="Seconds" min="0" max="59">
        <br>
        <button onclick="sendTime()">Start</button>
    </div>

    <script>
        function sendTime() {
            let minutes = document.getElementById('minutesInput').value;
            let seconds = document.getElementById('secondsInput').value;
            if (minutes === "") minutes = 0;
            if (seconds === "") seconds = 0;

            minutes = parseInt(minutes);
            seconds = parseInt(seconds);

            if (isNaN(minutes) || isNaN(seconds) || minutes < 0 || seconds < 0 || seconds > 59) {
                alert('Please enter valid minutes and seconds.');
                return;
            }

            let totalSeconds = (minutes * 60) + seconds;
            window.location = 'http://' + location.hostname + '/on?seconds=' + encodeURIComponent(totalSeconds);
        }
    </script>
</body>
</html>
)=====";

// Creating Neopixel object identity
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

#define EAP_ANONYMOUS_IDENTITY "a2-almuaini@uwe.ac.uk" //anonymous identity
#define EAP_IDENTITY "a2-almuaini@uwe.ac.uk"                 //user identity
#define EAP_PASSWORD "5xuMSUY34628" //eduroam user password
const char* ssid = "eduroam"; // eduroam SSID

WebServer server(80);

void logRequest(int HTTP_CODE) {
  String logMessage = "esp32 \"";
  logMessage += (server.method() == HTTP_GET) ? "GET" : "POST";
  logMessage += " " + server.uri() + "\" " + HTTP_CODE;
  Serial.println(logMessage);
}

// Changes colours of LED Strip based on time left & Total Time
void startTimerLights(int time_limit)
{
  // Light will be set to green until 10 seconds left. Then turn to yellow until time reaches 0. Then the timer will stay red for 5 seconds before returning to white.
  if(time_limit <= 60)
  {
    if (time_elapsed > 10) // Green Light
    {
      for (int i = 0; i < NUMPIXELS; ++i) 
            { // iterate over all pixels
              pixels.setPixelColor(i, pixels.Color(0, 255, 0)); // set color of pixel
            }
            pixels.show();
    }
    else if (time_elapsed <= 10 || time_elapsed > 0) // Yellow Light
    {
      for (int i = 0; i < NUMPIXELS; ++i) 
            { // iterate over all pixels
              pixels.setPixelColor(i, pixels.Color(169, 100, 0)); // set color of pixel
            }
            pixels.show();
    }
    if (time_elapsed <= 0) // Red Light + Reset
    {
      for (int i = 0; i < NUMPIXELS; ++i) 
                { // iterate over all pixels
                  pixels.setPixelColor(i, pixels.Color(255, 0, 0)); // set color of pixel
                }
                pixels.show();
                start_timer = false;
                delay(5000);
                logRequest(200);
                server.send(200, "text/html", webpage);
                start_timer = false;
    }    

  }  

  else if(time_limit > 60 || time_limit <= 600) // Light will be set to green until 30 seconds left.
  {
     if (time_elapsed > 30) // Green Light
    {
      for (int i = 0; i < NUMPIXELS; ++i) 
            { // iterate over all pixels
              pixels.setPixelColor(i, pixels.Color(0, 255, 0)); // set color of pixel
            }
            pixels.show();
    }

    else if (time_elapsed <= 30 || time_elapsed > 0) // Yellow Light
    {
      for (int i = 0; i < NUMPIXELS; ++i) 
            { // iterate over all pixels
              pixels.setPixelColor(i, pixels.Color(169, 100, 0)); // set color of pixel
            }
            pixels.show();
    }

    if (time_elapsed <= 0) // Red Light + Reset
    {
      for (int i = 0; i < NUMPIXELS; ++i) 
                { // iterate over all pixels
                  pixels.setPixelColor(i, pixels.Color(255, 0, 0)); // set color of pixel
                }
                pixels.show();
                start_timer = false;
                delay(5000);
                logRequest(200);
                server.send(200, "text/html", webpage);
                start_timer = false;
    }    
  }  

  
  else if(time_limit > 600) // Light will be set to green until 60 seconds left. 

    if(time_elapsed > 60) // Green Light
    {
      for (int i = 0; i < NUMPIXELS; ++i) 
            { // iterate over all pixels
              pixels.setPixelColor(i, pixels.Color(0, 255, 0)); // set color of pixel
            }
            pixels.show();
    }
    
    else if (time_elapsed <= 60 || time_elapsed > 0) // Yellow Light
    {
      for (int i = 0; i < NUMPIXELS; ++i) 
            { // iterate over all pixels
              pixels.setPixelColor(i, pixels.Color(169, 100, 0)); // set color of pixel
            }
            pixels.show();
    }

    if (time_elapsed <= 0) // Red Light + Reset
    {
      for (int i = 0; i < NUMPIXELS; ++i) 
                { // iterate over all pixels
                  pixels.setPixelColor(i, pixels.Color(255, 0, 0)); // set color of pixel
                }
                pixels.show();
                start_timer = false;
                delay(5000);
                logRequest(200);
                server.send(200, "text/html", webpage);
                start_timer = false;
    }    
      
  }
    time_elapsed--;
    delay(DELAY); // wait for 5ms 
}

//This funcation handles user input converstion, timer start & page transition
void handleSetTime() {
    if (server.hasArg("seconds")) {
        timeInSeconds = server.arg("seconds").toInt(); // Converts HTML user input int C++ interger variables
        // Set variables for startTimerLights() 
        time_elapsed = timeInSeconds;
        new_timer = true;
        start_timer = true;
        // Switches to "Timer Started" page with options to stop and restart timer
        server.send(200, "text/html", webpage2);
    } else {
        Serial.println("No time recieved"); // If false input requested, webpage doesnt transition
        server.send(400, "text/plain", "Bad Request");
    }
}

// Default Webpage function
void handleRoot() {
  // Resets variable to stop and active timers
  new_timer = false;
  start_timer = false;
  logRequest(200);
  server.send_P(200, "text/html", webpage); // Changes to main webpage
}


// Error 404 Page 
void handleNotFound() {
  String message = "Error 404: Page Not Found ¯\_(ツ)_/¯\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  logRequest(404);
  server.send(404, "text/plain", message);
}


// I think this is legacy code but I'm too afarid to delete it so... it stays ¯\_(ツ)_/¯
void ledOn() {
  start_timer = true;
  logRequest(200);
  server.send(200, "text/plain", "LED ON");
  digitalWrite(LED_BUILTIN, HIGH);
}

// Stop Button Funcationallity 
void ledOff() {
  // Reset Variables
  start_timer = false;
  timeInSeconds = 0;
  time_limit = 0;
  time_elapsed = 0;
  // "Timer Stopped" Webpage Transition
  logRequest(200);
  server.send(200, "text/html", webpage3);
}


// IDK what this is and it scares me but it makes the wifi works ⊙﹏⊙∥
int counter = 0;
const char* test_root_ca = \
                           "-----BEGIN CERTIFICATE-----\n" \
                           "MIIEsTCCA5mgAwIBAgIQCKWiRs1LXIyD1wK0u6tTSTANBgkqhkiG9w0BAQsFADBh\n" \
                           "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n" \
                           "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n" \
                           "QTAeFw0xNzExMDYxMjIzMzNaFw0yNzExMDYxMjIzMzNaMF4xCzAJBgNVBAYTAlVT\n" \
                           "MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n" \
                           "b20xHTAbBgNVBAMTFFJhcGlkU1NMIFJTQSBDQSAyMDE4MIIBIjANBgkqhkiG9w0B\n" \
                           "AQEFAAOCAQ8AMIIBCgKCAQEA5S2oihEo9nnpezoziDtx4WWLLCll/e0t1EYemE5n\n" \
                           "+MgP5viaHLy+VpHP+ndX5D18INIuuAV8wFq26KF5U0WNIZiQp6mLtIWjUeWDPA28\n" \
                           "OeyhTlj9TLk2beytbtFU6ypbpWUltmvY5V8ngspC7nFRNCjpfnDED2kRyJzO8yoK\n" \
                           "MFz4J4JE8N7NA1uJwUEFMUvHLs0scLoPZkKcewIRm1RV2AxmFQxJkdf7YN9Pckki\n" \
                           "f2Xgm3b48BZn0zf0qXsSeGu84ua9gwzjzI7tbTBjayTpT+/XpWuBVv6fvarI6bik\n" \
                           "KB859OSGQuw73XXgeuFwEPHTIRoUtkzu3/EQ+LtwznkkdQIDAQABo4IBZjCCAWIw\n" \
                           "HQYDVR0OBBYEFFPKF1n8a8ADIS8aruSqqByCVtp1MB8GA1UdIwQYMBaAFAPeUDVW\n" \
                           "0Uy7ZvCj4hsbw5eyPdFVMA4GA1UdDwEB/wQEAwIBhjAdBgNVHSUEFjAUBggrBgEF\n" \
                           "BQcDAQYIKwYBBQUHAwIwEgYDVR0TAQH/BAgwBgEB/wIBADA0BggrBgEFBQcBAQQo\n" \
                           "MCYwJAYIKwYBBQUHMAGGGGh0dHA6Ly9vY3NwLmRpZ2ljZXJ0LmNvbTBCBgNVHR8E\n" \
                           "OzA5MDegNaAzhjFodHRwOi8vY3JsMy5kaWdpY2VydC5jb20vRGlnaUNlcnRHbG9i\n" \
                           "YWxSb290Q0EuY3JsMGMGA1UdIARcMFowNwYJYIZIAYb9bAECMCowKAYIKwYBBQUH\n" \
                           "AgEWHGh0dHBzOi8vd3d3LmRpZ2ljZXJ0LmNvbS9DUFMwCwYJYIZIAYb9bAEBMAgG\n" \
                           "BmeBDAECATAIBgZngQwBAgIwDQYJKoZIhvcNAQELBQADggEBAH4jx/LKNW5ZklFc\n" \
                           "YWs8Ejbm0nyzKeZC2KOVYR7P8gevKyslWm4Xo4BSzKr235FsJ4aFt6yAiv1eY0tZ\n" \
                           "/ZN18bOGSGStoEc/JE4ocIzr8P5Mg11kRYHbmgYnr1Rxeki5mSeb39DGxTpJD4kG\n" \
                           "hs5lXNoo4conUiiJwKaqH7vh2baryd8pMISag83JUqyVGc2tWPpO0329/CWq2kry\n" \
                           "qv66OSMjwulUz0dXf4OHQasR7CNfIr+4KScc6ABlQ5RDF86PGeE6kdwSQkFiB/cQ\n" \
                           "ysNyq0jEDQTkfa2pjmuWtMCNbBnhFXBYejfubIhaUbEv2FOQB3dCav+FPg5eEveX\n" \
                           "TVyMnGo=\n" \
                           "-----END CERTIFICATE-----\n";
WiFiClientSecure client;

void setup() {
  pixels.begin(); // Turns on Neopixel
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  delay(10);
  Serial.println();
  Serial.print("Connecting to network: ");
  Serial.println(ssid);
  WiFi.disconnect(true);  //disconnect form wifi to set new wifi connection
  WiFi.mode(WIFI_STA); //init wifi mode
  esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)EAP_ANONYMOUS_IDENTITY, strlen(EAP_ANONYMOUS_IDENTITY)); //provide identity
  esp_wifi_sta_wpa2_ent_set_username((uint8_t *)EAP_IDENTITY, strlen(EAP_IDENTITY)); //provide username
  esp_wifi_sta_wpa2_ent_set_password((uint8_t *)EAP_PASSWORD, strlen(EAP_PASSWORD)); //provide password
  esp_wifi_sta_wpa2_ent_enable();
  WiFi.begin(ssid); //connect to wifi
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    counter++;
    if (counter >= 60) { //after 30 seconds timeout - reset board (on unsucessful connection)
      ESP.restart();
    }
  }
  client.setCACert(test_root_ca);
  //client.setCertificate(test_client_cert); // for client verification - certificate
  //client.setPrivateKey(test_client_key);  // for client verification - private key
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address set: ");
  Serial.println(WiFi.localIP()); //print LAN IP
  if (MDNS.begin("esp32")) {
    Serial.println("MDNS responder started");
  }


  // Non of these functions are used in C++. Transitions between pages are handled in HTML
  server.on("/", handleRoot);
  server.on("/on",handleSetTime); 
  server.on("/off",ledOff);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started")
}


void loop() {
  server.handleClient();
  if(start_timer == true && new_timer == true) //Starts Timer
  {  
    startTimerLights(timeInSeconds);
  }
  else
  {
    for (int i = 0; i < NUMPIXELS; ++i) // Sets strip to white
            { // iterate over all pixels
              pixels.setPixelColor(i, pixels.Color(255,180,60)); // set color of pixel
            }
    pixels.show();
  }

  delay(2);
}

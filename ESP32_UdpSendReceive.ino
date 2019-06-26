#include <WiFi.h>
#include <WiFiUdp.h>
#include "wifi_password.h"

//接続先passwordとSSIDは別ヘッダファイルにあるよ↑

const char * udpAddress = "192.168.1.7";
const int udpPort = 8888;

//create UDP instance
WiFiUDP udp;

hw_timer_t *timer = NULL;
volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

volatile uint32_t isrCounter = 0;
volatile uint32_t lastIsrAt = 0;

volatile uint8_t counter = 0;

void IRAM_ATTR onTimer(){
  // Increment the counter and set the time of ISR
  portENTER_CRITICAL_ISR(&timerMux);
  isrCounter++;
  lastIsrAt = millis();
  portEXIT_CRITICAL_ISR(&timerMux);
  // Give a semaphore that we can check in the loop
  xSemaphoreGiveFromISR(timerSemaphore, NULL);
  // It is safe to use digitalRead/Write here if you want to toggle an output
}

void udpSend(){
  
  //data will be sent to server
  uint8_t buffer[50]="Hello World";
  
  udp.beginPacket(udpAddress, udpPort);
  udp.write(buffer,12);
  udp.endPacket();
  memset(buffer, 0, 50);
  
  counter++;
}

void setup(){
  Serial.begin(115200);
  
  //Connect to the WiFi network
   WiFi.begin(ssid, pwd);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  //This initializes udp and transfer buffer
  udp.begin(udpPort);

  // Create semaphore to inform us when the timer has fired
  timerSemaphore = xSemaphoreCreateBinary();

  // Use 1st timer of 4 (counted from zero).
  // Set 80 divider for prescaler (see ESP32 Technical Reference Manual for more
  // info).
  timer = timerBegin(0, 80, true);

  // Attach onTimer function to our timer.
  timerAttachInterrupt(timer, &onTimer, true);

  // Set alarm to call onTimer function every second (value in microseconds).
  // Repeat the alarm (third parameter)
  timerAlarmWrite(timer, 1000000, true);

  // Start an alarm
  timerAlarmEnable(timer);
}

void loop(){
  uint8_t buff[50];
  memset(buff, 0, 50);
  //processing incoming packet, must be called before reading the buffer
  udp.parsePacket();
  //receive response from server, it will be HELLO WORLD
  if(udp.read(buff, 50) > 0){
    Serial.print("Server to client: ");
    Serial.println((char *)buff);
  }

  if (xSemaphoreTake(timerSemaphore, 0) == pdTRUE){
    uint32_t isrCount = 0, isrTime = 0;
    portENTER_CRITICAL(&timerMux); // Read the interrupt count and time
    isrCount = isrCounter;
    isrTime = lastIsrAt;
    portEXIT_CRITICAL(&timerMux);
    udpSend();
  }
}

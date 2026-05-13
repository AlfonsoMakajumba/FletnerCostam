/*
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleServer.cpp
    Ported to Arduino ESP32 by Evandro Copercini
    updates by chegewara
*/

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Encoder.h>

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID "43a934fe-4ff0-4476-9c20-eb6b8f76121b"
#define kierunek_UUID "0a48ab5a-393d-4041-9702-a2a83cc6f4ee"
#define velocity_UUID "dc8f48ec-69a1-45cd-ab1b-fc4f5a432458"

#define ENCODER_B 9
#define ENCODER_A 10
volatile unsigned int counter = 0;

String str_kierunek = "";
String last_kierunek = "";
BLECharacteristic *kierunek;
BLECharacteristic *velocity;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE work!");

  pinMode(11, INPUT);
  pinMode(12, INPUT);
  pinMode(ENCODER_A, INPUT_PULLUP);
  pinMode(ENCODER_B, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(ENCODER_A), read_encoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_B), read_encoder, CHANGE);

  if (!BLEDevice::init("Pilot Server")) {
    Serial.println("BLE initialization failed!");
    return;
  }
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  kierunek = pService->createCharacteristic(kierunek_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  velocity = pService->createCharacteristic(velocity_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  
  pService->start();
  // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Characteristic defined! Now you can read it in your phone!");
}

void loop() {
  if(analogRead(11) < 50){
    str_kierunek = "prawo";
  }
  if(analogRead(12) < 50){
    str_kierunek = "lewo";
  }
  delay(100);
  if(last_kierunek != str_kierunek){
    kierunek->setValue(str_kierunek);
    kierunek->notify();
    Serial.println(str_kierunek);
    last_kierunek = str_kierunek; 
  }
  static int lastCounter = 0;
 
  // If count has changed print the new value to serial
  if(counter != lastCounter){
    Serial.println(counter);
    velocity->setValue(int(counter));
    velocity->notify();
    lastCounter = counter;
  }
}

void read_encoder(){
  static uint8_t old_AB = 3;  // Lookup table index
  static int8_t encval = 0;   // Encoder value  
  static const int8_t enc_states[]  = {0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0}; // Lookup table
 
  old_AB <<=2;  // Remember previous state
 
  if (digitalRead(ENCODER_A)) old_AB |= 0x02; // Add current state of pin A
  if (digitalRead(ENCODER_B)) old_AB |= 0x01; // Add current state of pin B
   
  encval += enc_states[( old_AB & 0x0f )];
 
  // Update counter if encoder has rotated a full indent, that is at least 4 steps
  if( encval > 3 ) {        // Four steps forward
    counter = counter + 5;
    if(counter > 255)counter = 255;             // Increase counter
    encval = 0;
  }
  else if( encval < -3 ) {  // Four steps backwards
   counter = counter - 5;               // Decrease counter
   encval = 0;
   if(counter > 255)counter = 0;  
  }
}

  

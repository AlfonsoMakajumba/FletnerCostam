// HMI FLETTNER ROTOR - SERVER
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Encoder.h>
#include "esp_pm.h"

// UUID generator
// https://www.uuidgenerator.net/

#define SERVICE_UUID "43a934fe-4ff0-4476-9c20-eb6b8f76121b"
#define kierunek_UUID "0a48ab5a-393d-4041-9702-a2a83cc6f4ee"
#define velocity_UUID "dc8f48ec-69a1-45cd-ab1b-fc4f5a432458"

#define ENCODER_B 9
#define ENCODER_A 10
volatile  int counter = 0;
int currentCounter = 0;
bool changed = false;
uint8_t timer = 400;


#define CPU_FREQ 80 //can change if interrups are broken

String str_kierunek = "";
String last_kierunek = "";
BLECharacteristic *kierunek;
BLECharacteristic *velocity;

#define ReadvertTimeSafe 5000 //in ms

class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer* pServer)
  {
    Serial.println("Client connected");
    pServer->updateConnParams(pServer->getConnId(), 0x28, 0x3C, 0 , 400); //you can safely delete this if somethings not working, its a minmaxing trick || also if it wants 4 params just delete "pServer->getConnId()"
    timer = 400;
  }
  void onDisconnect(BLEServer* pServer)
  {
    Serial.println("Client disconnected, readvertising");
    BLEDevice::startAdvertising();
  }
};

BLEServer *pServer = nullptr;

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
   if(counter < 0 )counter = 0;
   encval = 0;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Server booting up");

  setCpuFrequencyMhz(CPU_FREQ);
  esp_pm_config_esp32_t pmConfig = {
    .max_freq_mhz = CPU_FREQ,
    .min_freq_mhz = 10,
    .light_sleep_enable = true //jak sie jebie to do zmiany
  };
  esp_pm_configure(&pmConfig);

  pinMode(11, INPUT_PULLUP);
  pinMode(12, INPUT_PULLUP);
  pinMode(ENCODER_A, INPUT_PULLUP);
  pinMode(ENCODER_B, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(ENCODER_A), read_encoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_B), read_encoder, CHANGE);

  BLEDevice::init("Pilot Server");
 
  pServer = BLEDevice::createServer();
  
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  kierunek = pService->createCharacteristic(kierunek_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  velocity = pService->createCharacteristic(velocity_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  
  pService->start();
  // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinInterval(0x0320); //can be deleted
  pAdvertising->setMaxInterval(0x0320); //can be deleted
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Characteristics defined");
}

void loop() {
  changed = false;
  if(timer != 0)
  {
    kierunek->notify();
    timer--;
  }
  if(digitalRead(11) == LOW){
    if(str_kierunek != "prawo")
    {
      str_kierunek = "prawo";
      changed = true;
    }
  }
  else if(digitalRead(12) == LOW){
    if(str_kierunek != "lewo")
    {
      str_kierunek = "lewo";
      changed = true;
    }
  }
  
  if(changed)
  {
    kierunek->setValue(str_kierunek.c_str());
    counter = 0;
    Serial.println(str_kierunek);
    kierunek->notify();
  }

  static int lastCounter = 0;
 
  // If count has changed print the new value to serial
  noInterrupts();
  currentCounter = counter;
  interrupts();
  if(currentCounter != lastCounter){
    Serial.println(currentCounter);
    velocity->setValue(int(currentCounter));
    velocity->notify();
    lastCounter = currentCounter;
  }
  delay(50);
}  
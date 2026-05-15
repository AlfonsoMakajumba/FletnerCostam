#include <Arduino.h>
#include "BLEDevice.h"

// ===== UUID =====
static BLEUUID serviceUUID("43a934fe-4ff0-4476-9c20-eb6b8f76121b");
static BLEUUID kierunek_UUID("0a48ab5a-393d-4041-9702-a2a83cc6f4ee");
static BLEUUID velocity_UUID("dc8f48ec-69a1-45cd-ab1b-fc4f5a432458");

// ===== STATE =====
static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;

static BLERemoteCharacteristic *pRemoteCharacteristickierunek;
static BLERemoteCharacteristic *pRemoteCharacteristicvelocity;
static BLEAdvertisedDevice *myDevice;

String lastKierunek = "";

int value = 0;

// ===== VELOCITY CALLBACK =====
static void notifyCallback(
  BLERemoteCharacteristic *pBLERemoteCharacteristic,
  uint8_t *pData,
  size_t length,
  bool isNotify)
{
  if (pBLERemoteCharacteristic->getUUID().equals(BLEUUID("dc8f48ec-69a1-45cd-ab1b-fc4f5a432458"))) {

    memcpy(&value, pData, sizeof(int));

    Serial.print("DATA: ");
    Serial.println(value);
  }
}

// ===== CLIENT CALLBACK =====
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient *pclient) {}

  void onDisconnect(BLEClient *pclient) {
    connected = false;
    Serial.println("onDisconnect");
  }
};

// ===== CONNECT =====
bool connectToServer() {

  Serial.print("Lacze sie z ");
  Serial.println(myDevice->getAddress().toString().c_str());

  BLEClient *pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());

  pClient->connect(myDevice);
  Serial.println("Polaczono z serwerem");

  BLERemoteService *pRemoteService = pClient->getService(serviceUUID);
  if (!pRemoteService) {
    Serial.println("Brak serwera");
    pClient->disconnect();
    return false;
  }

  // ===== KIERUNEK =====
  pRemoteCharacteristickierunek = pRemoteService->getCharacteristic(kierunek_UUID);
  if (!pRemoteCharacteristickierunek) {
    Serial.println("Brak kierunku");
    pClient->disconnect();
    return false;
  }

  if (pRemoteCharacteristickierunek->canNotify()) {
    pRemoteCharacteristickierunek->registerForNotify(notifyCallback);
  }

  // ===== VELOCITY =====
  pRemoteCharacteristicvelocity = pRemoteService->getCharacteristic(velocity_UUID);
  if (!pRemoteCharacteristicvelocity) {
    Serial.println("Brak velocity");
    pClient->disconnect();
    return false;
  }

  if (pRemoteCharacteristicvelocity->canNotify()) {
    pRemoteCharacteristicvelocity->registerForNotify(notifyCallback);
  }

  connected = true;
  return true;
}

// ===== SCAN =====
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {

    if (advertisedDevice.haveServiceUUID() &&
        advertisedDevice.isAdvertisingService(serviceUUID)) {

      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = true;
    }
  }
};

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  Serial.println("Start BLE...");

  BLEDevice::init("");

  BLEScan *pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);

  pinMode(13, OUTPUT);
  pinMode(12, OUTPUT);
  pinMode(11, OUTPUT);
}

// ===== LOOP =====
void loop() {

  if (doConnect) {
    if (connectToServer()) {
      Serial.println("OK polaczenie");
    } else {
      Serial.println("Blad polaczenia");
    }
    doConnect = false;
  }

  if (!connected) return;

  // ===== KIERUNEK =====
  String kierunek = String(pRemoteCharacteristickierunek->readValue().c_str());

  if (kierunek != lastKierunek) {

    lastKierunek = kierunek;

    if (kierunek == "prawo") {
      Serial.println("PRAWO");
    }
    else if (kierunek == "lewo") {
      Serial.println("LEWO");
    }
  }

  // ===== VELOCITY (POPRAWIONE) =====

  // ===== STEROWANIE =====
  if (kierunek == "prawo") {
    digitalWrite(13, HIGH);
    digitalWrite(12, LOW);
  }
  else if (kierunek == "lewo") {
    digitalWrite(12, HIGH);
    digitalWrite(13, LOW);
  }

  analogWrite(11, value);

  delay(50);
}
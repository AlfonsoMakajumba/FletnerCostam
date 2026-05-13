#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ===== UUID =====
static BLEUUID serviceUUID("43a934fe-4ff0-4476-9c20-eb6b8f76121b");
static BLEUUID intCharUUID("dc8f48ec-69a1-45cd-ab1b-fc4f5a432458");

// ===== BLE =====
BLECharacteristic* intCharacteristic;
bool deviceConnected = false;

// ===== ENKODER PINS =====
#define CLK  18
#define DT   19

volatile int encoderValue = 0;
volatile int lastCLK = HIGH;

// ============================
// ENKODER ISR
// ============================
void IRAM_ATTR readEncoder() {
    int clkState = digitalRead(CLK);
    int dtState  = digitalRead(DT);

    if (clkState != lastCLK) {
        if (dtState != clkState) {
            encoderValue++;
        } else {
            encoderValue--;
        }
    }

    lastCLK = clkState;
}

// ============================
// CALLBACK BLE
// ============================
class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("Client connected");
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("Client disconnected");
        pServer->startAdvertising();
    }
};

// ============================
// SETUP
// ============================
void setup() {
    Serial.begin(115200);

    // ===== ENKODER =====
    pinMode(CLK, INPUT_PULLUP);
    pinMode(DT, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(CLK), readEncoder, CHANGE);

    // ===== BLE =====
    BLEDevice::init("ESP32-ENCODER");

    BLEServer* pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService* pService = pServer->createService(serviceUUID);

    intCharacteristic = pService->createCharacteristic(
        intCharUUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY
    );

    intCharacteristic->addDescriptor(new BLE2902());

    pService->start();

    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(serviceUUID);
    BLEDevice::startAdvertising();

    Serial.println("BLE Server started");
}

// ============================
// LOOP
// ============================
void loop() {

    if (deviceConnected) {

        int value;

        // bezpieczne pobranie z ISR
        noInterrupts();
        value = encoderValue;
        interrupts();

        intCharacteristic->setValue((uint8_t*)&value, sizeof(value));
        intCharacteristic->notify();

        Serial.print("Encoder value: ");
        Serial.println(value);

        delay(50); // szybkie odświeżanie
    }
}
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>
#include <BLERemoteCharacteristic.h>

// ===== UUID =====
static BLEUUID serviceUUID("43a934fe-4ff0-4476-9c20-eb6b8f76121b");

static BLEUUID intCharUUID("dc8f48ec-69a1-45cd-ab1b-fc4f5a432458");
static BLEUUID stringCharUUID("0a48ab5a-393d-4041-9702-a2a83cc6f4ee");

// ===== Adres urządzenia BLE =====
static BLEAddress serverAddress("A0:F2:62:EC:B3:C9");

// ===== Characteristic pointers =====
BLERemoteCharacteristic* intCharacteristic;
BLERemoteCharacteristic* stringCharacteristic;

BLEClient* pClient;

// ============================
// CALLBACK DLA INT
// ============================
void intNotifyCallback(
    BLERemoteCharacteristic* pBLERemoteCharacteristic,
    uint8_t* pData,
    size_t length,
    bool isNotify)
{
    if (length >= sizeof(int)) {
        int receivedValue;

        memcpy(&receivedValue, pData, sizeof(int));

        Serial.print("Odebrany INT: ");
        Serial.println(receivedValue);
    }
}

// ============================
// CALLBACK DLA STRING
// ============================
void stringNotifyCallback(
    BLERemoteCharacteristic* pBLERemoteCharacteristic,
    uint8_t* pData,
    size_t length,
    bool isNotify)
{
    String receivedString = "";

    for (size_t i = 0; i < length; i++) {
        receivedString += (char)pData[i];
    }

    Serial.print("Odebrany STRING: ");
    Serial.println(receivedString);
}

// ============================
// CONNECT
// ============================
bool connectToServer() {

    pClient = BLEDevice::createClient();

    Serial.println("Łączenie z BLE server...");

    if (!pClient->connect(serverAddress)) {
        Serial.println("Nie udało się połączyć");
        return false;
    }

    Serial.println("Połączono!");

    // Pobranie service
    BLERemoteService* pRemoteService =
        pClient->getService(serviceUUID);

    if (pRemoteService == nullptr) {
        Serial.println("Nie znaleziono service");
        pClient->disconnect();
        return false;
    }

    // ===== INT characteristic =====
    intCharacteristic =
        pRemoteService->getCharacteristic(intCharUUID);

    if (intCharacteristic == nullptr) {
        Serial.println("Nie znaleziono int characteristic");
        return false;
    }

    // ===== STRING characteristic =====
    stringCharacteristic =
        pRemoteService->getCharacteristic(stringCharUUID);

    if (stringCharacteristic == nullptr) {
        Serial.println("Nie znaleziono string characteristic");
        return false;
    }

    // ===== Rejestracja notify =====
    if (intCharacteristic->canNotify()) {
        intCharacteristic->registerForNotify(intNotifyCallback);
    }

    if (stringCharacteristic->canNotify()) {
        stringCharacteristic->registerForNotify(stringNotifyCallback);
    }

    Serial.println("Notify zarejestrowane");

    return true;
}

// ============================
// SETUP
// ============================
void setup() {

    Serial.begin(115200);

    BLEDevice::init("");

    connectToServer();
}

// ============================
// LOOP
// ============================
void loop() {

    // notify działają w callbackach

    delay(1000);
}
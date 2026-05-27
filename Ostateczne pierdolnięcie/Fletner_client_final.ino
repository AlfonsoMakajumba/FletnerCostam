#include <Arduino.h>
#include "BLEDevice.h"
#include "esp_pm.h"

#define CPU_FREQ 80 // in MHz
#define PWM_FREQ 25000 //in Hz // can be changed to 18kHz

// ===== UUID =====
static BLEUUID serviceUUID("43a934fe-4ff0-4476-9c20-eb6b8f76121b");
static BLEUUID kierunek_UUID("0a48ab5a-393d-4041-9702-a2a83cc6f4ee");
static BLEUUID velocity_UUID("dc8f48ec-69a1-45cd-ab1b-fc4f5a432458");

// ===== CONNECTION STATE =====
static boolean doConnect = false;
static boolean connected = false;
static boolean scanning = false;

static BLERemoteCharacteristic *pRemoteCharacteristickierunek;
static BLERemoteCharacteristic *pRemoteCharacteristicvelocity;
static BLEAdvertisedDevice *myDevice;

//current values from notifications
static String kierunek = "";
static int velocity = 0;
static int speedConstrained = 0;
static bool kierunekUpdated = false;
static bool velocityUpdated = false;
static bool needUpdate = false;

// ===== Timings for BLE connection =====
unsigned long lastScanTime = 0;
const int ScanDuration = 5; //in seconds
const int ScanInterval = 3000; //in ms, time between scans

// ===== NOTIFICATIONs CALLBACK =====
static void notifyCallback(BLERemoteCharacteristic *pChar, uint8_t *pData, size_t length, bool isNotify)
{
  if(pChar->getUUID().equals(velocity_UUID)) 
  {
    memcpy(&velocity, pData, sizeof(int));
    velocityUpdated = true;
    Serial.println(String(velocity));
  }
  else if(pChar->getUUID().equals(kierunek_UUID))
  {
    kierunek = String((char*)pData, length); //ascii random bullshit go
    kierunekUpdated = true;
    Serial.println(String(kierunek));
  }
}

// ===== CLIENT CALLBACK =====
class MyClientCallback : public BLEClientCallbacks
{
  void onConnect(BLEClient *pclient) {}
  void onDisconnect(BLEClient *pclient)
  {
    connected = false;
    Serial.println("onDisconnet event - reconnecting...");
    scanning = false;
    doConnect = false;
    lastScanTime = 0;
  }
};

// ===== CONNECT =====
bool connectToServer() 
{
  Serial.print("Connecting to ");
  Serial.println(myDevice->getAddress().toString().c_str());

  BLEClient *pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());

  if(!pClient->connect(myDevice))
  {
    Serial.println("Connection failed");
    return false;
  }

  Serial.println("Polaczono z serwerem");

// ===== service =====
  BLERemoteService *pRemoteService = pClient->getService(serviceUUID);
  if(!pRemoteService) 
  {
    Serial.println("BLE service not found");
    pClient->disconnect();
    return false;
  }

  // ===== KIERUNEK =====
  pRemoteCharacteristickierunek = pRemoteService->getCharacteristic(kierunek_UUID);
  if(!pRemoteCharacteristickierunek) 
  {
    Serial.println("Brak kierunku");
    pClient->disconnect();
    return false;
  }

  if (pRemoteCharacteristickierunek->canNotify()) 
  {
    pRemoteCharacteristickierunek->registerForNotify(notifyCallback);
  }

  // ===== VELOCITY =====
  pRemoteCharacteristicvelocity = pRemoteService->getCharacteristic(velocity_UUID);
  if(!pRemoteCharacteristicvelocity) 
  {
    Serial.println("Brak velocity");
    pClient->disconnect();
    return false;
  }

  if (pRemoteCharacteristicvelocity->canNotify()) 
  {
    pRemoteCharacteristicvelocity->registerForNotify(notifyCallback);
  }

  connected = true;
  return true;
}

// ===== SCAN =====
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) 
  {
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) 
    {
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      scanning = false;
    }
  }
};

void updateMotor()
{
  //kierunek
  if(kierunek == "prawo")
  {
    digitalWrite(13, HIGH);
    digitalWrite(12, LOW);
  }
  else if(kierunek == "lewo")
  {
    digitalWrite(12, HIGH);
    digitalWrite(13, LOW);
  }
  else //better safe than sorry
  {
    digitalWrite(13, LOW);
    digitalWrite(12, LOW);
  }
  //velocity
  speedConstrained = constrain(velocity, 0, 255); //eliminates errors if they happen
  analogWrite(11, speedConstrained);
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  Serial.println("Start BLE client...");

  setCpuFrequencyMhz(CPU_FREQ);
  esp_pm_config_esp32_t pmConfig = {
    .max_freq_mhz = CPU_FREQ,
    .min_freq_mhz = 10,
    .light_sleep_enable = true //can be changed if ISRs are broken
  };
  esp_pm_configure(&pmConfig);

  BLEDevice::init("");

  BLEScan *pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(false);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(30);

  esp_bt_sleep_enable();

  pinMode(13, OUTPUT);
  pinMode(12, OUTPUT);
  pinMode(11, OUTPUT);

  analogWriteFrequency(11, PWM_FREQ); //Freq of PWM for H bridge
}

// ===== LOOP =====
void loop() {

  if (doConnect) 
  {
    if (connectToServer()) 
    {
      Serial.println("Connection is fine");
    } 
    else 
    {
      Serial.println("Connection failed, retrying...");
      doConnect = false;
      scanning = false;
      lastScanTime = 0;
    }
    doConnect = false;
  }

  // ===== CONNECTION RETRY =====
  if(!connected && !scanning)
  {
    unsigned long time = millis();
    unsigned long elapsed = time - lastScanTime;
    if(elapsed >= ScanInterval)
    {
      Serial.println("Scanning for server");
      BLEScan *pBLEscan = BLEDevice::getScan();
      scanning = true;
      pBLEscan->start(ScanDuration, false);
      scanning = false;
      lastScanTime = time;
      if(!doConnect) Serial.println("Server not found, retrying");
    }
    else
    {
      unsigned long sleep = ScanInterval - elapsed;
      esp_sleep_enable_timer_wakeup(sleep * 1000); //in us
      esp_light_sleep_start();
      Serial.flush();
      return;
    }
  }

  if (!connected) 
  {
    delay(10);
    return;
  }

  //Updates only when needed
  needUpdate = false;
  if(kierunekUpdated)
  {
    kierunekUpdated = false;
    needUpdate = true;
  }
  if(velocityUpdated)
  {
    velocityUpdated = false;
    needUpdate = true;
  }
  if(needUpdate) updateMotor();

  Serial.flush();
}
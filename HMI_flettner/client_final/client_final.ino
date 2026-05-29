// HMI FLETTNER ROTOR - CLIENT
#include <Arduino.h>
#include "BLEDevice.h"
#include "esp_pm.h"

#define CPU_FREQ 80 // in MHz
#define PWM_FREQ 250 //in Hz // can be changed to 18kHz
#define STOP_TIME 3000 //is ms

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
static BLEAdvertisedDevice *myDevice = nullptr;
static BLEClient *pClient = nullptr;

//current values from notifications
static char kierunekBuf[16] = "";
static int velocity = 0;
static int speedConstrained = 0;
static bool kierunekUpdated = false;
static bool velocityUpdated = false;
static bool needUpdate = false;
static String currentDirection = "";
bool directionChanged = false;

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
    Serial.println(velocity);
  }
  else if(pChar->getUUID().equals(kierunek_UUID))
  {
    size_t copyLen = (length < sizeof(kierunekBuf) - 1) ? length : sizeof(kierunekBuf) - 1;
    memcpy(kierunekBuf, pData, copyLen);
    kierunekBuf[copyLen] = '\0';
    kierunekUpdated = true;
    Serial.println(kierunekBuf);
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
    //motor shutdown
    analogWrite(11, 0);
    digitalWrite(12, LOW);
    digitalWrite(13, LOW);
    currentDirection = "";

    kierunekBuf[0] = '\0';
    velocity = 0;
    kierunekUpdated = false;
    velocityUpdated = false;

    scanning = false;
    doConnect = false;
    lastScanTime = 0;
  }
};

static MyClientCallback myCallback;

// ===== CONNECT =====
bool connectToServer() 
{
  if (myDevice == nullptr) 
  {
    Serial.println("ERROR: myDevice is nullptr, cannot connect");
    return false;
  }

  if(pClient != nullptr)
  {
    pClient->disconnect();
    delete pClient;
    pClient = nullptr;
  }

  Serial.print("Connecting to ");
  Serial.println(myDevice->getAddress().toString().c_str());

  pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(&myCallback);

  if(!pClient->connect(myDevice))
  {
    Serial.println("Connection failed");
    delete pClient;
    pClient = nullptr;
    return false;
  }

  Serial.println("Polaczono z serwerem");

// ===== service =====
  BLERemoteService *pRemoteService = pClient->getService(serviceUUID);
  if(!pRemoteService) 
  {
    Serial.println("BLE service not found");
    pClient->disconnect();
    delete pClient;
    pClient = nullptr;
    return false;
  }

  // ===== KIERUNEK =====
  pRemoteCharacteristickierunek = pRemoteService->getCharacteristic(kierunek_UUID);
  if(!pRemoteCharacteristickierunek) 
  {
    Serial.println("Brak kierunku");
    pClient->disconnect();
    delete pClient;
    pClient = nullptr;
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
    delete pClient;
    pClient = nullptr;
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
      if(myDevice != nullptr) delete myDevice;
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      scanning = false;
    }
  }
};

void updateMotor()
{
  String kierunek = String(kierunekBuf);
  directionChanged = currentDirection != "" && kierunek != currentDirection;

  if(directionChanged)
  {
    analogWrite(11, 0);
    digitalWrite(13, HIGH);
    digitalWrite(12, HIGH);
    delay(STOP_TIME);
  }
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

  currentDirection = kierunek;

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

  //esp_bt_sleep_enable();

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
      //unsigned long sleep = ScanInterval - elapsed;
      Serial.flush();
      //esp_sleep_enable_timer_wakeup(sleep * 100); //in us //chuj wie czemu to nie dziala, ale to musi byc zakomentowane XD
      //esp_light_sleep_start();
      delay(10); //hopefully this works
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
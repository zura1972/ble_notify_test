/**
 * Ground Gear
 * Mbox BLE implementation
 */
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

#define GG_DEVICE_NAME "GroundGear"
#define LED_PIN 2

#define SERIAL_SPEED 115200

// Ground Gear BLE status
enum GGState {
  GG_STATE_DISCONNECTED,
  GG_STATE_ADVERTISING,
  GG_STATE_CONNECTING,
  GG_STATE_CONNECTED
};

BLEServer *pServer = nullptr;
BLEService *pService = nullptr;
BLECharacteristic *pCharacteristicTx = nullptr; // 送信用
BLECharacteristic *pCharacteristicRx = nullptr; // 受信用
GGState ggState = GG_STATE_DISCONNECTED;
uint32_t countAdvertizing = 0;
uint32_t countLoop = 0;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define UUID_SERVICE "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define UUID_CHARACTERISTIC_TX "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define UUID_CHARACTERISTIC_RX "eaaf5154-4693-4d68-b887-0ef9dfde70aa"

/**
 * BLEサーバ 接続コールバッククラス
 */
class GGServerCallbacks : public BLEServerCallbacks {
public:
  void onConnect(BLEServer *pServer) { ggState = GG_STATE_CONNECTING; }
  void onDisconnect(BLEServer *pServer) { ggState = GG_STATE_DISCONNECTED; }
};

/**
 * BLE特性 受信コールバッククラス
 */
class GGCharacteristicRxCallbacks : public BLECharacteristicCallbacks {
public:
  void onWrite(BLECharacteristic *pCharacteristic) override {
    auto value = pCharacteristic->getValue();
    Serial.print("get value = ");
    Serial.println(value.c_str());
    switch (value[0]) {
    case '0':
      digitalWrite(LED_PIN, LOW);
      break;
    case '1':
      digitalWrite(LED_PIN, HIGH);
      break;
    }
  }
};

// BLEサービスの作成
void initService() { pService = pServer->createService(UUID_SERVICE); }

// BLE特性(キャラクタリスティック)の作成
void initCharacteristics() {
  // 送信用BLE特性の作成と初期化
  pCharacteristicTx = pService->createCharacteristic(
      UUID_CHARACTERISTIC_TX, BLECharacteristic::PROPERTY_NOTIFY);
  pCharacteristicTx->addDescriptor(new BLE2902());

  // 受信用BLE特性の作成と初期化
  pCharacteristicRx = pService->createCharacteristic(
      UUID_CHARACTERISTIC_RX, BLECharacteristic::PROPERTY_WRITE);
  pCharacteristicRx->setCallbacks(new GGCharacteristicRxCallbacks());
}

// アドバタイズ初期化
void initAdvertising() {
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();

  pAdvertising->addServiceUUID(BLEUUID(UUID_SERVICE));
  pAdvertising->setScanResponse(false);

  // set value to 0x00 to not advertise this parameter
  pAdvertising->setMinPreferred(0x0);
}

// アドバタイズ開始
void startAdvertising() {
  BLEDevice::startAdvertising();
  delay(500);
}

/**
 * setupエントリ
 */
void setup() {
  Serial.begin(SERIAL_SPEED);

  // BLEデバイスの初期化
  BLEDevice::init(GG_DEVICE_NAME);

  // BLEサーバの作成
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new GGServerCallbacks());

  initService();
  initCharacteristics();

  // サービス開始
  pService->start();

  // アドバタイズの初期化と開始
  initAdvertising();
  startAdvertising();

  pinMode(LED_PIN, OUTPUT);
}

/**
 * loopエントリ
 */
void loop() {
  switch (ggState) {
  case GG_STATE_DISCONNECTED:
    Serial.println("disconnected");
    digitalWrite(LED_PIN, LOW);

    delay(1000); // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // restart advertising
    Serial.println("start advertising...");

    countAdvertizing = 0;
    ggState = GG_STATE_ADVERTISING;
    break;

  case GG_STATE_ADVERTISING:
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);

    Serial.print("trying to be pairing... ");
    Serial.println(countAdvertizing);

    countAdvertizing += 1;
    break;

  case GG_STATE_CONNECTING:
    digitalWrite(LED_PIN, HIGH);
    countLoop = 0;
    ggState = GG_STATE_CONNECTED;
    break;

  case GG_STATE_CONNECTED:
    pCharacteristicTx->setValue((uint8_t *)&countLoop, 4);
    pCharacteristicTx->notify();
    // pCharacteristicTx->indicate();
    delay(1000);
    /*
    delay(3); // bluetooth stack will go into congestion, if too many packets
              // are sent, in 6 hours test i was able to go as low as 3ms
    */

    countLoop += 1;
    Serial.print("countLoop=");
    Serial.println(countLoop);

    break;
  }
}

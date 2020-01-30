/*
    Video: https://www.youtube.com/watch?v=oCMOYS71NIU
    Based on Neil Kolban example for IDF:
   https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleNotify.cpp
    Ported to Arduino ESP32 by Evandro Copercini
    updated by chegewara

   Create a BLE server that, once we receive a connection, will send periodic
   notifications. The service advertises itself as:
   4fafc201-1fb5-459e-8fcc-c5c9c331914b And has a characteristic of:
   beb5483e-36e1-4688-b7f5-ea07361b26a8

   The design of creating the BLE server is:
   1. Create a BLE Server
   2. Create a BLE Service
   3. Create a BLE Characteristic on the Service
   4. Create a BLE Descriptor on the characteristic
   5. Start the service.
   6. Start advertising.

   A connect hander associated with the server starts a background task that
   performs notification every couple of seconds.
*/
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

#define LED_PIN 2
#define MAX_ADVERTISE_COUNT 1000

// Ground Gear BLE status
enum GGState {
  GG_STATE_DISCONNECTED,
  GG_STATE_ADVERTISING,
  GG_STATE_CONNECTING,
  GG_STATE_CONNECTED
};

BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;
bool deviceConnected = false;
GGState ggState = GG_STATE_DISCONNECTED;
uint32_t countAdvertizing = 0;
uint32_t value = 0;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a9"

class GroundGearCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
    ggState = GG_STATE_CONNECTING;
  };
  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    ggState = GG_STATE_DISCONNECTED;
  }
};

void setup() {
  Serial.begin(115200);

  // Create the BLE Device
  BLEDevice::init("GroundGear");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new GroundGearCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ |
                               BLECharacteristic::PROPERTY_WRITE |
                               BLECharacteristic::PROPERTY_NOTIFY |
                               BLECharacteristic::PROPERTY_INDICATE);

  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Create a BLE Descriptor
  pCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(BLEUUID(SERVICE_UUID));
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(
      0x0); // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  delay(500);

  Serial.println("Setup finished.");
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  switch (ggState) {
  case GG_STATE_DISCONNECTED:
    Serial.println("disconnectd");
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
    if (countAdvertizing > MAX_ADVERTISE_COUNT) {
      pServer->startAdvertising(); // restart advertising
    }
    delay(1000);
    break;

  case GG_STATE_CONNECTING:
    digitalWrite(LED_PIN, HIGH);
    ggState = GG_STATE_CONNECTED;
    break;

  case GG_STATE_CONNECTED:
    pCharacteristic->setValue((uint8_t *)&value, 4);
    pCharacteristic->notify();
    delay(3); // bluetooth stack will go into congestion, if too many packets
              // are sent, in 6 hours test i was able to go as low as 3ms

    value += 1;
    Serial.print("value=");
    Serial.println(value);

    break;
  }
}

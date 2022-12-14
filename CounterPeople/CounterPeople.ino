#include <esp_now.h>
#include <WiFi.h>
// Mac address of the slave
uint8_t broadcastAddress[] = {0x58, 0xBF, 0x25, 0x37, 0xF2, 0xC0};
// Structure example to send data
// Must match the receiver structure
typedef struct struct_message {
  int id;
  int count;
} struct_message;

// Create a struct_message called myData
struct_message myData;

// Create peer interface
esp_now_peer_info_t peerInfo;

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

//Heigh of door is 2m, valid human's height to be detected is >= 1m
const int trigPin1 = 25;
const int echoPin1 = 33;

const int trigPin2 = 27;
const int echoPin2 = 26;

#define SOUND_SPEED 0.0343
#define NOTHING     0
#define IN          1
#define READY_INC1  2
#define READY_INC2  3
#define OUT         4
#define READY_DEC1  5
#define READY_DEC2  6
#define AVG_DISTANCE  40

int state, count, last_count = -1;
long duration1, duration2;
float distanceCm1, distanceCm2;

void setup() {
  Serial.begin(115200); // Starts the serial communication
  pinMode(trigPin1, OUTPUT); // Sets the trigPin as an Output
  pinMode(trigPin2, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin1, INPUT); // Sets the echoPin as an Input
  pinMode(echoPin2, INPUT); // Sets the echoPin as an Input

  // Init Serial Monitor
  Serial.begin(115200);
 
   // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
  
  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
}

void fsm_detect() {
  switch (state) {
    case NOTHING:
      if (distanceCm1 <= AVG_DISTANCE && distanceCm2 > AVG_DISTANCE) {
        state = IN;
      }
      if (distanceCm1 > AVG_DISTANCE && distanceCm2 <= AVG_DISTANCE) {
        state = OUT;
      }
      break;

    case IN:
      if (distanceCm1 <= AVG_DISTANCE && distanceCm2 <= AVG_DISTANCE) {
        state = READY_INC1;
      }
      if (distanceCm1 > AVG_DISTANCE && distanceCm2 > AVG_DISTANCE) {
        state = NOTHING;
      }
      break;

    case READY_INC1:
      if (distanceCm1 > AVG_DISTANCE && distanceCm2 <= AVG_DISTANCE) {
        state = READY_INC2;
      }
      if (distanceCm1 <= AVG_DISTANCE && distanceCm2 > AVG_DISTANCE) {
        state = IN;
      }
      break;

    case READY_INC2:
      if (distanceCm1 > AVG_DISTANCE && distanceCm2 > AVG_DISTANCE) {
        count++;
        state = NOTHING;
      }
      if (distanceCm1 <= AVG_DISTANCE && distanceCm2 <= AVG_DISTANCE) {
        state = READY_INC1;
      }
      break;

    case OUT:
      if (distanceCm1 <= AVG_DISTANCE && distanceCm2 <= AVG_DISTANCE) {
        state = READY_DEC1;
      }
      if (distanceCm1 > AVG_DISTANCE && distanceCm2 > AVG_DISTANCE) {
        state = NOTHING;
      }
      break;

    case READY_DEC1:
      if (distanceCm1 <= AVG_DISTANCE && distanceCm2 > AVG_DISTANCE) {
        state = READY_DEC2;
      }
      if (distanceCm1 > AVG_DISTANCE && distanceCm2 <= AVG_DISTANCE) {
        state = OUT;
      }
      break;

    case READY_DEC2:
      if (distanceCm1 > AVG_DISTANCE & distanceCm2 > AVG_DISTANCE) {
        if (count > 0)
          count--;
        state = NOTHING;
      }
      if (distanceCm1 <= AVG_DISTANCE && distanceCm2 <= AVG_DISTANCE) {
        state = READY_DEC1;
      }
      break;
  }
}

void loop() {
  // Clears the trigPin
  digitalWrite(trigPin1, LOW);
  digitalWrite(trigPin2, LOW);

  delayMicroseconds(2);
  digitalWrite(trigPin1, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin1, LOW);
  
  duration1 = pulseIn(echoPin1, HIGH, 100000);
  distanceCm1 = duration1 * SOUND_SPEED/2;

  digitalWrite(trigPin2, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin2, LOW);

  duration2 = pulseIn(echoPin2, HIGH, 100000);
  distanceCm2 = duration2 * SOUND_SPEED/2;
  
  fsm_detect();
  
  Serial.print(distanceCm1);
  Serial.print(" ");
  Serial.print(distanceCm2);
  Serial.print(" ");
  Serial.print(count);
  Serial.print(" ");
  Serial.println(state);

  // Set values to send
  myData.id = 3;
  myData.count = count;

  // Send message via ESP-NOW

  esp_err_t result;
  if (count != last_count) {
    result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
    last_count = count;
  }
   
  if (result == ESP_OK) {
    Serial.println("Sent with success");
  }
  else {
    Serial.println("Error sending the data");
  }
  delay(10);
}
// MAC ID:   58:BF:25:37:F2:C0

#include <esp_now.h>
#include <WiFi.h>

// REPLACE WITH THE RECEIVER'S MAC Address
uint8_t broadcastAddress[] = {0x58, 0xBF, 0x25, 0x37, 0xF2, 0xC0};
int cambien = 36; //Cảm biến nối chân số 2 esp32
int giatri[10];

// Structure example to send data
// Must match the receiver structure
typedef struct struct_message {
    int id; // must be unique for each sender board
    int anhSang;
} struct_message;

// Create a struct_message called myDatas
struct_message myData;

// Create peer interface
esp_now_peer_info_t peerInfo;

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}
 
void setup() {
  // Init Serial Monitor
  Serial.begin(115200);
  pinMode(cambien, INPUT); //Cảm biến nhận tín hiệu

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

int count_high = 0;
int count_low = 0;
int i = 0;
void loop() {
  // Set values to send
  myData.id = 2;
  giatri[i++] = analogRead(cambien); //Đọc giá trị analog của cảm biến và gán vào biến giatri

  Serial.print("Gia tri cam bien: ");
  Serial.println(giatri[i-1]);
  
  if(i == 10){
    for(int j = 0; j < i; ++j){
      if(giatri[j] > 1800) count_high ++;
      else count_low ++;
    } 
    if(count_high >= count_low){
      myData.anhSang = 0;
    }else{
      myData.anhSang = 1;      
    }
    // Send message via ESP-NOW
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
  
    if (result == ESP_OK) {
      Serial.println("Sent with success");
    }
    else {
      Serial.println("Error sending the data");
    }
    Serial.print("myData.anhSang : ");
    Serial.println(myData.anhSang);
    count_high = 0;
    count_low = 0;
    i = 0;
  } 
  
  delay(1000);
}












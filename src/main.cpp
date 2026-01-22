#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "UsartProtocol.h"
#include "WirelessProtocol.h"

/*

主机mac地址(STA MAC)：14:33:5C:C1:4A:70

*/

// ==================== 前置声明 ====================
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int len);
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void processWp(uint8_t seq, uint8_t flag, const uint8_t *pld, uint16_t len);
void handlePairingRequest(const uint8_t *requester_mac);
void handlePairingResponse(const uint8_t *responder_mac);
void printMac(const uint8_t *mac);
void addDeviceToESPNow(const uint8_t *mac);
void sendDeviceListToSerial();
void sendPairingRequest();
void sendPairingResponse(const uint8_t *target_mac);


esp_now_peer_info_t peerInfo;   // esp-now实体信息

uint8_t send_buf[260];          // 发送缓冲区

uint8_t last_received_mac[6];

// 创建Address别名
typedef uint8_t Address[6];

// mac地址
Address master = {0x14, 0x33, 0x5C, 0xC1, 0x4A, 0x70};

// 动态设备管理
#define MAX_DEVICES 10
Address device_list[MAX_DEVICES];   // 从机设备列表
uint8_t device_count = 0;           // 当前设备数量

// 广播MAC地址（用于配对请求）
Address broadcast_mac = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// 串口通信和无线通信对象
UsartProtocol usart_parser;
WirelessProtocol wireless_parser;

// 函数声明
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int len){
    //  处理4字节配对请求
    if (len == 4 && data[0] == 0xAA && data[1] == 0x55) {
        // 验证校验和
        Serial.println("");
        uint8_t checksum = data[0] + data[1] + data[2];
        uint8_t expected_checksum = ~checksum;
        
        if (data[3] == expected_checksum) {
            // 校验通过，处理包类型
            uint8_t packet_type = data[2];
            
            if (packet_type == 0x00) {
                // 配对请求
                handlePairingRequest(mac_addr);
            } else if (packet_type == 0x01) {
                // 配对响应
                handlePairingResponse(mac_addr);
            }
            return;
        } 
    }

    else {
      // 保存当前数据包的MAC地址
      memcpy(last_received_mac, mac_addr, 6);

      for (int i = 0; i < len; i++){
        wireless_parser.feed(data[i]);
      }
    }
   
}
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status){

}



void processWp(uint8_t seq, uint8_t flag, const uint8_t *pld, uint16_t len){
  /*Serial.print("seq:");
  Serial.println(seq);
  Serial.print("flag:");
  Serial.println(flag);
  Serial.println("pld:");
  for (uint16_t i = 0; i< len; ++i) Serial.print(pld[i]);
  Serial.println("\n");*/
  uint8_t address = 0; 
  // 查找发送者位号
  for (int i = 0; i < device_count; i++) {
      if (memcmp(device_list[i], last_received_mac, 6) == 0) {
          address = i + 1;  // 1-based位号
          break;
      }
  }

  uint8_t l = usart_parser.pack(address, pld, len, send_buf, 250);
  Serial.write(send_buf, l);
}

// ==================== 处理请求函数 ====================
// 处理配对请求（从机端调用）
void handlePairingRequest(const uint8_t *requester_mac) {
    Serial.print("收到配对请求，来自: ");
    printMac(requester_mac);
    
    // 发送配对响应
    sendPairingResponse(requester_mac);
}

// 处理配对响应（主机端调用）
void handlePairingResponse(const uint8_t *responder_mac) {
    Serial.print("收到配对响应，来自: ");
    printMac(responder_mac);
    
    // 添加到设备列表
    addDeviceToESPNow(responder_mac);
}

// 辅助函数：打印MAC地址
void printMac(const uint8_t *mac) {
    for (int i = 0; i < 6; i++) {
        Serial.printf("%02X", mac[i]);
        if (i < 5) Serial.print(":");
    }
    Serial.println();
}

void addDeviceToESPNow(const uint8_t *mac) {

    // 1. 检查是否已存在
    for (int i = 0; i < device_count; i++) {
        if (memcmp(device_list[i], mac, 6) == 0) {
            Serial.printf("设备已存在");
        }
    }
    
    // 2. 检查列表是否已满
    if (device_count >= MAX_DEVICES) {
        Serial.println("设备列表已满");
    }
    
    // 3. 添加到本地列表
    memcpy(device_list[device_count], mac, 6);
    memcpy(peerInfo.peer_addr, device_list[device_count], 6);

    if (esp_now_add_peer(&peerInfo) != ESP_OK){
      Serial.println("Failed to add peer");
      return;
    }
    device_count++;
    
    Serial.print("添加一个mac到本地列表成功, 当前设备数：");
    Serial.println(device_count);

    // 4. 发送设备列表到串口（位号用0表示这是设备列表）
    sendDeviceListToSerial();
}

void sendDeviceListToSerial() {   
    // 准备数据：将所有设备的MAC地址打包
    uint8_t device_data[MAX_DEVICES * 6 + 1];  // 每个设备6字节 + 设备数量1字节
    uint8_t data_len = 0;
    
    // 第一个字节：设备数量
    device_data[data_len++] = device_count;
    
    // 后续字节：所有设备的MAC地址
    for (int i = 0; i < device_count; i++) {
        memcpy(&device_data[data_len], device_list[i], 6);
        data_len += 6;
    }
    
    // 使用位号0打包设备列表数据
    uint8_t l = usart_parser.pack(0, device_data, data_len, send_buf, 250);
    Serial.write(send_buf, l);
}

// ==================== 配对协议函数 ====================
void sendPairingRequest() {
    uint8_t pairing_packet[4];  // 4字节
    
    // 帧头
    pairing_packet[0] = 0xAA;   // 魔数1
    pairing_packet[1] = 0x55;   // 魔数2
    pairing_packet[2] = 0x00;   // 包类型：0x00=主机配对请求
    
    // 计算校验和（加法后取反）
    uint8_t checksum = pairing_packet[0] + pairing_packet[1] + pairing_packet[2];
    pairing_packet[3] = ~checksum;  // 取反校验
    
    // 发送广播
    esp_now_send(broadcast_mac, pairing_packet, sizeof(pairing_packet));

    Serial.println("发送配对请求");
}


// 发送配对响应（4字节）
void sendPairingResponse(const uint8_t *target_mac) {
    uint8_t response_packet[4];
    
    response_packet[0] = 0xAA;   // 魔数1
    response_packet[1] = 0x55;   // 魔数2
    response_packet[2] = 0x01;   // 包类型：0x01=配对响应
    
    // 计算校验和（加法后取反）
    uint8_t checksum = response_packet[0] + response_packet[1] + response_packet[2];
    response_packet[3] = ~checksum;
    
    // 发送给请求者
    esp_now_send(target_mac, response_packet, sizeof(response_packet));
    
    Serial.print("发送配对响应");
}

void setup() {
  // put your setup code here, to run once:
  // 设置串口
  Serial.begin(115200);

  // 设置WiFi模式
  WiFi.mode(WIFI_STA);

  // 初始化esp-now
  if (esp_now_init() != ESP_OK) {
  // Serial.println("Error initializing ESP-NOW");
    return;
  }

  // 配置对等（对等点）网络
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;

  // 添加主esp32网络节点
  memcpy(peerInfo.peer_addr, master, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add master");
    return;
  }
  device_count++;

  wireless_parser.setCallback(processWp);

  // 注册回调函数
  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);

}

void loop() {
  // put your main code here, to run repeatedly:
  static uint8_t i = 0;

  while (Serial.available()){
    usart_parser.feed(Serial.read());
    if(usart_parser.available()){
      uint8_t to = usart_parser.getTo();
      if(to <= 10 || to == 100){
        uint16_t len = wireless_parser.pack(i++, 0, usart_parser.getPayload(), usart_parser.getLen(), send_buf, 250);
        if(to && to != 100) esp_now_send(device_list[to - 1], send_buf, len);
        else if(to == 100) sendPairingRequest();
        else esp_now_send(master, send_buf, len);
      }
      usart_parser.reset();
    }
  }
}



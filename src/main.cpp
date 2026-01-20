#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

/*

主机mac地址(STA MAC)：14:33:5C:C1:4A:70

*/

// 全局变量
bool is_pairing = false;    // 配对模式标志


// 函数声明
void OnDataRecv();
void OnDataSent();


void setup() {
  // put your setup code here, to run once:
  // 设置串口
  Serial.begin(115200);

  // 设置WiFi模式
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // 设置WiFi频道（所有设备必须相同）
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  // 注册回调函数
  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);

}

void loop() {
  // put your main code here, to run repeatedly:

}



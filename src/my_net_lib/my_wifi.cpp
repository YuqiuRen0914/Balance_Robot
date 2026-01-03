#include "my_net_config.h"

void my_wifi_init()
{
    // 初始化WiFi
    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, PASSWORD);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    WiFi.setSleep(false);
    Serial.println("Connected to WiFi");
    // 打印ESP-01S的IP地址
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
}

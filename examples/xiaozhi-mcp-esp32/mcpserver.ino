/**
 * WebSocketMCPExample.ino
 * ESP32版WebSocket MCP客户端示例
 * 用于演示如何使用WebSocketMCP库连接到MCP服务器并进行双向通信
 */

#include <Arduino.h>
#include <WiFi.h>
#include "WebSocketMCP.h"

/********** 配置项 ***********/
// WiFi设置
const char* WIFI_SSID = "test";
const char* WIFI_PASS = "test";

// WebSocket MCP服务器地址
const char* MCP_ENDPOINT = "wss://api.xiaozhi.me/mcp/?token=e***************GciOiJFUzI1NiIsInR5cCI6IkpXVCJ9.eyJ1c2VySWQiOjEzODMzOCwiYWdlbnRJZCI6ODQyNDAsImVuZHBvaW50SWQiOiJhZ2VudF84NDI0MCIsInB1cnBvc2UiOiJtY3AtZW5kcG9pbnQiLCJpYXQiOjE3NDgyNzAyMDV9.87EvN4xXmWHgVmF120T0xNonOG4HRMMk5kH1byxlIP9p_ZuAE-S_j0goIzrsALin5uYjIpVMwCRE6VL9gWCezg";

// 调试信息
#define DEBUG_SERIAL Serial
#define DEBUG_BAUD_RATE 115200

// 状态LED引脚(可选)
#define STATUS_LED_PIN 2  // ESP32板载LED

// 添加LED控制引脚定义
#define LED_PIN 2  // 默认使用ESP32板载LED

/********** 全局变量 ***********/
WebSocketMCP mcpClient;

// 缓冲区管理
#define MAX_INPUT_LENGTH 1024
char inputBuffer[MAX_INPUT_LENGTH];
int inputBufferIndex = 0;
bool newCommandAvailable = false;

// 连接状态
bool wifiConnected = false;
bool mcpConnected = false;

/********** 函数声明 ***********/
void setupWifi();
void onMcpOutput(const String &message);
void onMcpError(const String &error);
void onMcpConnectionChange(bool connected);
void processSerialCommands();
void blinkLed(int times, int delayMs);
void registerMcpTools();

void setup() {
  // 初始化串口
  DEBUG_SERIAL.begin(DEBUG_BAUD_RATE);
  DEBUG_SERIAL.println("\n\n[ESP32 MCP客户端] 初始化...");
  
  // 初始化状态LED和控制LED
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // 连接WiFi
  setupWifi();
  
  // 初始化MCP客户端
  if (mcpClient.begin(MCP_ENDPOINT, onMcpConnectionChange)) {
    DEBUG_SERIAL.println("[ESP32 MCP客户端] 初始化成功，尝试连接到MCP服务器...");
  } else {
    DEBUG_SERIAL.println("[ESP32 MCP客户端] 初始化失败!");
  }
  
  // 显示帮助信息
  DEBUG_SERIAL.println("\n使用说明:");
  DEBUG_SERIAL.println("- 通过串口控制台输入命令并回车发送");
  DEBUG_SERIAL.println("- 从MCP服务器接收的消息将显示在串口控制台上");
  DEBUG_SERIAL.println("- 输入\"help\"查看可用命令");
  DEBUG_SERIAL.println();
}

void loop() {
  // 处理MCP客户端
  mcpClient.loop();
  
  // 处理来自串口的命令
  processSerialCommands();
  
  // 状态LED显示
  if (!wifiConnected) {
    // WiFi未连接: 快速闪烁
    blinkLed(1, 100);
  } else if (!mcpConnected) {
    // WiFi已连接但MCP未连接: 慢闪
    blinkLed(1, 500);
  } else {
    // 全部连接成功: LED亮起
    digitalWrite(STATUS_LED_PIN, HIGH);
  }
}

/**
 * 设置WiFi连接
 */
void setupWifi() {
  DEBUG_SERIAL.print("[WiFi] 连接到 ");
  DEBUG_SERIAL.println(WIFI_SSID);
  
  // 开始连接
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  // 等待连接
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    DEBUG_SERIAL.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    DEBUG_SERIAL.println();
    DEBUG_SERIAL.println("[WiFi] 连接成功!");
    DEBUG_SERIAL.print("[WiFi] IP地址: ");
    DEBUG_SERIAL.println(WiFi.localIP());
  } else {
    wifiConnected = false;
    DEBUG_SERIAL.println();
    DEBUG_SERIAL.println("[WiFi] 连接失败! 将继续尝试...");
  }
}

/**
 * MCP输出回调函数(stdout替代)
 */
void onMcpOutput(const String &message) {
  DEBUG_SERIAL.print("[MCP输出] ");
  DEBUG_SERIAL.println(message);
}

/**
 * MCP错误回调函数(stderr替代)
 */
void onMcpError(const String &error) {
  DEBUG_SERIAL.print("[MCP错误] ");
  DEBUG_SERIAL.println(error);
}

/**
 * 注册MCP工具
 * 在连接成功后注册工具
 */
void registerMcpTools() {
  DEBUG_SERIAL.println("[MCP] 注册工具...");
  
  // 注册LED控制工具
  mcpClient.registerTool(
    "led_blink",  // 工具名称
    "控制ESP32 LED状态", // 工具描述
    "{\"properties\":{\"state\":{\"title\":\"LED状态\",\"type\":\"string\",\"enum\":[\"on\",\"off\",\"blink\"]}},\"required\":[\"state\"],\"title\":\"ledControlArguments\",\"type\":\"object\"}",  // 输入schema
    [](const String& args) {
      // 解析参数
      DEBUG_SERIAL.println("[工具] LED控制: " + args);
      DynamicJsonDocument doc(256);
      DeserializationError error = deserializeJson(doc, args);
      
      if (error) {
        // 返回错误响应
        WebSocketMCP::ToolResponse response("{\"success\":false,\"error\":\"无效的参数格式\"}", true);
        return response;
      }
      
      String state = doc["state"].as<String>();
      DEBUG_SERIAL.println("[工具] LED控制: " + state);
      
      // 控制LED
      if (state == "on") {
        digitalWrite(LED_PIN, HIGH);
      } else if (state == "off") {
        digitalWrite(LED_PIN, LOW);
      } else if (state == "blink") {
        // 这里可以触发闪烁模式
        // 为简单起见，我们只是切换几次LED状态
        for (int i = 0; i < 5; i++) {
          digitalWrite(LED_PIN, HIGH);
          delay(200);
          digitalWrite(LED_PIN, LOW);
          delay(200);
        }
      }
      
      // 返回成功响应
      String resultJson = "{\"success\":true,\"state\":\"" + state + "\"}";
      return WebSocketMCP::ToolResponse(resultJson);
    }
  );
  DEBUG_SERIAL.println("[MCP] LED控制工具已注册");
  
  // 注册系统信息工具
  mcpClient.registerTool(
    "system-info",
    "获取ESP32系统信息",
    "{\"properties\":{},\"title\":\"systemInfoArguments\",\"type\":\"object\"}",
    [](const String& args) {
      DEBUG_SERIAL.println("[获取ESP32系统信息: " + args);
      // 收集系统信息
      String chipModel = ESP.getChipModel();
      uint32_t chipId = ESP.getEfuseMac() & 0xFFFFFFFF;
      uint32_t flashSize = ESP.getFlashChipSize() / 1024;
      uint32_t freeHeap = ESP.getFreeHeap() / 1024;
      
      // 构造JSON响应
      String resultJson = "{\"success\":true,\"model\":\"" + chipModel + "\",\"chipId\":\"" + String(chipId, HEX) + 
                         "\",\"flashSize\":" + String(flashSize) + ",\"freeHeap\":" + String(freeHeap) + 
                         ",\"wifiStatus\":\"" + (WiFi.status() == WL_CONNECTED ? "connected" : "disconnected") + 
                         "\",\"ipAddress\":\"" + WiFi.localIP().toString() + "\"}";
      
      return WebSocketMCP::ToolResponse(resultJson);
    }
  );
  DEBUG_SERIAL.println("[MCP] 系统信息工具已注册");
  
  // 注册计算器工具 (简单示例)
  mcpClient.registerTool(
    "calculator",
    "简单计算器",
    "{\"properties\":{\"expression\":{\"title\":\"表达式\",\"type\":\"string\"}},\"required\":[\"expression\"],\"title\":\"calculatorArguments\",\"type\":\"object\"}",
    [](const String& args) {
       DEBUG_SERIAL.println("[简单计算器: " + args);
      DynamicJsonDocument doc(256);
      deserializeJson(doc, args);
      
      String expr = doc["expression"].as<String>();
      DEBUG_SERIAL.println("[工具] 计算器: " + expr);
      
      // 这里只是演示，实际应用中需要实现表达式计算
      // 我们只处理简单的加减法
      int result = 0;
      if (expr.indexOf("+") > 0) {
        int plusPos = expr.indexOf("+");
        int a = expr.substring(0, plusPos).toInt();
        int b = expr.substring(plusPos + 1).toInt();
        result = a + b;
      } else if (expr.indexOf("-") > 0) {
        int minusPos = expr.indexOf("-");
        int a = expr.substring(0, minusPos).toInt();
        int b = expr.substring(minusPos + 1).toInt();
        result = a - b;
      }
      
      String resultJson = "{\"success\":true,\"expression\":\"" + expr + "\",\"result\":" + String(result) + "}";
      return WebSocketMCP::ToolResponse(resultJson);
    }
  );
  DEBUG_SERIAL.println("[MCP] 计算器工具已注册");
  
  DEBUG_SERIAL.println("[MCP] 工具注册完成，共" + String(mcpClient.getToolCount()) + "个工具");
}

/**
 * MCP连接状态变化回调函数
 */
void onMcpConnectionChange(bool connected) {
  mcpConnected = connected;
  if (connected) {
    DEBUG_SERIAL.println("[MCP] 已连接到MCP服务器");
    // 连接成功后注册工具
    registerMcpTools();
  } else {
    DEBUG_SERIAL.println("[MCP] 与MCP服务器断开连接");
  }
}

/**
 * 处理来自串口的命令
 */
void processSerialCommands() {
  // 检查是否有串口数据
  while (DEBUG_SERIAL.available() > 0) {
    char inChar = (char)DEBUG_SERIAL.read();
    
    // 处理回车或换行
    if (inChar == '\n' || inChar == '\r') {
      if (inputBufferIndex > 0) {
        // 添加字符串结束符
        inputBuffer[inputBufferIndex] = '\0';
        
        // 处理命令
        String command = String(inputBuffer);
        command.trim();
        
        if (command.length() > 0) {
          if (command == "help") {
            printHelp();
          } else if (command == "status") {
            printStatus();
          } else if (command == "reconnect") {
            DEBUG_SERIAL.println("正在重新连接...");
            mcpClient.disconnect();
          } else if (command == "tools") {
            // 显示已注册工具
            DEBUG_SERIAL.println("已注册工具数量: " + String(mcpClient.getToolCount()));
          } else {
            // 将命令发送到MCP服务器(stdin替代)
            if (mcpClient.isConnected()) {
              mcpClient.sendMessage(command);
              DEBUG_SERIAL.println("[发送] " + command);
            } else {
              DEBUG_SERIAL.println("未连接到MCP服务器，无法发送命令");
            }
          }
        }
        
        // 重置缓冲区
        inputBufferIndex = 0;
      }
    } 
    // 处理退格键
    else if (inChar == '\b' || inChar == 127) {
      if (inputBufferIndex > 0) {
        inputBufferIndex--;
        DEBUG_SERIAL.print("\b \b"); // 退格、空格、再退格
      }
    }
    // 处理普通字符
    else if (inputBufferIndex < MAX_INPUT_LENGTH - 1) {
      inputBuffer[inputBufferIndex++] = inChar;
      DEBUG_SERIAL.print(inChar); // Echo
    }
  }
}

/**
 * 打印帮助信息
 */
void printHelp() {
  DEBUG_SERIAL.println("可用命令:");
  DEBUG_SERIAL.println("  help     - 显示此帮助信息");
  DEBUG_SERIAL.println("  status   - 显示当前连接状态");
  DEBUG_SERIAL.println("  reconnect - 重新连接到MCP服务器");
  DEBUG_SERIAL.println("  tools    - 查看已注册工具");
  DEBUG_SERIAL.println("  其他任何文本将直接发送到MCP服务器");
}

/**
 * 打印当前状态
 */
void printStatus() {
  DEBUG_SERIAL.println("当前状态:");
  DEBUG_SERIAL.print("  WiFi: ");
  DEBUG_SERIAL.println(wifiConnected ? "已连接" : "未连接");
  if (wifiConnected) {
    DEBUG_SERIAL.print("  IP地址: ");
    DEBUG_SERIAL.println(WiFi.localIP());
    DEBUG_SERIAL.print("  信号强度: ");
    DEBUG_SERIAL.println(WiFi.RSSI());
  }
  DEBUG_SERIAL.print("  MCP服务器: ");
  DEBUG_SERIAL.println(mcpConnected ? "已连接" : "未连接");
}

/**
 * LED闪烁函数
 */
void blinkLed(int times, int delayMs) {
  static int blinkCount = 0;
  static unsigned long lastBlinkTime = 0;
  static bool ledState = false;
  static int lastTimes = 0;

  if (times == 0) {
    digitalWrite(STATUS_LED_PIN, LOW);
    blinkCount = 0;
    lastTimes = 0;
    return;
  }
  if (lastTimes != times) {
    blinkCount = 0;
    lastTimes = times;
    ledState = false;
    lastBlinkTime = millis();
  }
  unsigned long now = millis();
  if (blinkCount < times * 2) {
    if (now - lastBlinkTime > delayMs) {
      lastBlinkTime = now;
      ledState = !ledState;
      digitalWrite(STATUS_LED_PIN, ledState ? HIGH : LOW);
      blinkCount++;
    }
  } else {
    digitalWrite(STATUS_LED_PIN, LOW);
    blinkCount = 0;
    lastTimes = 0;
  }
}
/**
 * WebSocketMCP.h
 * ESP32版WebSocket MCP客户端库头文件
 * 用于连接到MCP服务器并进行双向通信
 */

#ifndef WEBSOCKET_MCP_H
#define WEBSOCKET_MCP_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>  // 需要添加这个库来解析JSON
#include <vector>
#include <functional>

/**
 * WebSocketMCP类
 * 封装了与MCP服务器的WebSocket连接及其通信
 */
class WebSocketMCP {
public:
  // 定义工具响应内容结构
  //Sending to WebSocket: {"jsonrpc":"2.0","id":48,"result":{"content":[{"type":"text","text":"{\n  \"success\": true,\n  \"result\": 2\n}"}],"isError":false}}
  struct ToolContentItem {
    String type;  // 内容类型，如"text"
    String text;  // 文本内容
  };
  
  // 定义工具响应结构
  struct ToolResponse {
    std::vector<ToolContentItem> content;  // 响应内容数组
    bool isError;                          // 是否为错误响应
    
    // 构造函数：从单个文本内容创建响应
    ToolResponse(const String& textContent, bool error = false) {
      ToolContentItem item;
      item.type = "text";
      
      // 尝试检测是否为JSON并格式化（通过查找开头的 { 和结尾的 }）
      String trimmedText = textContent;
      trimmedText.trim();
      
      if (trimmedText.startsWith("{") && trimmedText.endsWith("}")) {
        // 使用WebSocketMCP的formatJsonString方法格式化
        // 由于这是一个静态方法内，我们需要创建一个临时实例来调用非静态方法
        WebSocketMCP* instance = WebSocketMCP::instance;
        if (instance) {
          item.text = instance->formatJsonString(textContent);
        } else {
          item.text = textContent;
        }
      } else {
        item.text = textContent;
      }
      
      content.push_back(item);
      isError = error;
    }

    // 新增构造函数：用于处理 bool 和 String 类型的参数
    ToolResponse(bool error, const String& message) {
      ToolContentItem item;
      item.type = "text";
      item.text = message;
      content.push_back(item);
      isError = error;
    }
    
    // 默认构造函数
    ToolResponse() : isError(false) {}
    
    // 从JSON对象创建响应（便捷方法）
    static ToolResponse fromJson(const JsonObject& json, bool error = false) {
      String jsonStr;
      serializeJson(json, jsonStr);
      return ToolResponse(jsonStr, error);
    }
  };

  // 用于参数处理的辅助类
  class ToolParams {
  public:
    ToolParams(const String& json) {
      DeserializationError error = deserializeJson(doc, json);
      valid = !error;
    }

    // NEW: Static factory method from JsonVariantConst
    // This creates a new ToolParams instance by serializing the variant and re-parsing it.
    // Useful for handling items from JsonArray or nested JsonObjects reliably.
    static ToolParams fromVariant(const JsonVariantConst& variant) {
      String variantJson;
      serializeJson(variant, variantJson);
      return ToolParams(variantJson);
    }
    
    template<typename T>
    T get(const String& key, T defaultValue) const {
      if (!valid || !doc.is<JsonObject>() || !doc.as<JsonObjectConst>().containsKey(key)) {
        return defaultValue;
      }
      return doc.as<JsonObjectConst>()[key].as<T>();
    }
    
    JsonVariantConst getJsonValue(const String& key) const {
      if (!valid || !doc.is<JsonObject>() || !doc.as<JsonObjectConst>().containsKey(key)) {
        return JsonVariantConst();
      }
      return doc.as<JsonObjectConst>()[key];
    }
    
    JsonArrayConst getJsonArray(const String& key) const {
      if (!valid || !doc.is<JsonObject>() || !doc.as<JsonObjectConst>().containsKey(key) || !doc.as<JsonObjectConst>()[key].is<JsonArray>()) {
        return JsonArrayConst();
      }
      return doc.as<JsonObjectConst>()[key].as<JsonArrayConst>();
    }
    
    bool isArray(const String& key) const {
      if (!valid || !doc.is<JsonObject>() || !doc.as<JsonObjectConst>().containsKey(key)) {
        return false;
      }
      return doc.as<JsonObjectConst>()[key].is<JsonArray>();
    }
    
    size_t getArraySize(const String& key) const {
      if (!valid || !doc.is<JsonObject>() || !doc.as<JsonObjectConst>().containsKey(key) || !doc.as<JsonObjectConst>()[key].is<JsonArray>()) {
        return 0;
      }
      return doc.as<JsonObjectConst>()[key].as<JsonArrayConst>().size();
    }

    // NEW: Method to check if the root of this ToolParams' document is a JSON object
    bool isJsonObject() const {
      return valid && doc.is<JsonObject>();
    }

    // NEW: Method to check if the root of this ToolParams' document is a JSON array
    bool isJsonArray() const {
      return valid && doc.is<JsonArray>();
    }

    // NEW: Method to get the root as a JsonObjectConst if it is one
    JsonObjectConst getAsJsonObject() const {
      if (isJsonObject()) {
        return doc.as<JsonObjectConst>();
      }
      return JsonObjectConst(); // Returns a null JsonObjectConst
    }

    // NEW: Method to get the root as a JsonArrayConst if it is one
    JsonArrayConst getAsJsonArray() const {
      if (isJsonArray()) {
        return doc.as<JsonArrayConst>();
      }
      return JsonArrayConst(); // Returns a null JsonArrayConst
    }
    
    bool contains(const String& key) const {
      return valid && doc.is<JsonObject>() && doc.as<JsonObjectConst>().containsKey(key);
    }
    
    String getDebugJson() const {
      String result;
      if (valid) {
        serializeJson(doc, result);
      } else {
        result = "{\"error\":\"Invalid JSON document in ToolParams\"}";
      }
      return result;
    }
    
    bool isValid() const {
      return valid;
    }
    
  private:
    DynamicJsonDocument doc{2048}; // Adjust size as needed
    bool valid = false;
  };

  // 重新定义工具回调函数类型 - 接收JSON字符串参数，返回ToolResponse结构
  typedef std::function<ToolResponse(const String&)> ToolCallback; // 更改为接收 ToolParams&

  // 回调类型定义
  // 输出回调：void(const String&)
  typedef void (*OutputCallback)(const String&);
  // 错误回调：void(const String&)
  typedef void (*ErrorCallback)(const String&);
  // 连接状态回调：void(bool)
  typedef void (*ConnectionCallback)(bool);

  WebSocketMCP();

  /**
   * 初始化WebSocket连接
   * @param mcpEndpoint WebSocket服务器地址(ws://host:port/path)
   * @param outputCb 输出数据回调函数(相当于stdout)
   * @param errorCb 错误消息回调函数(相当于stderr)
   * @param connCb 连接状态变化回调函数
   * @return 初始化是否成功
   * 
   * 注意：连接会使用以下超时设置：
   * - PING_INTERVAL: 心跳ping间隔，默认为45秒
   * - DISCONNECT_TIMEOUT: 断开连接超时，默认为60秒
   * - INITIAL_BACKOFF: 初始重连等待时间，默认为1秒
   * - MAX_BACKOFF: 最大重连等待时间，默认为60秒
   */
  bool begin(const char *mcpEndpoint, ConnectionCallback connCb = nullptr);

  /**
   * 发送数据到WebSocket服务器(相当于stdin)
   * @param message 要发送的消息
   * @return 发送是否成功
   */
  bool sendMessage(const String &message);

  /**
   * 处理WebSocket事件和保持连接
   * 需要在主循环中频繁调用
   */
  void loop();

  /**
   * 是否已连接到服务器
   * @return 连接状态
   */
  bool isConnected();

  /**
   * 断开连接
   */
  void disconnect();

  /**
   * 设置日志级别和回调函数
   * @param level 日志级别
   * @param logCb 日志回调函数
   */

  // 工具注册和管理方法
  bool registerTool(const String &name, const String &description, const String &inputSchema, ToolCallback callback);
  // 简化工具注册API
  bool registerSimpleTool(const String &name, const String &description, 
                         const String &paramName, const String &paramDesc, 
                         const String &paramType, ToolCallback callback);
  
  bool unregisterTool(const String &name);
  size_t getToolCount();
  void clearTools();

private:
  WebSocketsClient webSocket;
  ConnectionCallback connectionCallback;

  bool connected;
  unsigned long lastReconnectAttempt;

  // 重连设置
  static const int INITIAL_BACKOFF = 1000; // 初始等待时间(毫秒)
  static const int MAX_BACKOFF = 60000;    // 最大等待时间(毫秒)
  static const int PING_INTERVAL = 10000;  // ping发送间隔(毫秒)
  static const int DISCONNECT_TIMEOUT = 60000; // 断开连接超时(毫秒)
  int currentBackoff;
  int reconnectAttempt;

  // WebSocket事件处理函数
  static void webSocketEvent(WStype_t type, uint8_t *payload, size_t length);
  static WebSocketMCP *instance;

  // 重连处理
  void handleReconnect();
  void resetReconnectParams();

  // 新增成员
  unsigned long lastPingTime = 0;
  void handleJsonRpcMessage(const String &message);

  // 工具结构定义
  struct Tool {
    String name;           // 工具名称
    String description;    // 工具描述 
    String inputSchema;    // 工具输入schema(JSON格式)
    ToolCallback callback; // 工具调用回调函数
  };

  // 工具列表
  std::vector<Tool> _tools;

  // 辅助方法
  String escapeJsonString(const String &input);
  
  // 格式化JSON字符串，每个键值对占一行
  String formatJsonString(const String &jsonStr);
};

#endif // WEBSOCKET_MCP_H

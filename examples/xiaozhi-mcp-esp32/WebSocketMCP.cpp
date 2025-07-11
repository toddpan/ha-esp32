/**
 * WebSocketMCP.cpp
 * ESP32版WebSocket MCP客户端库实现
 * 用于连接到MCP服务器并进行双向通信
 */

#include "WebSocketMCP.h"

// 静态实例指针初始化
WebSocketMCP* WebSocketMCP::instance = nullptr;

// 静态常量定义
const int WebSocketMCP::INITIAL_BACKOFF;
const int WebSocketMCP::MAX_BACKOFF;
const int WebSocketMCP::PING_INTERVAL;
const int WebSocketMCP::DISCONNECT_TIMEOUT;

WebSocketMCP::WebSocketMCP() : connected(false), lastReconnectAttempt(0), 
                              currentBackoff(INITIAL_BACKOFF), reconnectAttempt(0) {
  // 设置静态实例指针
  instance = this;
  connectionCallback = nullptr;
}

bool WebSocketMCP::begin(const char *mcpEndpoint,  ConnectionCallback connCb) {
  // 保存回调函数
  connectionCallback = connCb;
  
  // 解析WebSocket URL
  String url = String(mcpEndpoint);
  int protocolPos = url.indexOf("://");
  String protocol = url.substring(0, protocolPos);
  
  String remaining = url.substring(protocolPos + 3);
  int pathPos = remaining.indexOf('/');
  
  String host;
  String path = "/";
  int port = 80;
  
  if (pathPos >= 0) {
    host = remaining.substring(0, pathPos);
    path = remaining.substring(pathPos);
  } else {
    host = remaining;
  }
  
  // 检查端口
  int portPos = host.indexOf(':');
  if (portPos >= 0) {
    port = host.substring(portPos + 1).toInt();
    host = host.substring(0, portPos);
  } else {
    // 根据协议设置默认端口
    if (protocol == "ws") {
      port = 80;
    } else if (protocol == "wss") {
      port = 443;
    }
  }
  
  // 配置WebSocket客户端
  if (protocol == "wss") {
    webSocket.beginSSL(host.c_str(), port, path.c_str());
  } else {
    webSocket.begin(host.c_str(), port, path.c_str());
  }
  
  // 设置断开连接超时为60秒
  // webSocket.setReconnectInterval(DISCONNECT_TIMEOUT);
  webSocket.enableHeartbeat(PING_INTERVAL, PING_INTERVAL, DISCONNECT_TIMEOUT);
  
  // 注册事件回调
  webSocket.onEvent(webSocketEvent);
  
  Serial.println("[WebSocketMCP] 正在连接WebSocket服务器: " + url);
  return true;
}

void WebSocketMCP::webSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
  // 确保实例存在
  if (!instance) {
    return;
  }
  
  switch (type) {
    case WStype_DISCONNECTED:
      if (instance->connected) {
        instance->connected = false;
        Serial.println("[WebSocketMCP] WebSocket连接已断开");
        if (instance->connectionCallback) {
          instance->connectionCallback(false);
        }
      }
      break;
      
    case WStype_CONNECTED:
      {
        instance->connected = true;
        instance->resetReconnectParams();
        Serial.println("[WebSocketMCP] WebSocket已连接");
        if (instance->connectionCallback) {
          instance->connectionCallback(true);
        }
      }
      break;
      
    case WStype_TEXT:
      {
        // 接收到WebSocket消息，处理JSON-RPC请求
        String message = String((char *)payload);
        instance->handleJsonRpcMessage(message);
      }
      break;
      
    case WStype_BIN:
      Serial.println("[WebSocketMCP] 收到二进制数据，长度: " + String(length));
      break;
      
    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
      break;
  }
}

bool WebSocketMCP::sendMessage(const String &message) {
  if (!connected) {
    Serial.println("[WebSocketMCP] 未连接到WebSocket服务器，无法发送消息");
    return false;
  }
  // 发送文本消息到WebSocket服务器(相当于stdin)
  Serial.println("[WebSocketMCP] 发送消息: " + message);
  String msg = message;
  webSocket.sendTXT(msg);
  return true;
}

void WebSocketMCP::loop() {
  // 处理WebSocket连接
  webSocket.loop();
  
  // 检查是否需要重连
  if (!connected) {
    handleReconnect();
  }
  
  // 处理可能的ping超时
  if (connected && lastPingTime > 0) {
    unsigned long now = millis();
    // 如果超过2分钟没收到ping，可能连接已经断开
    if (now - lastPingTime > 120000) {
      Serial.println("[WebSocketMCP] Ping超时，重置连接");
      disconnect();
    }
  }
}

bool WebSocketMCP::isConnected() {
  return connected;
}

void WebSocketMCP::disconnect() {
  if (connected) {
    webSocket.disconnect();
    connected = false;
    lastPingTime = 0;
  }
}

void WebSocketMCP::handleReconnect() {
  // WebSocket库已经有自动重连功能，这里主要是处理重连状态的日志和通知
  unsigned long now = millis();
  if (!connected && (now - lastReconnectAttempt > currentBackoff || lastReconnectAttempt == 0)) {
    reconnectAttempt++;
    lastReconnectAttempt = now;
    
    // 计算下一次重连的等待时间(指数退避)
    currentBackoff = min(currentBackoff * 2, MAX_BACKOFF);
    
    Serial.println("[WebSocketMCP] 正在尝试重新连接(尝试次数: " + String(reconnectAttempt) + 
        ", 下次等待时间: " + String(currentBackoff / 1000.0, 2) + "秒)");
  }
}

void WebSocketMCP::resetReconnectParams() {
  reconnectAttempt = 0;
  currentBackoff = INITIAL_BACKOFF;
  lastReconnectAttempt = 0;
}

// 新增处理JSON-RPC消息的方法
void WebSocketMCP::handleJsonRpcMessage(const String &message) {
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, message);
  
  if (error) {
    Serial.println("[WebSocketMCP] 解析JSON失败: " + String(error.c_str()));
    return;
  }
  
  // 检查是否是ping请求
  if (doc.containsKey("method") && doc["method"] == "ping") {
    // 记录最后一次ping时间
    lastPingTime = millis();
    
    // 构造pong响应 - 使用原始id进行回应，不做修改
    String id = doc["id"].as<String>();
    Serial.println("[WebSocketMCP] 收到ping请求: " + id);

    String response = "{\"jsonrpc\":\"2.0\",\"id\":" + id + ",\"result\":{}}";
    sendMessage(response);

    Serial.println("[WebSocketMCP] 响应ping请求: " + id);
  }
  // 处理初始化请求
  else if (doc.containsKey("method") && doc["method"] == "initialize") {
    String id = doc["id"].as<String>();
    
    String serverName = "ESP-HA"; 

    // 发送初始化响应
    String response = "{\"jsonrpc\":\"2.0\",\"id\":" + id + 
      ",\"result\":{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{\"experimental\":{},\"prompts\":{\"listChanged\":false},\"resources\":{\"subscribe\":false,\"listChanged\":false},\"tools\":{\"listChanged\":false}},\"serverInfo\":{\"name\":\"" + serverName + "\",\"version\":\"1.0.0\"}}}";
    
    sendMessage(response);
    Serial.println("[WebSocketMCP] 响应initialize请求");
    
    // 发送initialized通知
    sendMessage("{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}");
  }
  // 处理tools/list请求
  else if (doc.containsKey("method") && doc["method"] == "tools/list") {
    String id = doc["id"].as<String>();
    
    // 这里可以根据实际情况定制工具列表
    String response = "{\"jsonrpc\":\"2.0\",\"id\":" + id + 
      ",\"result\":{\"tools\":[";
    
    // 从已注册工具列表生成工具信息
    if (_tools.size() > 0) {
      for (size_t i = 0; i < _tools.size(); i++) {
        if (i > 0) {
          response += ",";
        }
        response += "{\"name\":\"" + _tools[i].name + "\",";
        response += "\"description\":\"" + _tools[i].description + "\",";
        response += "\"inputSchema\":" + _tools[i].inputSchema + "}";
      }
    }
    
    response += "]}}";
    
    sendMessage(response);
    Serial.println("[WebSocketMCP] 响应tools/list请求，共" + String(_tools.size()) + "个工具");
  }
  // 处理tools/call请求
  else if (doc.containsKey("method") && doc["method"] == "tools/call") {
    int id = doc["id"].as<int>();
    String toolName = doc["params"]["name"].as<String>();
    JsonObject arguments = doc["params"]["arguments"].as<JsonObject>();
    
    Serial.println("[WebSocketMCP] 收到工具调用请求: " + toolName);
    
    // 查找工具
    bool toolFound = false;
    ToolResponse toolResponse;
    
    for (size_t i = 0; i < _tools.size(); i++) {
      if (_tools[i].name == toolName) {
        toolFound = true;
        // 调用工具回调，传入参数并获取结果
        if (_tools[i].callback) {
          String argumentsJson;
          serializeJson(arguments, argumentsJson);
          
          // 调用回调并获取结构化结果
          toolResponse = _tools[i].callback(argumentsJson);
        } else {
          toolResponse = ToolResponse("{\"error\":\"Tool callback not registered\"}", true);
        }
        break;
      }
    }
    
    if (!toolFound) {
      toolResponse = ToolResponse("{\"error\":\"Tool not found: " + toolName + "\"}", true);
    }
    
    // 构造响应
    DynamicJsonDocument responseDoc(2048);
    responseDoc["jsonrpc"] = "2.0";
    responseDoc["id"] = id;
    
    JsonObject result = responseDoc.createNestedObject("result");
    
    JsonArray content = result.createNestedArray("content");
    for (const auto& item : toolResponse.content) {
      JsonObject contentItem = content.createNestedObject();
      contentItem["type"] = item.type;
      contentItem["text"] = item.text;
    }

    result["isError"] = toolResponse.isError;
    
    String response;
    serializeJson(responseDoc, response);
    
    sendMessage(response);
    Serial.println("[WebSocketMCP] 工具调用完成: " + toolName + (toolResponse.isError ? " (出错)" : ""));
  }
}

// 转义JSON字符串中的特殊字符
String WebSocketMCP::escapeJsonString(const String &input) {
  String result = "";
  for (size_t i = 0; i < input.length(); i++) {
    char c = input[i];
    if (c == '\"' || c == '\\' || c == '/' || 
        c == '\b' || c == '\f' || c == '\n' || 
        c == '\r' || c == '\t') {
      if (c == '\"') result += "\\\"";
      else if (c == '\\') result += "\\\\";
      else if (c == '/') result += "\\/";
      else if (c == '\b') result += "\\b";
      else if (c == '\f') result += "\\f";
      else if (c == '\n') result += "\\n";
      else if (c == '\r') result += "\\r";
      else if (c == '\t') result += "\\t";
    } else {
      result += c;
    }
  }
  return result;
}

// 添加工具注册方法 - 带回调函数版
bool WebSocketMCP::registerTool(const String &name, const String &description, 
                              const String &inputSchema, ToolCallback callback) {
  // 检查工具是否已存在
  for (size_t i = 0; i < _tools.size(); i++) {
    if (_tools[i].name == name) {
      // 如果工具存在，可以选择更新回调
      _tools[i].callback = callback;
      Serial.println("[WebSocketMCP] 更新工具回调: " + name);
      return true;
    }
  }
  
  // 创建新工具并添加到列表
  Tool newTool;
  newTool.name = name;
  newTool.description = description;
  newTool.inputSchema = inputSchema;
  newTool.callback = callback;
  
  _tools.push_back(newTool);
  Serial.println("[WebSocketMCP] 成功注册工具: " + name);
  return true;
}

// 添加简化的工具注册方法
bool WebSocketMCP::registerSimpleTool(const String &name, const String &description, 
                                    const String &paramName, const String &paramDesc, 
                                    const String &paramType, ToolCallback callback) {
  // 构建简单的inputSchema
  String inputSchema = "{\"type\":\"object\",\"properties\":{\"" + 
                      paramName + "\":{\"type\":\"" + paramType + 
                      "\",\"description\":\"" + paramDesc + 
                      "\"}},\"required\":[\"" + paramName + "\"]}";
                      
  return registerTool(name, description, inputSchema, callback);
}

// 卸载工具
bool WebSocketMCP::unregisterTool(const String &name) {
  for (size_t i = 0; i < _tools.size(); i++) {
    if (_tools[i].name == name) {
      _tools.erase(_tools.begin() + i);
      Serial.println("[WebSocketMCP] 已卸载工具: " + name);
      return true;
    }
  }
  Serial.println("[WebSocketMCP] 工具 " + name + " 不存在，无法卸载");
  return false;
}

// 获取工具数量
size_t WebSocketMCP::getToolCount() {
  return _tools.size();
}

// 清空所有工具
void WebSocketMCP::clearTools() {
  _tools.clear();
  Serial.println("[WebSocketMCP] 已清空所有工具");
}

// 格式化JSON字符串，每个键值对占一行
String WebSocketMCP::formatJsonString(const String &jsonStr) {
  // 1. 处理空字符串或无效JSON
  if (jsonStr.length() == 0) {
    return "{}";
  }

  // 2. 尝试解析JSON以确保其有效
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, jsonStr);
  
  if (error) {
    // 如果解析失败，返回原字符串
    return jsonStr;
  }
  
  // 3. 初始化结果字符串
  String result = "{\n";
  
  // 4. 获取JSON中的所有键
  JsonObject obj = doc.as<JsonObject>();
  bool firstItem = true;
  
  // 5. 遍历JSON对象中的每个键值对
  for (JsonPair p : obj) {
    if (!firstItem) {
      result += ",\n";
    }
    firstItem = false;
    
    // 添加两个空格的缩进
    result += "  \"" + String(p.key().c_str()) + "\": ";
    
    // 根据值的类型添加相应的表示
    if (p.value().is<JsonObject>() || p.value().is<JsonArray>()) {
      // 如果值是对象或数组，使用serializeJson转换
      String nestedJson;
      serializeJson(p.value(), nestedJson);
      result += nestedJson;
    } else if (p.value().is<const char*>() || p.value().is<String>()) {
      // 如果是字符串，添加引号
      result += "\"" + String(p.value().as<const char*>()) + "\"";
    } else {
      // 对于其他类型（数字、布尔值等）
      String valueStr;
      serializeJson(p.value(), valueStr);
      result += valueStr;
    }
  }
  
  // 6. 添加结束括号（换行并闭合）
  result += "\n}";
  
  return result;
}


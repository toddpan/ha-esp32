
# MCP大模型智能家居网关

![version](https://img.shields.io/badge/version-v1.0.15-blue)
![ESP32](https://img.shields.io/badge/platform-ESP32-green)

## 项目简介
基于ESP32的智能家居控制系统，通过MCP协议实现大模型对小爱音箱和米家设备的深度控制。

[演示视频](https://b23.tv/0KlOaJY) | [完整教程](https://bxk64web49.feishu.cn/docx/XAVJdha5FoI5bjxKELqcz3rJnwg) | [固件下载](https://github.com/toddpan/ha-esp32/releases/)

## 🚀 技术架构

![MCP架构图](image.png)


## ✨ 核心功能

### 米家平台集成
- 小爱音箱设备控制
- TTS文本语音播报
- 支持40+款小爱音箱型号

### 设备适配
- 虾哥小智设备兼容（MCP协议）
- 微信小程序配置端
- 云平台HTTP接口开放（[接口文档](https://oneapi.sooncore.com/openapi/)）

### Coze平台扩展
- 飞阳MCP网关插件（[安装地址](https://www.coze.cn/store/plugin/7523201219662184483)）
- [接入教程](https://bxk64web49.feishu.cn/docx/XRBGdxUl2oDDQHxiBrOcZi1KnGj)

---

## 🎯 功能演示

```bash
# 设备控制示例
"小爱同学，打开客厅空调"

# TTS播报示例
"小爱同学，播报系统通知"

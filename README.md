# 🖨️ ArduinoMFP Library  
**Multi-Function Printer/Scanner SOAP Interface for ESP32 (and similar Wi-Fi boards)**

---

## 📘 Overview
`ArduinoMFP` is an Arduino C++ library designed to control and interact with **network multifunction printers (MFPs)** — specifically those that support **WSD, eSCL, or AirScan protocols** — over Wi-Fi.

It allows an ESP32 (or other Wi-Fi-enabled MCU) to:
- Discover network printers and scanners via **mDNS (Bonjour/ZeroConf)**
- **Scan documents** and optionally save them to SPIFFS or LittleFS
- **Send print jobs** via raw TCP socket
- **Query device metadata** (model name, supported services, URLs)
- Dynamically detect correct SOAP endpoints and schema versions for interoperability

The library wraps SOAP (Simple Object Access Protocol) XML messages for WSD printers/scanners, enabling full control without external libraries.

---

## 🧱 Architecture Summary
Internally, the library orchestrates several SOAP calls over HTTP:
1. `supported()` — Queries `/WebServices/Device` for model and service metadata.
2. `createScanJob()` — Builds and sends `CreateScanJobRequest` SOAP.
3. `retrieveImage()` — Fetches binary JPEG via multipart boundary parsing.
4. `scan()` — Combines the above calls, auto-saves result, and returns a buffer.

All SOAP calls follow Microsoft’s WSD/Scan (2006/08) schema and include dynamic UUIDs for `MessageID` and `JobUUID`.

---

## ⚙️ Features
- 🔍 **Device Discovery:** Search for available printers/scanners via mDNS  
- 🧭 **Dynamic Endpoint Detection:** Automatically resolves `ScannerService` URL from the printer’s WSD metadata  
- 🧰 **Schema Detection:** Identifies supported WSD schemas (`2004`, `2005`, `2006`) for compatibility  
- 📠 **Scanning Support:** Initiate scan jobs and retrieve JPEG data  
- 💾 **Filesystem Output:** Save scanned images to SPIFFS or LittleFS  
- 🧾 **Printing Support:** Send plain-text payloads to TCP-based printers  
- 🧩 **Metadata Fetching:** Retrieve printer model info and service URLs via WSD SOAP  
- 🔐 **Self-Managed Memory:** Automatically allocates and frees scan buffers  
- 🧩 **Enhanced XML Parsing:** Handles namespaces, attributes, and multiple schema tags  
- 🌐 **Error Reporting:** SOAP failures and timeouts return JSON-like error messages

---

## 📦 Installation
1. Create a folder:  
   ```
   Arduino/libraries/ArduinoMFP/
   ```
2. Place both files inside:  
   - `ArduinoMFP.cpp`  
   - `ArduinoMFP.h`
3. In your Arduino sketch:
   ```cpp
   #include <ArduinoMFP.h>
   ```
4. Make sure your board supports:
   - Wi-Fi (`<WiFi.h>`)
   - mDNS (`<ESPmDNS.h>`)
   - SPIFFS or LittleFS

---

## 🚀 Usage

### 1️⃣ Initialization
```cpp
#include <ArduinoMFP.h>

ArduinoMFP mfp;  // Create a library instance
```

---

### 2️⃣ Discover Printers or Scanners
```cpp
// mode = 0 → Printers, 1 → Scanners
String found = mfp.look(1);
Serial.println(found);
```
Example output:
```json
{"scanners":[{"host":"BrotherDCP","ip":"172.20.8.35","port":80}]}
```

---

### 3️⃣ Query Device Metadata
```cpp
String info = mfp.supported("172.20.8.35", 80);
Serial.println(info);
```
Example output:
```json
{
  "modelName": "Brother DCP-L2540DW series",
  "modelUrl": "http://www.brother.com",
  "printerService": "http://172.20.8.35:80/WebServices/PrinterService",
  "scannerService": "http://172.20.8.35:80/WebServices/ScannerService",
  "schema": "2006-scan,2004-xfer,2004-addr"
}
```

> The library can parse XML fields even when they include namespaces like `wsdp:` or `pnpx:`.

---

### 4️⃣ Scan a Document
```cpp
uint8_t* image = mfp.scan(
  300, 300,           // resolution height/width
  "Platen",           // source (Platen or Feeder)
  "172.20.8.35", 80,  // device IP and port
  "image/jpeg",       // format
  0                   // filesystem: 0=SPIFFS, 1=LittleFS, -1=no save
);

if (image != nullptr) {
  Serial.printf("Scan complete! Size: %d bytes\n", mfp.getImageSize());
}
```

> The `scan()` method first calls `supported()` to detect the correct `ScannerService` endpoint before starting the scan job.  
> When `filesystem` is set, it automatically saves `/scan.jpg` to SPIFFS or LittleFS.

---

### 5️⃣ Print Text
```cpp
String response = mfp.print("172.20.8.35", 9100, "Hello from ESP32!");
Serial.println(response);
```

---

## 🧠 Class Summary

| Method | Description |
|--------|--------------|
| `uint8_t* scan(int h, int w, const char* origin, const char* url, int port, const char* format, int filesystem)` | Starts a scan job and retrieves JPEG image. |
| `size_t getImageSize()` | Returns number of bytes in the current image buffer. |
| `uint8_t* getImageBuffer()` | Returns pointer to raw image data. |
| `String look(int mode)` | Discovers available printers (0) or scanners (1) via mDNS. |
| `String print(const char* ip, int port, String payload)` | Sends plain-text print job. |
| `String supported(const char* url, int port)` | Fetches model and service metadata using SOAP. |

---

## 💾 Filesystem Options

| Value | Description |
|--------|-------------|
| `0` | Save scan to SPIFFS (`/scan.jpg`) |
| `1` | Save scan to LittleFS (`/scan.jpg`) |
| `-1` | Do not save (keep in memory only) |

---

## 🚨 Error Handling
All major functions (`createScanJob`, `retrieveImage`, `supported`, `scan`) provide clear feedback:
- Empty string or `nullptr` → Connection or SOAP failure.
- JSON object with `"error"` field → SOAP timeout or invalid device.
- Serial monitor output:
  - `❌ Failed to create scan job`
  - `❌ Failed to retrieve image`
  - `💾 Image saved to filesystem`

---

## ⚠️ Known Limitations
- HTTPS not supported (SOAP uses plain HTTP).  
- JPEG only (no PNG/PDF encoding yet).  
- Some AirPrint-only scanners may reject WSD SOAP.  
- Large scans (>5 MB) may require increased ESP32 heap or PSRAM.  
- Blocking transfer — should run in separate task if used with UI.

---

## 🧰 Requirements
- **Board:** ESP32 / ESP8266 (with Wi-Fi & mDNS)
- **Libraries:**
  - `WiFi.h`
  - `ESPmDNS.h`
  - `FS.h`
  - `SPIFFS.h`
  - `LittleFS.h`

---

## 🧪 Example Output
```
📡 Searching for scanners...
{"scanners":[{"host":"BrotherDCP","ip":"172.20.8.35","port":80}]}

📘 Querying device info...
{"modelName":"Brother DCP-L2540DW series","scannerService":"http://172.20.8.35/WebServices/ScannerService"}

📠 Starting scan...
💾 Image saved to filesystem (409632 bytes)
```

---

## 🧑‍💻 Example Sketch
```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <ArduinoMFP.h>

const char* ssid = "YourSSID";
const char* pass = "YourPassword";

ArduinoMFP mfp;

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS init failed!");
    return;
  }

  Serial.println(mfp.supported("172.20.8.35", 80));
  mfp.scan(300, 300, "Platen", "172.20.8.35", 80, "image/jpeg", 0);
}

void loop() {}
```

---

## 🧩 Debug Tips
- Use `Serial.println(mfp.supported(ip, 80));` to verify `ScannerService` endpoint.  
- If no scanners are found via mDNS, repeat `look(1)` after 3 s delay (Bonjour lag).  
- Ensure port 80 (or 8080) is open on the MFP.  
- Check for `wscn:JobId` and `wscn:JobToken` in SOAP replies for successful jobs.

---

## 📄 License

MIT License  

Copyright (c) 2025 Duke Uku  

Permission is hereby granted, free of charge, to any person obtaining a copy  
of this software and associated documentation files (the "Software"), to deal  
in the Software without restriction, including without limitation the rights  
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell  
copies of the Software, and to permit persons to whom the Software is  
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all  
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR  
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,  
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE  
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER  
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,  
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE  
SOFTWARE.

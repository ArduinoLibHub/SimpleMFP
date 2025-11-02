#include "ArduinoMFP.h"
#include <FS.h>        // base filesystem support
#include <SPIFFS.h>    // SPIFFS support
#include <LittleFS.h>  // LittleFS support (make sure your board supports this)
#include <ESPmDNS.h>


ArduinoMFP::ArduinoMFP() : imageBuffer(nullptr), imageSize(0) {}

ArduinoMFP::~ArduinoMFP() {
    freeImageBuffer();
}

void ArduinoMFP::freeImageBuffer() {
    if (imageBuffer) {
        delete[] imageBuffer;
        imageBuffer = nullptr;
        imageSize = 0;
    }
}

String ArduinoMFP::generateUUID() {
    const char* hexChars = "0123456789abcdef";
    char uuid[37]; // 36 + null
    int i = 0;
    for (; i < 36; i++) {
        switch(i) {
            case 8:
            case 13:
            case 18:
            case 23:
                uuid[i] = '-';
                break;
            case 14:
                uuid[i] = '4';
                break;
            case 19:
                uuid[i] = hexChars[random(8, 12)];
                break;
            default:
                uuid[i] = hexChars[random(0, 16)];
        }
    }
    uuid[36] = 0;
    return String(uuid);
}

String ArduinoMFP::buildCreateScanJobSOAP(const String& messageID, const String& jobUUID, int height, int width, const String& origin, const String& format) {
    // Use provided params instead of fixed ones
    return
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<soap:Envelope xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\" "
                   "xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
                   "xmlns:sca=\"http://schemas.microsoft.com/windows/2006/08/wdp/scan\">"
    "<soap:Header>"
      "<wsa:To>http://" + messageID + "/WebServices/ScannerService</wsa:To>"  // will fix in scan()
      "<wsa:Action>http://schemas.microsoft.com/windows/2006/08/wdp/scan/CreateScanJob</wsa:Action>"
      "<wsa:MessageID>urn:uuid:" + messageID + "</wsa:MessageID>"
      "<wsa:ReplyTo><wsa:Address>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</wsa:Address></wsa:ReplyTo>"
      "<wsa:From><wsa:Address>urn:uuid:" + jobUUID + "</wsa:Address></wsa:From>"
    "</soap:Header>"
    "<soap:Body>"
      "<sca:CreateScanJobRequest>"
        "<sca:ScanTicket>"
          "<sca:JobDescription>"
            "<sca:JobName>Scan Job</sca:JobName>"
            "<sca:JobOriginatingUserName>ESP32</sca:JobOriginatingUserName>"
          "</sca:JobDescription>"
          "<sca:DocumentParameters>"
            "<sca:Format sca:MustHonor=\"true\">" + format + "</sca:Format>"
            "<sca:InputSource sca:MustHonor=\"true\">" + origin + "</sca:InputSource>"
            "<sca:MediaSides>"
              "<sca:MediaFront>"
                "<sca:ColorProcessing>RGB24</sca:ColorProcessing>"
                "<sca:Resolution><sca:Width>" + String(width) + "</sca:Width><sca:Height>" + String(height) + "</sca:Height></sca:Resolution>"
              "</sca:MediaFront>"
            "</sca:MediaSides>"
          "</sca:DocumentParameters>"
        "</sca:ScanTicket>"
      "</sca:CreateScanJobRequest>"
    "</soap:Body>"
    "</soap:Envelope>";
}

String ArduinoMFP::buildRetrieveImageSOAP(const String& messageID, const String& jobUUID, const String& jobId, const String& jobToken) {
    return
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<soap:Envelope xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\" "
                   "xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
                   "xmlns:sca=\"http://schemas.microsoft.com/windows/2006/08/wdp/scan\">"
    "<soap:Header>"
      "<wsa:To>http://" + jobUUID + "/WebServices/ScannerService</wsa:To>"  // fix in scan()
      "<wsa:Action>http://schemas.microsoft.com/windows/2006/08/wdp/scan/RetrieveImage</wsa:Action>"
      "<wsa:MessageID>urn:uuid:" + messageID + "</wsa:MessageID>"
      "<wsa:ReplyTo><wsa:Address>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</wsa:Address></wsa:ReplyTo>"
      "<wsa:From><wsa:Address>urn:uuid:" + jobUUID + "</wsa:Address></wsa:From>"
    "</soap:Header>"
    "<soap:Body>"
      "<sca:RetrieveImageRequest>"
        "<sca:JobId>" + jobId + "</sca:JobId>"
        "<sca:JobToken>" + jobToken + "</sca:JobToken>"
        "<sca:DocumentDescription>"
          "<sca:DocumentName>Scanned image file for the WSD Scan Driver</sca:DocumentName>"
        "</sca:DocumentDescription>"
      "</sca:RetrieveImageRequest>"
    "</soap:Body>"
    "</soap:Envelope>";
}

String ArduinoMFP::sendSoapRequest(const String& host, int port, const String& soapBody) {
    WiFiClient client;
    if (!client.connect(host.c_str(), port)) {
        return "";
    }

    String request =
        "POST /WebServices/ScannerService HTTP/1.1\r\n"
        "Host: " + host + "\r\n"
        "Content-Type: application/soap+xml\r\n"
        "Content-Length: " + soapBody.length() + "\r\n"
        "Connection: close\r\n\r\n" +
        soapBody;

    client.print(request);

    String response = "";
    unsigned long timeout = millis();
    while (client.connected() || client.available()) {
        if (client.available()) {
            response += (char)client.read();
            timeout = millis();
        }
        if (millis() - timeout > 5000) break; // 5s timeout
    }
    client.stop();

    return response;
}

String ArduinoMFP::extractTag(const String& data, const String& tag) {
    int start = data.indexOf("<" + tag + ">");
    int end = data.indexOf("</" + tag + ">");
    if (start == -1 || end == -1) return "";
    start += tag.length() + 2;
    return data.substring(start, end);
}

bool ArduinoMFP::createScanJob(const String& host, int port, int height, int width, const String& origin, const String& format, String& jobUUID, String& jobId, String& jobToken) {
    jobUUID = generateUUID();
    String messageID = generateUUID();

    // Build SOAP with correct URL embedded
    String soap = buildCreateScanJobSOAP(messageID, jobUUID, height, width, origin, format);
    soap.replace("http://" + messageID + "/WebServices/ScannerService", "http://" + host + "/WebServices/ScannerService");

    String response = sendSoapRequest(host, port, soap);
    if (response == "") return false;

    jobId = extractTag(response, "wscn:JobId");
    jobToken = extractTag(response, "wscn:JobToken");

    return (jobId != "" && jobToken != "");
}

bool ArduinoMFP::retrieveImage(const String& host, int port, const String& jobUUID, const String& jobId, const String& jobToken) {
    String messageID = generateUUID();
    String soap = buildRetrieveImageSOAP(messageID, jobUUID, jobId, jobToken);
    soap.replace("http://" + jobUUID + "/WebServices/ScannerService", "http://" + host + "/WebServices/ScannerService");

    WiFiClient client;
    if (!client.connect(host.c_str(), port)) return false;

    String request =
        "POST /WebServices/ScannerService HTTP/1.1\r\n"
        "Host: " + host + "\r\n"
        "Content-Type: application/soap+xml\r\n"
        "Content-Length: " + soap.length() + "\r\n"
        "Connection: close\r\n\r\n" +
        soap;

    client.print(request);

    // Read headers to find boundary
    String boundary = "";
    unsigned long startTime = millis();
    bool gotHeaders = false;

    while (millis() - startTime < 5000) {
        while (client.available()) {
            String line = client.readStringUntil('\n');
            line.trim();

            if (line.length() == 0) {
                gotHeaders = true;
                break;
            }

            if (line.startsWith("Content-Type:")) {
                int bIndex = line.indexOf("boundary=");
                if (bIndex != -1) {
                    boundary = line.substring(bIndex + 9);
                    int semi = boundary.indexOf(';');
                    if (semi != -1) boundary = boundary.substring(0, semi);
                    boundary.trim();
                    boundary.replace("\"", "");
                }
            }
        }
        if (gotHeaders) break;
        delay(5);
    }

    if (boundary == "") {
        client.stop();
        return false;
    }

    String boundaryStart = "--" + boundary;
    String boundaryEnd = boundaryStart + "--";

    bool foundImagePart = false;
    bool readingImage = false;

    // Buffer for accumulating image data - realloc as needed
    freeImageBuffer();

    const size_t bufferChunk = 1024;
    size_t allocSize = bufferChunk;
    imageBuffer = new uint8_t[allocSize];
    imageSize = 0;

    while (client.connected() || client.available()) {
        String line = client.readStringUntil('\n');
        line.trim();

        if (line == boundaryStart) {
            foundImagePart = false;
            readingImage = false;
        } else if (line.startsWith("Content-Type: image/jpeg")) {
            foundImagePart = true;
        } else if (foundImagePart && line == "") {
            readingImage = true;
        } else if (line == boundaryEnd) {
            break;
        } else if (readingImage) {
            // binary data starts here
            break;
        }
    }

    if (!readingImage) {
        freeImageBuffer();
        client.stop();
        return false;
    }

    // Read raw binary JPEG data until boundary found again
    const int bLen = boundaryStart.length();
    char* buf = new char[bLen + 4];
    int bufIdx = 0;

    while (client.connected() || client.available()) {
        if (!client.available()) {
            delay(1);
            continue;
        }
        char c = client.read();
        buf[bufIdx++] = c;

        if (bufIdx == bLen) {
            buf[bufIdx] = 0;
            String bufStr = String(buf).substring(0, bufIdx);
            if (bufStr == boundaryStart || bufStr == boundaryEnd) {
                // boundary reached - done reading image
                break;
            }

            // write first char to image buffer
            if (imageSize + 1 > allocSize) {
                // realloc buffer bigger
                allocSize += bufferChunk;
                uint8_t* newBuffer = new uint8_t[allocSize];
                memcpy(newBuffer, imageBuffer, imageSize);
                delete[] imageBuffer;
                imageBuffer = newBuffer;
            }
            imageBuffer[imageSize++] = (uint8_t)buf[0];

            // shift left buffer by 1
            memmove(buf, buf + 1, bufIdx - 1);
            bufIdx--;
        }
    }
    delete[] buf;

    client.stop();

    return imageSize > 0;
}

size_t ArduinoMFP::getImageSize() const {
    return imageSize;
}
 
uint8_t* ArduinoMFP::getImageBuffer() const {
    return imageBuffer;
}


uint8_t* ArduinoMFP::scan(int height, int width, const char* origin, const char* url, int port, const char* format, int filesystem = -1) {
    freeImageBuffer();
    imageSize = 0;

    String host(url);

    String jobUUID, jobId, jobToken;

    if (!createScanJob(host, port, height, width, String(origin), String(format), jobUUID, jobId, jobToken)) {
        return nullptr;
    }

    if (!retrieveImage(host, port, jobUUID, jobId, jobToken)) {
        return nullptr;
    }

    if (filesystem == 0 || filesystem == 1) {
        File filePtr;

        if (filesystem == 0) {
            filePtr = SPIFFS.open("/scan.jpg", FILE_WRITE);
        } else {
            filePtr = LittleFS.open("/scan.jpg", FILE_WRITE);
        }

        if (!filePtr) {
            Serial.println("Failed to open file for writing");
            return nullptr;
        }

        filePtr.write(imageBuffer, imageSize);
        filePtr.close();

        Serial.println("Image saved to filesystem");
    }

    return imageBuffer;
}


String ArduinoMFP::look(int mode) {
    if (mode != 0 && mode != 1) return "{}";  // reject any number not 0 or 1

    String json = "{";
    bool first = true;

    if (mode == 0) {  // Printers
        int n = MDNS.queryService("ipp", "tcp");
        if (n > 0) {
            json += "\"printers\":[";
            for (int i = 0; i < n; i++) {
                if (i > 0) json += ",";
                json += "{\"host\":\"" + MDNS.hostname(i) + "\",\"ip\":\"" +
                        MDNS.address(i).toString() + "\",\"port\":" + MDNS.port(i) + "}";
            }
            json += "]";
        }
    }

    if (mode == 1) {  // Scanners
        const char* services[] = {"uscanner", "scanner", "airscan", "escl"};
        json += "\"scanners\":[";
        bool any = false;
        for (const char* service : services) {
            int n = MDNS.queryService(service, "tcp");
            for (int j = 0; j < n; j++) {
                if (any) json += ",";
                json += "{\"host\":\"" + MDNS.hostname(j) + "\",\"ip\":\"" +
                        MDNS.address(j).toString() + "\",\"port\":" + MDNS.port(j) + "}";
                any = true;
            }
        }
        json += "]";
    }

    json += "}";
    return json;
}

String ArduinoMFP::print(const char* ip, int port, String payload) {
    WiFiClient client;
    String response = "";

    if (!client.connect(ip, (uint16_t)port)) {
        response = "Connection failed! Check if " + String(ip) + ":" + String(port) + " is actually real, open and not being used.";
        return response;
    }

    String message = payload + "\n\f";
    client.print(message);

    unsigned long start = millis();
    while (client.connected() && millis() - start < 10000) {
        while (client.available()) {
            int val = client.read();
            if (val == -1) break; // no data
            char c = (char)val;
            response += c;  // append char to response
        }
    }
    client.stop();

    if (response == "") {
        response = "Successful, but sorry no output received.";
    } else {
        response = "Here is the output:\n" + response;
    }
    return response;
}


String ArduinoMFP::supported(const char* url, int port) {
  WiFiClient client;
  String endpoint = "/WebServices/Device";
  String host = String(url);
  String uuid = "uuid:12345678-1234-1234-1234-123456789abc";

  String soapBody =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<soap:Envelope xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\""
    " xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\""
    " xmlns:wxf=\"http://schemas.xmlsoap.org/ws/2004/09/transfer\""
    " xmlns:wsd=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\">"
    "<soap:Header>"
    "<wsa:To>http://" + host + ":" + String(port) + endpoint + "</wsa:To>"
    "<wsa:Action>http://schemas.xmlsoap.org/ws/2004/09/transfer/Get</wsa:Action>"
    "<wsa:MessageID>" + uuid + "</wsa:MessageID>"
    "</soap:Header>"
    "<soap:Body />"
    "</soap:Envelope>";

  String request =
    "POST " + endpoint + " HTTP/1.1\r\n" +
    "Host: " + host + ":" + String(port) + "\r\n" +
    "Content-Type: application/soap+xml\r\n" +
    "Content-Length: " + String(soapBody.length()) + "\r\n" +
    "Connection: close\r\n\r\n" +
    soapBody;

  if (!client.connect(url, port)) {
    return "{\"error\": \"connection failed\"}";
  }

  client.print(request);

  unsigned long timeout = millis();
  while (client.connected() && !client.available()) {
    if (millis() - timeout > 5000) {
      client.stop();
      return "{\"error\": \"timeout waiting for response\"}";
    }
    delay(10);
  }

  String response;
  while (client.available()) {
    response += client.readString();
  }
  client.stop();

  // --- Improved Extract Helper (handles xml:lang, namespaces, etc.) ---
  auto extractAnyTag = [](const String& xml, const char* const tags[], int tagCount) {
    for (int i = 0; i < tagCount; i++) {
      String tag = String(tags[i]);
      String base = tag;
      int colon = tag.indexOf(':');
      if (colon != -1) base = tag.substring(colon + 1);  // e.g. wsdp:ModelName → ModelName

      // Try full tag form (<wsdp:ModelName ...>value</wsdp:ModelName>)
      int open = xml.indexOf("<" + tag);
      if (open != -1) {
        open = xml.indexOf(">", open);
        if (open != -1) {
          open++;
          int close = xml.indexOf("</" + base + ">", open);
          if (close == -1)
            close = xml.indexOf("</" + tag + ">", open);
          if (close != -1) {
            String result = xml.substring(open, close);
            result.trim();
            return result;
          }
        }
      }

      // Try namespace-free form (<ModelName ...>value</ModelName>)
      open = xml.indexOf("<" + base);
      if (open != -1) {
        open = xml.indexOf(">", open);
        if (open != -1) {
          open++;
          int close = xml.indexOf("</" + base + ">", open);
          if (close != -1) {
            String result = xml.substring(open, close);
            result.trim();
            return result;
          }
        }
      }
    }
    return String("");
  };

  // --- Extract fields ---
  const char* modelTags[] = {"wsdp:ModelName", "pnpx:ModelName", "df:ModelName", "ModelName"};
  const char* modelUrlTags[] = {"wsdp:ModelUrl", "pnpx:ModelUrl", "df:ModelUrl", "ModelUrl"};

  String modelName = extractAnyTag(response, modelTags, 4);
  String modelUrl  = extractAnyTag(response, modelUrlTags, 4);

  // --- Find printer/scanner URLs ---
  String printerUrl = "";
  String scannerUrl = "";
  int pos = 0;
  while ((pos = response.indexOf("<wsdp:Hosted>", pos)) != -1 ||
         (pos = response.indexOf("<Hosted>", pos)) != -1) {
    int end = response.indexOf("</wsdp:Hosted>", pos);
    if (end == -1) end = response.indexOf("</Hosted>", pos);
    if (end == -1) break;
    String block = response.substring(pos, end);

    const char* addrTags[] = {"wsa:Address", "Address"};
    if (block.indexOf("PrinterService") != -1)
      printerUrl = extractAnyTag(block, addrTags, 2);
    if (block.indexOf("ScannerService") != -1)
      scannerUrl = extractAnyTag(block, addrTags, 2);

    pos = end + 1;
  }

  // --- Return JSON summary ---
  String json = "{";
  json += "\"modelName\": \"" + modelName + "\", ";
  json += "\"modelUrl\": \"" + modelUrl + "\", ";
  json += "\"printerService\": \"" + printerUrl + "\", ";
  json += "\"scannerService\": \"" + scannerUrl + "\"";
  json += "}";

  return json;
}

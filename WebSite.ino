#include <ESP8266WebServer.h>


void startWebServer()
{
    Serial.println("Starting Web server");

    server.on("/",                  handleRoot);
    server.on("/config",            handleConfigJson);
    server.on("/status",            handleStatusJson);
    server.on("/brightness",        handleBrightnessJson);
    server.on("/debug",             handleDebugText);
    server.on("/reset",             handleReset);
    server.on("/timezones.json",    handleGetTimezonesJson);
    server.on("/displayModes.json", handleGetDisplayModesJson);
    server.on("/dateFormats.json",  handleGetDateFormatsJson);
    server.on("/alarmSounds.json",  handleGetAlarmSoundsJson);
    server.on("/favicon.ico",       handleGetFavicon);
    server.onNotFound(              handleNotFound);

    server.begin();
}


// return content type and set headers based on type of file to be sent
String getContentTypeAndHeaders(String filename)
{
    String contentType;

    if (filename.endsWith(".ico") )
    {
        server.sendHeader("Cache-Control", "max-age=31536000, immutable");
        server.sendHeader("X-Content-Type-Options", "nosniff");

        contentType = "image/x-icon";
    }
    else
    {
        server.sendHeader("Cache-Control", "no-cache");
        server.sendHeader("X-Content-Type-Options", "nosniff");

        if (filename.endsWith(".html"))
        {
            contentType = "text/html; charset=utf-8";
        }

        else if (filename.endsWith(".css"))
        {
            contentType = "text/css; charset=utf-8";
        }

        else if (filename.endsWith(".json"))
        {
            contentType = "application/json; charset=utf-8";
        }

        else
        {
            contentType = "text/plain; charset = utf-8";
        }
    }

    return (contentType);
}


// handles reading a file from SPIFFS
bool handleFileRead(String path)
{
    // assume success
    bool bReturn = true;

    Serial.println("handleFileRead: " + path);

    // if a folder is requested, send the index file
    if (path.endsWith("/")) path += "index.html";

    // if the file exists as a compressed archive
    if (SPIFFS.exists(path + ".gz"))
    {
        // use the compressed verion
        path += ".gz";
    }

    // if the file exists
    if (SPIFFS.exists(path))
    {
        // get the MIME type and set headers
        String contentType = getContentTypeAndHeaders(path);

        // send the file to the client
        File file = SPIFFS.open(path, "r");
        size_t sent = server.streamFile(file, contentType);
        file.close();

        Serial.println(String("Sent file: ") + path);
    }
    else
    {
        Serial.println(String("File Not Found: ") + path);
        bReturn = false;
    }

    return (bReturn);
}


void handleRoot()
{
    Serial.println("Web handleRoot");

    // turn off buzzer
    buzzerOn = false;

    bool notFoundError = true;

    if (handleFileRead("/Config.html"))
    {
        if (handleFileRead("/timezones.json"))
        {
            if (handleFileRead("/displayModes.json"))
            {
                if (handleFileRead("/dateFormats.json"))
                {
                    if (handleFileRead("/alarmSounds.json"))
                    {
                        notFoundError = false;
                    }
                }
            }
        }
    }

    if (notFoundError)
    {
        server.send(404, "text/plain; charset = utf-8", "404: Not Found");
    }
}


void handleConfigJson()
{
    switch (server.method())
    {
        case HTTP_GET:
            handleGetConfigJson();
            break;

        case HTTP_POST:
            handlePostConfigJson();
            break;

        default:
            server.send(405, "text/plain; charset = utf-8", "Method Not Allowed");
            break;
    }
}


void handleGetConfigJson()
{
    Serial.println("Web handleGetConfigJson");

    DynamicJsonDocument doc = convertConfigToJson();

    String buffer;
    serializeJson(doc, buffer);
    server.sendHeader("Cache-Control", "no-cache");
    server.sendHeader("X-Content-Type-Options", "nosniff");
    server.send(200, "application/json; charset=utf-8", buffer);
}


void handlePostConfigJson()
{
    Serial.println("Web handlePostConfigJson");

    String previousDeviceName     = config.getDeviceName();
    String previousTimezone       = config.getTimezone();
    String previousDisplayMode    = config.getDisplayMode();
    String previousDateFormat     = config.getDateFormat();
    String previousAlarmSound     = config.getAlarmSound();
    String previousSnoozeTime     = config.getSnoozeTime();
    bool   previousAutoBrightness = config.getAutoBrightnessEnable();

    DynamicJsonDocument json(2048);

    deserializeJson(json, server.arg("plain"));
    loadSettingsFromJson(json);

    // Write it out to the serial console
    serializeJson(json, Serial);
    Serial.println();

    saveSettings();

    String deviceName     = config.getDeviceName();
    String timezone       = config.getTimezone();
    String displayMode    = config.getDisplayMode();
    String dateFormat     = config.getDateFormat();
    String alarmSound     = config.getAlarmSound();
    String snoozeTime     = config.getSnoozeTime();
    bool   autoBrightness = config.getAutoBrightnessEnable();

    server.sendHeader("Cache-Control", "no-cache");
    server.sendHeader("X-Content-Type-Options", "nosniff");
    server.send(200, "text/plain; charset=utf-8");

    if (!deviceName.equalsIgnoreCase(previousDeviceName))
    {
        Serial.print("Device Name changed to: ");
        Serial.println(deviceName);
        MDNS.setHostname(deviceName);
        ArduinoOTA.setHostname(deviceName.c_str());
    }

    if (!timezone.equalsIgnoreCase(previousTimezone))
    {
        Serial.print("Timezone changed to: ");
        Serial.println(timezone);
        setup_timezone();
    }

    if (!displayMode.equalsIgnoreCase(previousDisplayMode))
    {
        Serial.print("Display Mode changed to: ");
        Serial.println(displayMode);
    }


    if (!dateFormat.equalsIgnoreCase(previousDateFormat))
    {
        Serial.print("Date Format changed to: ");
        Serial.println(dateFormat);
    }

    if (!alarmSound.equalsIgnoreCase(previousAlarmSound))
    {
        Serial.print("Alarm Sound changed to: ");
        Serial.println(alarmSound);
    }

    if (!snoozeTime.equalsIgnoreCase(previousSnoozeTime))
    {
        Serial.print("Snooze Time changed to: ");
        Serial.println(snoozeTime);
    }

    if (autoBrightness != previousAutoBrightness)
    {
        Serial.print("Auto Brightness changed to: ");
        Serial.println(autoBrightness ? "enabled" : "disabled");
    }
}


void handleStatusJson()
{
//     Serial.println("Web handleStatusJson");

    char tempBuffer[40];

    DynamicJsonDocument doc(2048);

    doc["SSID"] = WiFi.SSID();
    doc["IP"]   = WiFi.localIP().toString();
    doc["RSSI"] = WiFi.RSSI();

    uint8_t tempHr  = currentHour;
    uint8_t tempMin = currentMinute;
    uint8_t tempSec = currentSecond;
    switch (config.getDisplayModeNumber())
    {
        // 12 hour display modes
        case TIME_MODE_12_SOLID_COLON:
        case TIME_MODE_12_SOLID_COLON_AM_LED:
        case TIME_MODE_12_SOLID_COLON_PM_LED:
        case TIME_MODE_12_BLINKING_COLON_ALWAYS:
        case TIME_MODE_12_BLINKING_COLON_AM:
        case TIME_MODE_12_BLINKING_COLON_PM:
            {
                String AM_PM_string = (tempHr < 12) ? "AM" : "PM";
                if (tempHr == 0)
                {
                    tempHr = 12;
                }
                if (tempHr >= 13)
                {
                    tempHr -= 12;
                }
                sprintf(tempBuffer, "%u:%02u:%02u %s", tempHr, tempMin, tempSec, AM_PM_string);
            }
            break;

        // else must be 24 hour mode
        default:
        case TIME_MODE_24_SOLID_COLON:
        case TIME_MODE_24_BLINKING_COLON:
            sprintf(tempBuffer, "%02u:%02u:%02u", tempHr, tempMin, tempSec);
            break;
    }
    doc["TIME"] = tempBuffer;

    if (!updatedByNTP)
    {
        sprintf(tempBuffer, "never");
    }
    else
    {
        unsigned long secondsSinceNTP = (millis() - last_NTP_Update) / 1000;
        unsigned long minutesSinceNTP = secondsSinceNTP / 60;
        unsigned long hoursSinceNTP   = minutesSinceNTP / 60;
        unsigned long daysSinceNTP    = hoursSinceNTP   / 24;
        secondsSinceNTP %= 60;
        minutesSinceNTP %= 60;
        hoursSinceNTP   %= 24;
        if (daysSinceNTP > 0)
        {
            sprintf(tempBuffer, "%u dy, %u hr, %u min, %u sec",
                    (unsigned int)daysSinceNTP, (unsigned int)hoursSinceNTP, (unsigned int)minutesSinceNTP, (unsigned int)secondsSinceNTP);
        }
        else if (hoursSinceNTP > 0)
        {
            sprintf(tempBuffer, "%u hr, %u min, %u sec",
                    (unsigned int)hoursSinceNTP, (unsigned int)minutesSinceNTP, (unsigned int)secondsSinceNTP);
        }
        else if (minutesSinceNTP > 0)
        {
            sprintf(tempBuffer, "%u min, %u sec",
                    (unsigned int)minutesSinceNTP, (unsigned int)secondsSinceNTP);
        }
        else
        {
            sprintf(tempBuffer, "%u sec",
                    (unsigned int)secondsSinceNTP);
        }
    }
    doc["NTP"] = tempBuffer;

    doc["RESET"] = lastResetTime;

    doc["BRIGHTNESS"] = currentBrightness;

    String buffer;
    serializeJson(doc, buffer);

    server.sendHeader("Cache-Control", "no-cache");
    server.sendHeader("X-Content-Type-Options", "nosniff");
    server.send(200, "application/json; charset=utf-8", buffer);
}


void handleBrightnessJson()
{
    Serial.println("Web handleBrightnessJson");

    DynamicJsonDocument json(128);

    deserializeJson(json, server.arg("plain"));

    uint8_t newBrightness = json["newBrightness"];
    if ((newBrightness >= MIN_BRIGHTNESS)  &&  (newBrightness <= MAX_BRIGHTNESS))
    {
        manualBrightness = newBrightness;
        setDisplayBrightness(manualBrightness, true);
    }

    server.sendHeader("Cache-Control", "no-cache");
    server.sendHeader("X-Content-Type-Options", "nosniff");
    server.send(200, "text/plain; charset=utf-8");
}


void handleDebugText()
{
    Serial.println(server.arg("plain"));

    server.sendHeader("Cache-Control", "no-cache");
    server.sendHeader("X-Content-Type-Options", "nosniff");
    server.send(200, "text/plain; charset=utf-8");
}


void handleReset()
{
    server.sendHeader("Cache-Control", "no-cache");
    server.sendHeader("X-Content-Type-Options", "nosniff");
    server.send(200, "text/plain; charset=utf-8");

    if (server.arg("plain").equalsIgnoreCase("ResetNow"))
    {
        delay(500);
        ESP.restart();
    }
}


void handleGetTimezonesJson()
{
    Serial.println("Web handleGetTimezonesJson");
    handleFileRead("/timezones.json");
}


void handleGetDisplayModesJson()
{
    Serial.println("Web handleGetDisplayModesJson");
    handleFileRead("/displayModes.json");
}


void handleGetDateFormatsJson()
{
    Serial.println("Web handleGetDateFormatsJson");
    handleFileRead("/dateFormats.json");
}


void handleGetAlarmSoundsJson()
{
    Serial.println("Web handleGetAlarmSoundsJson");
    handleFileRead("/alarmSounds.json");
}


void handleGetFavicon()
{
    Serial.println("Web handleGetFavicon");
    handleFileRead("/favicon.ico");
}


void handleNotFound()
{
    Serial.println("Web handleNotFound");

    String message = "Not Found Error\n\n";
    message += "URI: " + server.uri();
    message += "\n\n Method: " + (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\n\n Arguments: "+ server.args();
    message += "\n";

    server.send(404, "text/plain", message);
}

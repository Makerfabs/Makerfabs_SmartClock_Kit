#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <SPI.h>
#include "wifi_save.h"
#include <WiFi.h>
#include "time.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "a1.h"
#include "sun.h"
#include "rain.h"
#include "cloud.h"
#include "snow.h"

#define BUZZER_PIN 32

#define BUTTON_S WIFI_SET_PIN
#define BUTTON1 26
#define BUTTON2 27

#define TFT_CS 15        // Hallowing display control pins: chip select
#define TFT_RST 23       // Display reset
#define TFT_DC 22        // Display data/command select
#define TFT_BACKLIGHT -1 // Display backlight pin

#define TFT_MOSI 13 // Data out
#define TFT_SCLK 14 // Clock out

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

const char *ntpServer = "120.25.108.11";
const long gmtOffset_sec = (-5) * 60 * 60; //China+8
const int daylightOffset_sec = 0;

struct tm timeinfo;
const int page_num = 4;
int page = 0;
int page_line = 0;
int page_add = 0;
int16_t alarm_h = 8;
int16_t alarm_m = 0;
int16_t timezone = 0;
int alarm_flag = 0;
int alarm_enable = 1;
int last_h = -1;
int last_m = -1;
long int weather_runtime = -600000;
int button_flag = 0;
int location_index = 0;

String weather_location[] = {"Newyork", "London", "beijing", "paris"};
String timezone_city[24] = {"-11", "-10", "-9", "Vancouver", "-7", "Chicago", "Newyork", "-4", "-3", "-2", "-1", "London", "Paris", "Athens", "Moscow", "4", "New Delhi", "Bangladesh", "Bangkok", "Beijing", "Tokyo", "10", "11", "12"};

void setup(void)
{

    Serial.begin(115200);
    Serial.print(F("Hello! Makerfabs Smartclock Kit"));

    pin_init();
    tft_init();
    congfig_init();
    wifi_init();

    tft.setCursor(0, 110);
    tft.print("All init over");
    delay(2000);

    tft.fillScreen(ST77XX_BLACK);

    //Show logo
    tft.drawRGBBitmap(0, 0, a1, 128, 128);
    delay(1000);
}

void loop()
{
    main_menu();
}

//Init

void pin_init()
{
    Serial.println("Start PIN init");

    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(BUTTON_S, INPUT_PULLUP);
    pinMode(BUTTON1, INPUT_PULLUP);
    pinMode(BUTTON2, INPUT_PULLUP);

    /*
    digitalWrite(BUZZER_PIN, HIGH);
    delay(500);
    digitalWrite(BUZZER_PIN, LOW);
    */

    Serial.println("PIN init done");
    delay(100);
}

void tft_init()
{
    Serial.println("Start TFT init");

    tft.initR(INITR_144GREENTAB);
    tft.fillScreen(ST77XX_BLACK);
    delay(100);

    Serial.println("TFT init done");
    delay(100);

    tft.setCursor(0, 0);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1);
    tft.print("TFT init over");
}

void wifi_init()
{
    Serial.println("Start WIFI config and ntp init");
    tft.fillScreen(ST77XX_BLACK);

    tft.setCursor(0, 10);
    tft.print("WIFI init start");

    tft.setCursor(0, 30);
    tft.print("If you want set WIFI. Hold down S1 button 3 second, then clock will go into Settings Mode. Use your phone connect to Makerfabs_ap, and visit \"192.168.4.1\".");

    if (wifi_set_main())
    {
        tft.fillRect(0, 20, 128, 128, ST77XX_BLACK);
        Serial.println("Connect WIFI SUCCESS");
        tft.setCursor(0, 50);
        tft.print("Connect WIFI SUCCESS");
    }
    else
    {
        tft.fillRect(0, 30, 128, 100, ST77XX_BLACK);
        Serial.println("Connect WIFI FAULT");
        tft.setCursor(0, 50);
        tft.print("Connect WIFI FAULT");
    }

    //configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    configTime((const long)(timezone * 3600), daylightOffset_sec, ntpServer);
    Serial.println(F("Alread get npt time."));
    tft.setCursor(0, 100);
    tft.print("Alread get npt time");
}

void congfig_init()
{
    read_congfig(&alarm_h, &alarm_m, &timezone);
    tft.setCursor(10, 10);
    tft.print("Alread get congfig");
    delay(500);
}

//Menu

void main_menu()
{
    if (!getLocalTime(&timeinfo))
    {
        Serial.println("Failed to obtain time");
    }

    switch (page)
    {
    case 0:
        clock_page();
        break;
    case 1:
        alarm_page();
        break;
    case 2:
        timezone_page();
        break;
    case 3:
        weather_page();
        //weather_request();
        break;

    default:
        break;
    }

    //Check alarm clock
    if (timeinfo.tm_hour == alarm_h && timeinfo.tm_min == alarm_m && alarm_enable == 1)
    {
        if (alarm_flag == 0)
            alarming();
        alarm_flag = 1;
    }
    else
    {
        alarm_flag = 0;
    }

    //Check Button Status in 10s
    int runtime = millis();
    while (millis() - runtime < 10000)
    {
        if (digitalRead(BUTTON_S) == LOW)
        {
            delay(40);
            if (digitalRead(BUTTON_S) == LOW)
            {
                button_flag = 1;
                page = ++page % page_num;
                page_line = 0;
                page_add = 0;

                break;
            }
        }

        if (digitalRead(BUTTON1) == LOW)
        {
            delay(40);
            if (digitalRead(BUTTON1) == LOW)
            {
                button_flag = 1;
                page_line++;
                break;
            }
        }

        if (digitalRead(BUTTON2) == LOW)
        {
            delay(40);
            if (digitalRead(BUTTON2) == LOW)
            {
                button_flag = 1;
                page_add++;
                break;
            }
        }

        delay(100);
    }
}

void clock_page()
{
    //If minute wasn't change,don't flesh screen
    if (last_h == timeinfo.tm_hour && last_m == timeinfo.tm_min && button_flag == 0)
        return;
    else
    {
        button_flag = 0;
        last_h = timeinfo.tm_hour;
        last_m = timeinfo.tm_min;
    }
    tft.fillScreen(ST77XX_BLACK);

    String date_str = (String)(timeinfo.tm_year + 1900) + "/" + (String)(timeinfo.tm_mon + 1) + "/" + (String)(timeinfo.tm_mday);

    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1);
    tft.setCursor(10, 10);
    tft.print(date_str);

    //Check weather alarm enable set
    if (page_add == 1)
    {
        page_add = 0;
        alarm_enable = (alarm_enable + 1) % 2;
    }

    //Check alarm enable display
    if (alarm_enable)
    {
        tft.fillRect(70, 110, 5, 5, ST77XX_YELLOW);
    }
    else
    {
        tft.fillRect(70, 110, 5, 5, ST77XX_BLACK);
    }

    //Config display
    tft.setCursor(80, 110);
    String alarm_str = "";
    alarm_h < 10 ? alarm_str += "0" : alarm_str += "";
    alarm_str += (String)alarm_h + ":";
    alarm_m < 10 ? alarm_str += "0" : alarm_str += "";
    alarm_str += (String)alarm_m;
    tft.print(alarm_str);

    //Clock display
    tft.setCursor(45, 30);
    tft.setTextColor(ST77XX_YELLOW);
    tft.setTextSize(4);

    if (timeinfo.tm_hour < 10)
        tft.print("0");
    tft.print(timeinfo.tm_hour);
    tft.setCursor(45, 64);
    if (timeinfo.tm_min < 10)
        tft.print("0");
    tft.print(timeinfo.tm_min);
}

void weather_page()
{
    //Every ten minutes requst.
    if (millis() - weather_runtime < 600000 && button_flag == 0)
        return;
    else
    {
        if (page_add == 1)
        {
            page_add = 0;
            location_index = (location_index + 1) % 4;
        }
        button_flag = 0;
        weather_runtime = millis();
    }

    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(0, 10);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1);
    tft.print("Get the weather online");

    tft.setCursor(0, 40);
    tft.print("Waiting ... ...");

    weather_request();
}

void alarm_page()
{
    tft.fillScreen(ST77XX_BLACK);

    tft.setCursor(10, 10);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(2);
    tft.print("ALARM SET");

    //Check button action, set alarm time.
    if (page_line % 2 == 0)
    {
        tft.fillCircle(35, 40, 4, ST77XX_WHITE);
        if (page_add != 0)
        {
            page_add = 0;
            alarm_h = ++alarm_h % 24;
            record_alarm(alarm_h, alarm_m);
        }
    }
    else
    {
        tft.fillCircle(35, 74, 4, ST77XX_WHITE);
        if (page_add != 0)
        {
            page_add = 0;
            alarm_m = ++alarm_m % 60;
            record_alarm(alarm_h, alarm_m);
        }
    }

    tft.setCursor(45, 30);
    tft.setTextColor(ST77XX_YELLOW);
    tft.setTextSize(4);
    if (alarm_h < 10)
        tft.print("0");
    tft.print(alarm_h);
    tft.setCursor(45, 64);
    if (alarm_m < 10)
        tft.print("0");
    tft.print(alarm_m);
}

void timezone_page()
{
    tft.fillScreen(ST77XX_BLACK);

    tft.setCursor(10, 10);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(2);
    tft.print("TIMEZONE");

    tft.setCursor(0, 30);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1);
    tft.print("Effective after rst");
    if (page_add != 0)
    {
        page_add = 0;
        timezone = ++timezone;
        if (timezone > 12)
            timezone = -11;
        record_timezone(timezone);
    }

    tft.setCursor(30, 60);
    tft.setTextColor(ST77XX_YELLOW);
    tft.setTextSize(4);
    if(timezone >= 0)
    tft.print("+");
    tft.print(timezone);

    tft.setCursor(20, 90);
    tft.setTextColor(ST77XX_YELLOW);
    tft.setTextSize(1);
    //tft.print(timezone_city[timezone + 11]);
}

//Functions

String wind_txt[] = {"north", "northeast", "east", "southeast", "south", "southwest", "west", "northwest"};

void weather_request()
{
    HTTPClient http;
    String text = "";

    Serial.print("[HTTP] begin...\n");
    // configure traged server and url

    //persional api,please change to yourself
    //bool begin(String url);
    String url = "https://free-api.heweather.net/s6/weather/now?location=" + weather_location[location_index] + "&key=2d63e6d9a95c4e8f8d3f65d0b5bcdf7f&lang=en";
    http.begin(url);

    Serial.print("[HTTP] GET...\n");
    // start connection and send HTTP header
    int httpCode = http.GET();

    // httpCode will be negative on error
    if (httpCode > 0)
    {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);

        // file found at server
        if (httpCode == HTTP_CODE_OK)
        {
            String payload = http.getString();
            Serial.println(payload);

            //JSON
            DynamicJsonDocument doc(1024);
            deserializeJson(doc, payload);
            JsonObject obj = doc.as<JsonObject>();

            String cond_num = doc["HeWeather6"][0]["now"]["cond_code"];
            String cond_txt = doc["HeWeather6"][0]["now"]["cond_txt"];
            String tmp = doc["HeWeather6"][0]["now"]["tmp"];
            String hum = doc["HeWeather6"][0]["now"]["hum"];
            int wind_deg = doc["HeWeather6"][0]["now"]["wind_deg"];

            weather_show(cond_num, tmp, hum);
            text = "Shenzhen, " + cond_txt + ", " + tmp + " centigrade, " + wind_txt[wind_deg / 45] + " wind,relative humidity " + hum + " percent.";
            Serial.println(text);
        }
    }
    else
    {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
}

void weather_show(String cond_num, String temperature, String hum)
{
    tft.fillScreen(ST77XX_BLACK);

    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1);
    tft.setCursor(20, 10);
    tft.print(weather_location[location_index]);

    int cond_code = cond_num.toInt();
    Serial.printf("cond_code: %d\n", cond_code);

    //The weather code is in https://dev.qweather.com/docs/start/icons/
    if (cond_code == 100 || cond_code == 150) //sun
        tft.drawRGBBitmap(32, 32, sun, 64, 64);
    else if (cond_code > 100 && cond_code < 300) //cloud
        tft.drawRGBBitmap(32, 32, cloud, 64, 64);
    else if (cond_code >= 300 && cond_code < 400) //rain
        tft.drawRGBBitmap(32, 32, rain, 64, 64);
    else if (cond_code >= 400 && cond_code < 500) //snow
        tft.drawRGBBitmap(32, 32, snow, 64, 64);
    else
    {
        tft.setTextColor(ST77XX_RED);
        tft.setTextSize(2);
        tft.setCursor(20, 60);
        tft.print("No Icon");
    }

    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(2);

    tft.setCursor(10, 110);
    tft.print(temperature);
    tft.print(" C");

    tft.setCursor(80, 110);
    tft.print(hum);
    tft.print("%");
}

void alarming()
{
    int flag = 0;
    int runtime = millis();

    while (1)
    {

        if (millis() - runtime > 1000)
        {
            runtime = millis();
            if (flag % 2 == 0)
            {
                tft.fillRect(0, 0, 128, 24, ST77XX_RED);
                tft.fillRect(0, 104, 128, 24, ST77XX_YELLOW);
                digitalWrite(BUZZER_PIN, HIGH);
            }
            else
            {
                digitalWrite(BUZZER_PIN, LOW);
                tft.fillRect(0, 0, 128, 24, ST77XX_YELLOW);
                tft.fillRect(0, 104, 128, 24, ST77XX_RED);
            }
            flag++;
        }
        if (digitalRead(BUTTON_S) == LOW)
        {
            break;
        }

        if (digitalRead(BUTTON1) == LOW)
        {
            break;
        }

        if (digitalRead(BUTTON2) == LOW)
        {
            break;
        }
        delay(10);
    }

    digitalWrite(BUZZER_PIN, LOW);
}

void record_alarm(int16_t hour, int16_t minute)
{

    // 初始化NVS，并检查初始化情况
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // 如果NVS分区被占用则对其进行擦除
        // 并再次初始化
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Open 打开NVS文件
    printf("\n");
    printf("Opening Non-Volatile Config (NVS) handle... ");
    nvs_handle my_handle;                                // 定义不透明句柄
    err = nvs_open("Config", NVS_READWRITE, &my_handle); // 打开文件
    if (err != ESP_OK)
    {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    }
    else
    {
        printf("Done\n");

        // Write
        printf("Updating alarm_h in NVS ... ");
        err = nvs_set_i16(my_handle, "alarm_h", hour);
        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

        printf("Updating alarm_m in NVS ... ");
        err = nvs_set_i16(my_handle, "alarm_m", minute);
        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

        // Commit written value.
        // After setting any values, nvs_commit() must be called to ensure changes are written
        // to flash storage. Implementations may write to storage at other times,
        // but this is not guaranteed.
        printf("Committing updates in NVS ... ");
        err = nvs_commit(my_handle);
        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

        // Close
        nvs_close(my_handle);
    }

    printf("\n");
}

void record_timezone(int16_t timezone)
{

    // 初始化NVS，并检查初始化情况
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // 如果NVS分区被占用则对其进行擦除
        // 并再次初始化
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Open 打开NVS文件
    printf("\n");
    printf("Opening Non-Volatile Config (NVS) handle... ");
    nvs_handle my_handle;                                // 定义不透明句柄
    err = nvs_open("Config", NVS_READWRITE, &my_handle); // 打开文件
    if (err != ESP_OK)
    {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    }
    else
    {
        printf("Done\n");

        // Write
        printf("Updating alarm_h in NVS ... ");
        err = nvs_set_i16(my_handle, "timezone", timezone);
        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

        printf("Committing updates in NVS ... ");
        err = nvs_commit(my_handle);
        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

        // Close
        nvs_close(my_handle);
    }

    printf("\n");
}

void read_congfig(int16_t *hour, int16_t *minute, int16_t *timezone)
{
    // 初始化NVS，并检查初始化情况
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // 如果NVS分区被占用则对其进行擦除
        // 并再次初始化
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Open 打开NVS文件
    printf("\n");
    printf("Opening Non-Volatile Config (NVS) handle... ");
    nvs_handle my_handle;                                // 定义不透明句柄
    err = nvs_open("Config", NVS_READWRITE, &my_handle); // 打开文件
    if (err != ESP_OK)
    {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    }
    else
    {
        printf("Done\n");

        // Read
        printf("Reading Config from NVS ... \n");

        //Read alarm_h
        err = nvs_get_i16(my_handle, "alarm_h", hour);
        switch (err)
        {
        case ESP_OK:
            printf("Done\n");
            printf("alarm_h: %d\n", *hour);
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            printf("The value is not initialized yet!\n");
            break;
        default:
            printf("Error (%s) reading!\n", esp_err_to_name(err));
        }

        //Read alarm_m
        err = nvs_get_i16(my_handle, "alarm_m", minute);
        switch (err)
        {
        case ESP_OK:
            printf("Done\n");
            printf("alarm_m: %d\n", *minute);
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            printf("The value is not initialized yet!\n");
            break;
        default:
            printf("Error (%s) reading!\n", esp_err_to_name(err));
        }

        //Read timezone
        err = nvs_get_i16(my_handle, "timezone", timezone);
        switch (err)
        {
        case ESP_OK:
            printf("Done\n");
            printf("timezone: %d\n", *timezone);
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            printf("The value is not initialized yet!\n");
            break;
        default:
            printf("Error (%s) reading!\n", esp_err_to_name(err));
        }

        // Close
        nvs_close(my_handle);
    }

    printf("\n");
    return;
}
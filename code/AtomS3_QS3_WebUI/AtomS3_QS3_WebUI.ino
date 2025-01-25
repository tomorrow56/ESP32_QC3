/********************
 * M5Stack Include
 ********************/
#include <M5Unified.h>
#include <FastLED.h>
/********************
 * ESP32 WebUI Include
 ********************/
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

/********************
 * LED設定
 ********************/
#define NUM_LEDS 1
// ATOMS3
#define LED_DATA_PIN 35
CRGB leds[NUM_LEDS];

/********************
 * QC3設定
 ********************/
// 使用するピン番号
/* ATOM S3 */
#define DP_H  5  //GPIO5, ADC1_CH4, 10k
#define DP_L  6  //GPIO6, 2.2k
#define DM_H  7  //GPIO7, ADC1_CH6, 10k
#define DM_L  39 //GPIO39, 2.2k
#define VBUS_DET  8 //GPIO8, ADC1_CH7
#define OUT_EN 38

// D+/D-の設定状態
#define QC_HIZ     0x00
#define QC_0V     0x01
#define QC_600mV   0x02
#define QC_3300mV  0x03

// ADCの読み出し値
uint16_t DP_VAL;
uint16_t DM_VAL;
uint16_t VBUS_DET_VAL;

// 現在のON/OFF状態のフラグ
bool isOn = false;
// 連続モードフラグ
bool isQcVal = false;

// ホストポートの種類
uint8_t HOST_TYPE;
#define BC_NA   0x00
#define BC_DCP  0x01
#define QC3     0x02

// VBUS出力設定値
uint16_t VBUS_VAL;
uint8_t QC_MODE;
#define QC_5V   0x00
#define QC_9V   0x01
#define QC_12V  0x02
#define QC_20V  0x03
#define QC_VAR  0x04

// VBUS可変範囲
#define QC3_Class_A
//#define QC3_Class_B

//#define QC3_VAR_MIN   3600
#define QC3_VAR_MIN   5000    //for ATOMS3
#define QC3A_VAR_MAX  12000    //QC3.0 Class A
#define QC3B_VAR_MAX  20000    //QC3.0 Class B

/********************
 * WiFi設定
 ********************/
// アクセスポイントのSSIDとパスワードを設定
const char* ssid = "ATOMS3_AP"; // アクセスポイント名
const char* password = "01234567"; // パスワード（8文字以上）

// AsyncWebServerオブジェクトをポート80で作成
AsyncWebServer server(80);

/********************
 * HTMLページ
 ********************/
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>QuickCharge 3.0 Control</title>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; margin: 0; padding: 0; font-size: 24px;}
        .button { padding: 20px 40px; margin: 10px; font-size: 48px; border: 1px solid #ccc; border-radius: 5px; cursor: pointer; background-color: #fffdd0;}
        .button:hover { background-color: #fffdd0; }
        .on-off { background-color: red; color: white; } /* 初期状態は緑 */
        .voltage-label { font-size: 48px; }
        .voltage-value { font-size: 48px; color: blue; font-weight: bold;}
    </style>
</head>
<body>
    <h1>QuickCharge 3.0 Control</h1>
    <div>
        <label class="voltage-label">Output: </label>
        <span id="current" class="voltage-value">0</span>
        <label class="voltage-label"> mV</label>
    </div>
    <br>
    <div>
        <button class="button" onclick="sendVoltage(5)">5 V</button>
        <button class="button" onclick="sendVoltage(9)">9 V</button>
        <button class="button" onclick="sendVoltage(12)">12 V</button>
        <button class="button" onclick="sendVoltage(20)">20 V</button>
    </div>
    <div>
        <button class="button" onclick="sendOffset(-200)">-200 mV</button>
        <button id="toggle-btn" class="button on-off" onclick="toggleOnOff()">OFF</button>
        <button class="button" onclick="sendOffset(200)">+200 mV</button>
    </div>
    <script>
        function sendVoltage(voltage) {
            fetch(`/voltage?value=${voltage}`);
        }

        function sendOffset(offset) {
            fetch(`/offset?value=${offset}`);
        }

        // ON/OFFを切り替える関数
        function toggleOnOff() {
            const toggleBtn = document.getElementById('toggle-btn');
            const isCurrentlyOn = toggleBtn.innerText === 'ON'; // 現在の状態を確認

            // 状態を切り替え
            fetch('/toggle?state=' + (isCurrentlyOn ? 'off' : 'on'))
                .then(() => {
                    if (isCurrentlyOn) {
                        toggleBtn.innerText = 'OFF';
                        toggleBtn.style.backgroundColor = 'red';
                    } else {
                        toggleBtn.innerText = 'ON';
                        toggleBtn.style.backgroundColor = 'green';
                    }
                });
        }

        // 500ms毎に現在の値を更新
        setInterval(() => {
            fetch('/current')
                .then(response => response.text())
                .then(data => {
                    document.getElementById('current').innerText = data;
                });
        }, 500);
    </script>
</body>
</html>
)rawliteral";

/**********
* ADCの検出値を電圧値に変換する
* ESP32のADCの直線性が悪い部分は折線で近似
**********/
float readVoltage(uint16_t Vread){
  float Vdc;
  // Convert the read data into voltage
  /*
  if(Vread < 5){
    Vdc = 0;
  }else if(Vread <= 1084){
    Vdc = 0.11 + (0.89 / 1084) * Vread;
  }else if(Vread <= 2303){
    Vdc = 1.0 + (1.0 / (2303 - 1084)) * (Vread - 1084);
  }else if(Vread <= 3179){
    Vdc = 2.0 + (0.7 / (3179 - 2303)) * (Vread - 2303);
  }else if(Vread <= 3659){
    Vdc = 2.7 + (0.3 / (3659 - 3179)) * (Vread - 3179);
  }else if(Vread <= 4071){
    Vdc = 3.0 + (0.2 / (4071 - 3659)) * (Vread - 3659);
  }else{
    Vdc = 3.2;
  }
  */
  // for ESP32-S3
  // https://docs.espressif.com/projects/esp-idf/en/release-v4.4/esp32s3/api-reference/peripherals/adc.html
  Vdc = 0.03 + ((float)Vread / 4096) * 3.3;
  return Vdc;
}

/**********
* D+端子への印加電圧設定
**********/
void set_DP(uint8_t state){
  if(state == QC_HIZ){
    pinMode(DP_H, INPUT);
    pinMode(DP_L, INPUT);
  }else{
    pinMode(DP_H, OUTPUT);
    pinMode(DP_L, OUTPUT);
    if(state == QC_0V){
      digitalWrite(DP_H, LOW);
      digitalWrite(DP_L, LOW);
    }else if(state == QC_600mV){
      digitalWrite(DP_H, HIGH);
      digitalWrite(DP_L, LOW);
    }else if(state == QC_3300mV){
      digitalWrite(DP_H, HIGH);
      digitalWrite(DP_L, HIGH);
    }else{
      digitalWrite(DP_H, LOW);
      digitalWrite(DP_L, LOW);
    }
  }
}

/**********
* D‐端子への印加電圧設定
**********/
void set_DM(uint8_t state){
  if(state == QC_HIZ){
    pinMode(DM_H, INPUT);
    pinMode(DM_L, INPUT);
  }else{
    pinMode(DM_H, OUTPUT);
    pinMode(DM_L, OUTPUT);
    if(state == QC_0V){
      digitalWrite(DM_H, LOW);
      digitalWrite(DM_L, LOW);
    }else if(state == QC_600mV){
      digitalWrite(DM_H, HIGH);
      digitalWrite(DM_L, LOW);
    }else if(state == QC_3300mV){
      digitalWrite(DM_H, HIGH);
      digitalWrite(DM_L, HIGH);
    }else{
      digitalWrite(DM_H, LOW);
      digitalWrite(DM_L, LOW);
    }
  }
}

/**********
* VBUS出力電圧設定
**********/
bool set_VBUS(uint8_t mode){
  if(HOST_TYPE != QC3){
    return false;
  }
  QC_MODE = mode;
  switch(mode){
  case QC_5V:
    set_DP(QC_600mV);
    set_DM(QC_0V);
    VBUS_VAL = 5000;
    break;
  case QC_9V:
    set_DP(QC_3300mV);
    set_DM(QC_600mV);
    VBUS_VAL = 9000;
    break;
  case QC_12V:
    set_DP(QC_600mV);
    set_DM(QC_600mV);
    VBUS_VAL = 12000;
    break;
  case QC_20V:
    set_DP(QC_3300mV);
    set_DM(QC_3300mV);
    VBUS_VAL = 20000;
    break;
  case QC_VAR:
    set_DP(QC_600mV);
    set_DM(QC_3300mV);
    break;
  default:
    set_DP(QC_600mV);
    set_DM(QC_0V);
    VBUS_VAL = 5000;
    break;
 }
  return true;
}

/**********
* 連続動作モード
**********/
void var_inc(){
  if(QC_MODE != QC_VAR){
    return;
  }

  uint16_t QC3_VAR_MAX;
#ifdef QC3_Class_B
  QC3_VAR_MAX = QC3B_VAR_MAX;
#else
  QC3_VAR_MAX = QC3A_VAR_MAX;
#endif

  VBUS_VAL = VBUS_VAL + 200;
  if(VBUS_VAL > QC3_VAR_MAX){
    VBUS_VAL = QC3_VAR_MAX;
  }else{
    set_DP(QC_3300mV);
    delayMicroseconds(200);
    set_DP(QC_600mV);
    delay(100);
  }

}

void var_dec(){
  if(QC_MODE != QC_VAR){
    return;
  }
  VBUS_VAL = VBUS_VAL - 200;

  if(VBUS_VAL < QC3_VAR_MIN){
    VBUS_VAL = QC3_VAR_MIN;
   }else{
    set_DM(QC_600mV);
    delayMicroseconds(200);
    set_DM(QC_3300mV);
    delay(100);
   }
}

/**********
* 接続されたポートの検出
**********/
uint8_t detect_Charger(){
  set_DP(QC_HIZ);
  set_DM(QC_HIZ);

  //stage 1:check BC1.2 DCP
  set_DM(QC_0V);
  // ADC to Voltage(mV)
  DP_VAL = readVoltage(analogRead(DP_H)) * 1000;
   // ADC to Voltage(mV)
  //Serial.print("DP Voltage: ");
  //Serial.println(DP_VAL);
  if(DP_VAL >= 325){
    set_DM(QC_HIZ);
    return BC_NA;
  }else{
  //stage 2: set host to QC3
    set_DM(QC_HIZ);
    set_DP(QC_600mV);
    delay(1500);

   // ADC to Voltage(mV)
    DM_VAL = readVoltage(analogRead(DM_H)) * 1000;
    //Serial.print("DM Voltage: ");
    //Serial.println(DM_VAL);

  //stage 3: set devide to QC3
    int timeout = 20000;
    while(true){
      DM_VAL = readVoltage(analogRead(DM_H)) * 1000; // ADC to Voltage(mV)
      if(DM_VAL < 325){
        //Serial.println("DM Pull-Down is detected");
        break;
      }
      delayMicroseconds(100);
      timeout--;
      if(timeout <= 0){
        //Serial.println("Time Out!");
        return BC_DCP;
        break;
      }
    }
  }
  return QC3;
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  // シリアルモニタを開始
  Serial.begin(115200);
//  while (!Serial);

  //FastLED初期設定
  FastLED.addLeds<WS2811, LED_DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(20);

  // VBUS OUTPUTをOFFにする
  pinMode(OUT_EN, OUTPUT);
  digitalWrite(OUT_EN, LOW);

  //VBUS電圧検出ポートを入力にする
  pinMode(VBUS_DET, INPUT);

  // Chargerの種類を検出する
  HOST_TYPE = detect_Charger();

  Serial.print("Charger type: ");

  switch(HOST_TYPE){
  case BC_NA:
    Serial.println("No charging port");
    // NAの時はLEDを黄色にする
    leds[0] = CRGB::Yellow;
    FastLED.show();
    delay(10);
    break;
  case BC_DCP:
    Serial.println("USB BC1.2 DCP");
    // BC_DCPの時はLEDを橙色にする
    leds[0] = CRGB::Orange;
    FastLED.show();
    delay(10);
    break;
  case QC3:
    Serial.println("QC3.0");
    // QC3の時はLEDを赤にする
    leds[0] = CRGB::Red;
    FastLED.show();
    delay(10);
    // 初期値は5Vに設定
    set_VBUS(QC_5V);
    delay(100);
    break;
  default:
    Serial.println("Unknown");
    // 不明の時はLEDを黄色にする
    leds[0] = CRGB::Yellow;
    FastLED.show();
    delay(10);
    break;
  }

  // アクセスポイントを開始
  WiFi.softAP(ssid, password);
  delay(100);
  IPAddress ip(192,168,4,1);
  IPAddress subnet(255,255,255,0);
  WiFi.softAPConfig(ip, ip, subnet);
  Serial.println("Access Point Started");
  Serial.print("AP name: ");
  Serial.println(ssid);
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP()); // アクセスポイントのIPアドレスを表示

  // HTMLページを表示
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", htmlPage);
  });

  // 電圧変更を処理
  server.on("/voltage", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("value")) {
      String value = request->getParam("value")->value();
      // 連続モードの場合は一旦5Vに設定
      if(isQcVal == true){
        set_VBUS(QC_5V);
        isQcVal = false;
        delay(100);
      }

      // 出力電圧設定
      if(value == "5"){
        set_VBUS(QC_5V);
      }else if(value == "9"){
        set_VBUS(QC_9V);
      }else if(value == "12"){
        set_VBUS(QC_12V);
      }else if(value == "20"){
        #ifdef QC3_Class_B
          set_VBUS(QC_20V);
        #endif
      }
      Serial.print("Voltage set to: ");
      Serial.print(VBUS_VAL);
      Serial.println("mV");
      delay(100);
    }
    request->send(200, "text/plain", "OK");
  });

  // 連続モードの処理
  server.on("/offset", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("value")) {
      //連続モードへ切り替え
      set_VBUS(QC_VAR);
      delay(100);
      //連続モードフラグをセット
      isQcVal = true;
      Serial.println("<Continuous mode>");

      String value = request->getParam("value")->value();
      if(value == "200"){
        var_inc();
      }else if(value == "-200"){
        var_dec();
      }
      Serial.println("Offset: " + value + " mV");
      Serial.print("Voltage set to: ");
      Serial.print(VBUS_VAL);
      Serial.println("mV");
      delay(100);
    }
    request->send(200, "text/plain", "OK");
  });

  // ON/OFF切り替えを処理
  server.on("/toggle", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("state")) {
      String state = request->getParam("state")->value();
      isOn = (state == "on");
      if(isOn == true){
        digitalWrite(OUT_EN, HIGH);
        // ONの時はLEDを黄色にする
        leds[0] = CRGB::Green;
        FastLED.show();
        delay(10);
      }else{
        digitalWrite(OUT_EN, LOW);
        // ONの時はLEDを赤色にする
        leds[0] = CRGB::Red;
        FastLED.show();
        delay(10);
      }
      Serial.println("Output " + String(isOn ? "ON" : "OFF"));
    }
    request->send(200, "text/plain", "OK");
  });

  // 現在の電圧値を測定しUIに送信
  server.on("/current", HTTP_GET, [](AsyncWebServerRequest *request){
    // VBUS_DET端子の検出電圧(ADC to Voltage(mV)
    VBUS_DET_VAL = readVoltage(analogRead(VBUS_DET)) * 1000;
    //抵抗分割: 100kΩ/15kΩ
    String currentValue = (String)(int)(VBUS_DET_VAL * 7.67); // VBUS検出値
    Serial.println("Output: " + currentValue);
    request->send(200, "text/plain", currentValue);
  });

  // サーバーを開始
  server.begin();
}

void loop() {
  // この例ではループコードは必要なし
}

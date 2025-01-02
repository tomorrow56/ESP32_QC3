// 使用するピン番号
/* M5Stack Basic Core */
//#define DP_H  13  //GPIO13, ADC2_CH4, 10k
//#define DP_L  16  //GPIO16, 2.2k
//#define DM_H  26  //GPIO26, DAC_2, ADC2_CH9, 10k
//#define DM_L  17  //GPIO17, 2.2k
/* ATOM S3 */
#define DP_H  5  //GPIO5, ADC1_CH4, 10k
#define DP_L  6  //GPIO6, 2.2k
#define DM_H  7  //GPIO7, ADC1_CH6, 10k
#define DM_L  39 //GPIO39, 2.2k

// D+/D-の設定状態
#define QC_HIZ     0x00
#define QC_0V     0x01
#define QC_600mV   0x02
#define QC_3300mV  0x03

// ADCの読み出し値
uint16_t DP_VAL;
uint16_t DM_VAL;

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

#define QC3_VAR_MIN   3600
#define QC3A_VAR_MAX  12000    //QC3.0 Class A
#define QC3B_VAR_MAX  20000    //QC3.0 Class B

/**********
* ADCの検出値を電圧値に変換する
* ESP32のADCの直線性が悪い部分は折線で近似
**********/
float readVoltage(uint16_t Vread){
  float Vdc;
  // Convert the read data into voltage
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

  set_DP(QC_3300mV);
  delayMicroseconds(200);
  set_DP(QC_600mV);
  delay(100);
  VBUS_VAL = VBUS_VAL + 200;

  uint16_t QC3_VAR_MAX;
#ifdef QC3_Class_B
  QC3_VAR_MAX = QC3B_VAR_MAX;
#else
  QC3_VAR_MAX = QC3A_VAR_MAX;
#endif

  if(VBUS_VAL > QC3_VAR_MAX){
    VBUS_VAL = QC3_VAR_MAX;
  }
}

void var_dec(){
  if(QC_MODE != QC_VAR){
    return;
  }
  set_DM(QC_600mV);
  delayMicroseconds(200);
  set_DM(QC_3300mV);
  delay(100);
  VBUS_VAL = VBUS_VAL - 200;

  if(VBUS_VAL < QC3_VAR_MIN){
    VBUS_VAL = QC3_VAR_MIN;
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
  Serial.begin(115200);
 
  HOST_TYPE = detect_Charger();

  Serial.print("Charger type: ");

  switch(HOST_TYPE){
  case BC_NA:
    Serial.println("No charging port");
    break;
  case BC_DCP:
    Serial.println("USB BC1.2 DCP");
    break;
  case QC3:
    Serial.println("QC3.0");
    break;
  default:
    Serial.println("Unknown");
    break;
  }
}

void loop() {
  set_VBUS(QC_5V);
  Serial.print("Voltage: ");
  Serial.print(VBUS_VAL);
  Serial.println("mV");
  delay(3000);

  set_VBUS(QC_9V);
  Serial.print("Voltage: ");
  Serial.print(VBUS_VAL);
  Serial.println("mV");
  delay(3000);

  set_VBUS(QC_12V);
  Serial.print("Voltage: ");
  Serial.print(VBUS_VAL);
  Serial.println("mV");
  delay(3000);

#ifdef QC3_Class_B
  set_VBUS(QC_20V);
  Serial.print("Voltage: ");
  Serial.print(VBUS_VAL);
  Serial.println("mV");
  delay(3000);
#endif

  set_VBUS(QC_9V);
  Serial.print("Voltage: ");
  Serial.print(VBUS_VAL);
  Serial.println("mV");
  delay(3000);

  set_VBUS(QC_VAR);
  Serial.println("<Continuous mode>");
  Serial.print("Voltage: ");
  Serial.print(VBUS_VAL);
  Serial.println("mV");
  delay(1000);

  for(int i = 0; i < 5; i++){
    var_inc();
    Serial.println("+increment+");
    Serial.print("Voltage: ");
    Serial.print(VBUS_VAL);
    Serial.println("mV");
    delay(1000);
  }

  for(int i = 0; i < 10; i++){
    var_dec();
    Serial.println("-decriment-");
    Serial.print("Voltage: ");
    Serial.print(VBUS_VAL);
    Serial.println("mV");
    delay(1000);
  }

  for(int i = 0; i < 5; i++){
    var_inc();
    Serial.println("+increment+");
    Serial.print("Voltage: ");
    Serial.print(VBUS_VAL);
    Serial.println("mV");
    delay(1000);
  }

}

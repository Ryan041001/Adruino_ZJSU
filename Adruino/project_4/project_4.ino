#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_MOSI  11
#define OLED_CLK   13
#define OLED_DC    9
#define OLED_CS    10
#define OLED_RST   8

Adafruit_SSD1306 display(OLED_MOSI, OLED_CLK, OLED_DC, OLED_RST, OLED_CS);
RTC_DS1307 rtc;

DateTime now;
DateTime alarmTime;
bool alarmEnabled = false;

// 引脚定义
const int buzzer = 6;  // 蜂鸣器连接到D6
const int alarmFreq = 1000;  // 闹铃频率1000Hz

void setup() {
  Serial.begin(9600);
  pinMode(buzzer, OUTPUT);
  digitalWrite(buzzer, LOW);  // 初始化为低电平，确保蜂鸣器不响
  
  // OLED初始化
  if(!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println("SSD1306初始化失败");
    while(1);
  }
  display.clearDisplay();
  
  // 显示开机画面
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(5,10);
  display.print("Smart Clock");
  display.display();
  delay(2000);
  
  // RTC初始化
  if (!rtc.begin()) {
    Serial.println("RTC Error");
    while(1);
  }
  if (!rtc.isrunning()) {
    Serial.println("RTC未运行，设置为编译时间");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

void loop() {
  now = rtc.now();
  handleSerial();
  updateDisplay();
  checkAlarm();
}

void updateDisplay() {
  display.clearDisplay();
  
  // 顶部日期显示
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print(now.year());
  display.print("-");
  if(now.month() < 10) display.print("0");
  display.print(now.month());
  display.print("-");
  if(now.day() < 10) display.print("0");
  display.print(now.day());
  
  // 添加机械表盘 - 在日期右侧
  const int clockX = 85;  // 表盘中心X坐标
  const int clockY = 7;   // 表盘中心Y坐标
  const int radius = 7;   // 表盘半径
  
  // 绘制表盘外圈
  display.drawCircle(clockX, clockY, radius, SSD1306_WHITE);
  
  // 计算时针和分针的角度
  float hourAngle = (now.hour() % 12 + now.minute() / 60.0) * 30 * PI / 180;
  float minAngle = now.minute() * 6 * PI / 180;
  
  // 绘制时针（较短）
  int hourX = clockX + (radius-2) * sin(hourAngle);
  int hourY = clockY - (radius-2) * cos(hourAngle);
  display.drawLine(clockX, clockY, hourX, hourY, SSD1306_WHITE);
  
  // 绘制分针（较长）
  int minX = clockX + radius * sin(minAngle);
  int minY = clockY - radius * cos(minAngle);
  display.drawLine(clockX, clockY, minX, minY, SSD1306_WHITE);
  
  // 星期显示 - 调整位置
  int maxWidth = SCREEN_WIDTH;  // 128像素宽
  int maxHeight = SCREEN_HEIGHT;  // 64像素高
  display.setCursor(maxWidth - 20, 0);
  display.print(getWeekday(now.dayOfTheWeek()));
  
  // 中间时间显示 - 调整大小和位置
  display.setTextSize(2);
  display.setCursor(10, 16);  // 左对齐，减少宽度需求
  if(now.hour()<10) display.print("0");
  display.print(now.hour());
  display.print(":");
  if(now.minute()<10) display.print("0");
  display.print(now.minute());
  display.print(":");
  if(now.second()<10) display.print("0");
  display.print(now.second());

  display.display();
}

String getWeekday(uint8_t day) {
  const char* days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
  return days[day];
}

void handleSerial() {
  if(Serial.available()){
    String cmd = Serial.readStringUntil('\n');
    
    if(cmd.startsWith("SET")){
      setTime(cmd);
    }
    else if(cmd.startsWith("ALARM")){
      setAlarm(cmd);
    }
  }
}

void setTime(String cmd) {
  // 调试输出接收到的命令
  Serial.print("Received time command: ");
  Serial.println(cmd);
  
  // 确保命令格式正确
  if (cmd.length() < 23) {
    Serial.println("Time format error");
    return;
  }
  
  int y = cmd.substring(4,8).toInt();
  int m = cmd.substring(9,11).toInt();
  int d = cmd.substring(12,14).toInt();
  int hh = cmd.substring(15,17).toInt();
  int mm = cmd.substring(18,20).toInt();
  int ss = cmd.substring(21,23).toInt();
  
  // 验证时间有效性
  if (y >= 2000 && y <= 2099 && m >= 1 && m <= 12 && d >= 1 && d <= 31 &&
      hh >= 0 && hh < 24 && mm >= 0 && mm < 60 && ss >= 0 && ss < 60) {
    rtc.adjust(DateTime(y, m, d, hh, mm, ss));
    Serial.println("Time set successfully");
    
    // 立即更新当前时间
    now = rtc.now();
  } else {
    Serial.println("Invalid time values");
  }
}

void setAlarm(String cmd) {
  // 调试输出接收到的命令
  Serial.print("Received alarm command: ");
  Serial.println(cmd);
  
  // 格式检查
  if (cmd.length() < 11) {
    Serial.println("Alarm format error");
    return;
  }
  
  int hh = cmd.substring(6,8).toInt();
  int mm = cmd.substring(9,11).toInt();
  
  // 验证时间有效性
  if (hh >= 0 && hh < 24 && mm >= 0 && mm < 60) {
    // 确保闹钟设置为未来时间
    DateTime currentTime = rtc.now();
    DateTime newAlarm(currentTime.year(), currentTime.month(), currentTime.day(), hh, mm, 0);
    
    // 如果闹钟时间已经过去，设置为明天
    if (newAlarm.unixtime() <= currentTime.unixtime() && 
        (currentTime.hour() > hh || (currentTime.hour() == hh && currentTime.minute() >= mm))) {
      newAlarm = DateTime(currentTime.year(), currentTime.month(), currentTime.day() + 1, hh, mm, 0);
    }
    
    alarmTime = newAlarm;
    alarmEnabled = true;
    Serial.print("Alarm set to: ");
    Serial.print(hh);
    Serial.print(":");
    if(mm < 10) Serial.print("0");
    Serial.println(mm);
  } else {
    Serial.println("Invalid alarm time");
  }
}

void checkAlarm() {
  // 使用tone()函数让蜂鸣器发声
  if(alarmEnabled && now.hour()==alarmTime.hour() 
     && now.minute()==alarmTime.minute() && now.second()<3) {
    // 闹钟在分钟开始的前3秒内持续响铃
    tone(buzzer, alarmFreq);  // 使用tone()函数发声
  } else {
    noTone(buzzer);  // 停止发声
  }
}

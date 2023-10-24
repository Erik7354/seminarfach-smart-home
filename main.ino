#include <EnableInterrupt.h>
#include <LiquidCrystal_I2C.h>
#include <LCD.h>
#include <OneWire.h> 
#include <DallasTemperature.h>


//Vars for Terminal
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE); // initialize LCD
uint32_t temp_soll_or_ist_01 = -1; // Zeile 2 Anzeige
// Vars for Dimmer
const int dimmer_increase = 5; //same for decrease, wie schnell wird sie Hell/Dunkel
const int dimmer_pin = 10; // LED (PIN)
const int dimmer_on_pin = 3; // Button for HIGH an/hell (PIN)
const int dimmer_off_pin = 2; // Button for LOW aus/dunkel (PIN)
const int dimmer_max = 100;
uint32_t dimmer_start = 0;
int dimmer_intensity = 0; // 0-dimmer_max
int dimmer_mode = 0; // 0 == no dimm; 1 == dimm high; 2 == dimm low
// Vars for Heizung
const int temp_pin = 5; // Temp Sensor (PIN)
OneWire oneWire(temp_pin); 
DallasTemperature sensors(&oneWire);
const int temp_soll_minus = 7; // Temp Button Soll minus (PIN)
const int temp_soll_plus = 8; // Temp Button Soll plus (PIN)
const int temp_window = 4; // Temp Switch open/closed (PIN)
const int temp_heat_noHeat = 9; // Temp LED blink when heating (PIN)
boolean temp_window_close = true; // Temp window closed?
boolean temp_window_close_last_state = true; // Temp window closed? - last state
int temp_status = 0; // 0 = Comfort, 1 = StandBy, 2 = Frostschutz
int temp_target_comfort = 21; // target Temp comfort
int temp_target_standBy = temp_target_comfort - 3; // target Temp standBy
const int temp_target_antiFreeze = 5; // target Temp antiFreeze
int temp_target = temp_target_comfort; // target at the time
float temp_Celsius; // momentane Temp
String temp_status_string = "Komfortbetrieb"; // momentaner Modus, für's LCD
const String temp_comfort_string = "Komforbetrieb"; // Komfort Modus für's LCD
const String temp_standBy_string = "Stand By"; // StandBy Modus für's LCD
const String temp_antiFreeze_string = "Frostschutz"; // AntiFreeze Modus für's LCD
// Vars for constant light
const int cl_pin = 14; // photoresistor (PIN)
const int cl_led = 11; // constant light led output (PIN)
const int cl_mode_pin = 12; // clight on/auto/off (PIN)
const int cl_light_max = 255; // 1024 / 10 = cl_light_max
const int cl_light_divisor = 3; // 1024 / cl_light_divisor
int cl_mode = 2; // 0 = on, 1 = auto, 2 = off, 3 = blink
uint32_t cl_millis = 0;
// Vars for Room
const int room_pir = 13; // PIR Sensor (PIN)
boolean room_at_home = true; // Room at home?




void setup() {

  // Dimmer Setup
  pinMode(dimmer_pin, OUTPUT);
  pinMode(dimmer_on_pin, INPUT);
  pinMode(dimmer_off_pin, INPUT);
  analogWrite(dimmer_pin, 0);

  enableInterrupt(dimmer_on_pin, dimmer_plus_ri, RISING);
  enableInterrupt(dimmer_off_pin, dimmer_minus_ri, RISING);
  // Heizung Setup
  pinMode(temp_pin, INPUT);
  pinMode(temp_soll_minus, INPUT);
  pinMode(temp_soll_plus, INPUT);
  pinMode(temp_window, INPUT);

  //enableInterrupt(temp_window, temp_window_change_open, FALLING); // Fenster standard mäßig geschlossen
  enableInterrupt(room_pir, room_leave, FALLING); // Mensch standard mäßig zuhause
  enableInterrupt(temp_soll_minus, temp_soll_change_minus, RISING);
  enableInterrupt(temp_soll_plus, temp_soll_change_plus, RISING);
  // Constant Light Setup
  pinMode(cl_pin, INPUT);
  pinMode(cl_mode_pin, INPUT);
  pinMode(cl_led, OUTPUT);

  enableInterrupt(cl_mode_pin, cl_mode_change, RISING);
  // Terminal Setup
  lcd.begin(16, 2);
  lcd.print(temp_status_string);
  lcd.setCursor(0, 0);

  Serial.begin(9600);

}

void loop() {
  /*if (room_at_home){
    dimmer_lm();
    heater_lm();
    terminal_lm();
    clight_lm();
    lcd.backlight(on);
  }
  else{ lcd.backlight(off); }*/
  dimmer_lm();
  heater_lm();
  terminal_lm();
  clight_lm();
  
  //Serial.println("Runde");
  //delay(100);
}

/************************************/
/* LOOP METHODS
/************************************/

void heater_lm(){
  // MOMENTARY TEMP - analog
  /*int temp_reading = analogRead(temp_pin);
  float temp_voltage = temp_reading * 5.0 / 1024.0;
  //temp_Celsius = (temp_voltage - 0.5) * 100; // Inet Umrechnung, -> ungenau, finden wir
  temp_Celsius = temp_voltage * 100; // eigene Umrechnung, -> geht nicht in minus Bereich */ // ANALOG - TMP35
  //WINDOW SWITCH
  temp_window_close = digitalRead(temp_window);
  if (temp_window_close_last_state != temp_window_close){
    temp_window_close_last_state = temp_window_close;
    if (temp_window_close == HIGH){ temp_window_close = true; 
      temp_window_close = true;
      if(room_at_home){ temp_status = 0; temp_status_change(); }
      else{ temp_status = 1; temp_status_change(); } // überflüssig in der Realität, tritt nur ein wenn ich nicht zuhause bin und gleichzeitig mein Fenster schließe
    }
    else {
      temp_window_close = false;
      temp_status = 2; // Window hat Vorrang, vor allem
      temp_status_change();
    }
  }
  // MOMENTARY TEMP - digital
  sensors.requestTemperatures();
  temp_Celsius = sensors.getTempCByIndex(0);
  if (temp_Celsius < temp_target){ analogWrite(temp_heat_noHeat, 255); }
  else { analogWrite(temp_heat_noHeat, 0); }
}

void dimmer_lm(){
  switch(dimmer_mode){
    case 0:
      break;
    case 1:
      if(millis() - dimmer_start > 1000){
        dimmer_intensity = dimmer_intensity + dimmer_increase;
        if (dimmer_intensity > dimmer_max){ dimmer_intensity = dimmer_max; }
      }
      break;
    case 2:
      if(millis() - dimmer_start > 1000){
        dimmer_intensity = dimmer_intensity - dimmer_increase;
        if (dimmer_intensity < 0){ dimmer_intensity = 0; }
      }
      break;
    default: //nothing
      break;
  }
  analogWrite(dimmer_pin, dimmer_intensity);
}

void terminal_lm(){
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(temp_status_string);
  lcd.setCursor(0, 1);
  if (temp_soll_or_ist_01 == -1){
    lcd.print("Temp: " + String(temp_Celsius) + "");
  }
  else if (millis() - temp_soll_or_ist_01 > 3000){
    lcd.setCursor(0, 1);
    temp_soll_or_ist_01 = -1;
    lcd.print("Temp: " + String(temp_Celsius) + "");
  }
  else{
    lcd.setCursor(0, 1);
    lcd.print("Sollwert: " + String(temp_target));
  }
}

void clight_lm(){
  if(cl_mode == 0){
    analogWrite(cl_led, cl_light_max);
  }
  else if (cl_mode == 1){
    int clight_intensity = cl_light_max - (analogRead(cl_pin) / cl_light_divisor);
    if(clight_intensity > cl_light_max){ clight_intensity = cl_light_max; }
    else if(clight_intensity < 0){ clight_intensity = 0; }
    analogWrite(cl_led, clight_intensity);
    Serial.println(String(clight_intensity));
  }
  else{ analogWrite(cl_led, 0); }
}

/************************************/
/* INTERRUPT METHODS
/************************************/
// TEMP - START
void temp_status_change(){
  //lcd.setCursor(0, 0);
  switch(temp_status){
    case 0: 
      // Komfort auf LCD ausgeben // LCD AUSGABE NUN DIREKT BEI ZUSTANDSÄNDERUNG!
      temp_status_string = temp_comfort_string;
      temp_target = temp_target_comfort;
      break;
    case 1:
      // Stand-By auf LCD ausgeben
      temp_status_string = temp_standBy_string;
      temp_target = temp_target_standBy;
      break;
    case 2:
      // Frostschutz auf LCD ausgeben
      temp_status_string = temp_antiFreeze_string;
      temp_target = temp_target_antiFreeze;
      break;
    default:
      // give it nich, alles druchgeplant
      break;
  }
  //lcd.print(temp_status_string);
}

void temp_soll_change_minus(){
  //Serial.println("temp_soll_change_minus");
  if (temp_status != 2){
    temp_target_comfort -= 1;
    temp_target_standBy -= 1;
    switch (temp_status){
      case 0:
        temp_target = temp_target_comfort;
        break;
      case 1:
        temp_target = temp_target_standBy;
      default:
        // give it nich, alles druchgeplant
        break;
    }
    temp_soll_or_ist_01 = millis();
  }
}

void temp_soll_change_plus(){
  //Serial.println("temp_soll_change_plus");
  if (temp_status != 2){
    temp_target_comfort += 1;
    temp_target_standBy += 1;
    switch (temp_status){
      case 0:
        temp_target = temp_target_comfort;
        break;
      case 1:
        temp_target = temp_target_standBy;
      default:
        // give it nich, alles druchgeplant
        break;
    }
    temp_soll_or_ist_01 = millis();
  }
}
// TEMP - END
// ROOM - START
void room_leave(){
  //Serial.println("room_leave");
  disableInterrupt(room_pir);
  room_at_home = false;
  if (temp_window_close){ temp_status = 1; temp_status_change(); } // Window Vorrang gewähren, weil Window wichtigere Bedingung, deshalb erst abfragen, Zeile 138 passiert das nicht
  //else{ temp_status = 2; } // überflüssig denke ich
  enableInterrupt(room_pir, room_comeIn, RISING);
}

void room_comeIn(){
  //Serial.println("room_comeIn");
  disableInterrupt(temp_window);
  room_at_home = true;
  if (temp_window_close){ temp_status = 0; temp_status_change(); }
  //else{ temp_status = 2; } // überflüssig denke ich
  enableInterrupt(room_pir, room_leave, FALLING);
}
// ROOM - END
// DIMMER - START
void dimmer_plus_ri(){
  disableInterrupt(dimmer_on_pin);
  //Serial.println("dimmer_plus_ri");
  delay(100);
  if (digitalRead(dimmer_on_pin) == HIGH){
    disableInterrupt(dimmer_on_pin);
    dimmer_start = millis();
    dimmer_mode = 1;
    enableInterrupt(dimmer_on_pin, dimmer_plus_fa, FALLING);
  }
  else{ enableInterrupt(dimmer_on_pin, dimmer_plus_ri, RISING); }
}

void dimmer_plus_fa(){
  //Serial.println("dimmer_plus_fa");
  disableInterrupt(dimmer_on_pin);
  if (millis() - dimmer_start <= 1000){ analogWrite(dimmer_pin, dimmer_max); dimmer_intensity = dimmer_max;}
  dimmer_start = 0;
  dimmer_mode = 0;
  enableInterrupt(dimmer_on_pin, dimmer_plus_ri, RISING);
}

void dimmer_minus_ri(){
  disableInterrupt(dimmer_off_pin);
  //Serial.println("dimmer_minus_ri");
  dimmer_start = millis();
  dimmer_mode = 2;
  delay(1000);
  enableInterrupt(dimmer_off_pin, dimmer_minus_fa, FALLING); 
}

void dimmer_minus_fa(){
  //Serial.println("dimmer_minus_fa");
  disableInterrupt(dimmer_off_pin);
  if (millis() - dimmer_start <= 1000){ analogWrite(dimmer_pin, 0); dimmer_intensity = 0;}
  dimmer_start = 0;
  dimmer_mode = 0;
  delay(1000);
  enableInterrupt(dimmer_off_pin, dimmer_minus_ri, RISING);
}
// DIMMER - END
// CONSTANT LIGHT -START
void cl_mode_change(){
  //Serial.println("cl_mode_change");
  disableInterrupt(cl_mode_pin);
  delay(1000);
  if (digitalRead(cl_mode_pin) == HIGH){
    Serial.println("cl_mode_CHANGE");
    if (cl_mode == 2){ cl_mode = 0; }
    else { cl_mode += 1; }
  }
  enableInterrupt(cl_mode_pin, cl_mode_change, RISING);
}
// CONSTANT LIGHT -END

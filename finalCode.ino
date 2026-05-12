//Alex Barajas, Andrew Beltran and Edgar Rodriguez Varela

//timer/buzzer stuff
volatile unsigned char *myTCCR1A = (unsigned char *) 0x80; 
volatile unsigned char *myTCCR1B = (unsigned char *) 0x81; 
volatile unsigned char *myTCCR1C = (unsigned char *) 0x82; 
volatile unsigned char *myTIMSK1 = (unsigned char *) 0x6F; 
volatile unsigned int  *myTCNT1  = (unsigned int *) 0x84; 
volatile unsigned char *myTIFR1  = (unsigned char *) 0x36; 

//porth pin 7 and pin 9
volatile unsigned char *ddr_h  = (unsigned char*) 0x101;
volatile unsigned char *port_h = (unsigned char*) 0x102;

//portb pin 10, pin 50, pin 52
volatile unsigned char *ddr_b  = (unsigned char*) 0x24;
volatile unsigned char *port_b = (unsigned char*) 0x25;
volatile unsigned char *pin_b  = (unsigned char*) 0x23;

//portg pin 39 and pin 41 
volatile unsigned char *ddrG  = (unsigned char*) 0x33;
volatile unsigned char *portG = (unsigned char*) 0x34;

//portl pin 43
volatile unsigned char *ddrL  = (unsigned char*) 0x10A;
volatile unsigned char *portL = (unsigned char*) 0x10B;

//portd pin 38
volatile unsigned char *ddr_d  = (unsigned char*) 0x2A;
volatile unsigned char *port_d = (unsigned char*) 0x2B;
volatile unsigned char *pin_d  = (unsigned char*) 0x29;

//portg pin 40
volatile unsigned char *pinG = (unsigned char*) 0x32;

//serial monitor stuff
#define RDA 0x80
#define TBE 0x20
volatile unsigned char *myUCSR0A = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0  = (unsigned int *) 0x00C4;
volatile unsigned char *myUDR0   = (unsigned char *)0x00C6;

#include <LiquidCrystal.h>
#include <Wire.h>
#include <RTClib.h>

const int RS = 11, EN = 12, D4 = 2, D5 = 3, D6 = 4, D7 = 5;
LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);

RTC_DS3231 rtc;

unsigned int inputFreq = 440;
bool buzzerOn = false;
bool prevBuzzer = false;


//states
// 0 = error, 1 = idle, 2 = active, 3 = off
volatile int state = 1;
int previousState = -1;

//error variable
int invalidCount = 0;

//millis timing for sensor
unsigned long prevSensor = 0;
unsigned long sensorInterval = 200;

void setup(){
  U0init(9600);

  //pin 7 (buzzer)
  *ddr_h |= 0x10;
  *port_h &= 0xEF;

  //ultrasonic sensor
  //pin 9 (trig)
  *ddr_h |= 0x40;
  //pin 10 (echo)
  *ddr_b &= 0xEF;

  //distance LEDs
  //pin 50 (green)
  *ddr_b |= 0x08;
  //pin 52 (red)
  *ddr_b |= 0x02;

  //state LEDs
  *ddrG |= 0x04; 
  *ddrG |= 0x01;
  *ddrL |= 0x40;

  //all OFF
  *portG &= 0xFB;
  *portG &= 0xFE;
  *portL &= 0xBF;

  //pin 18 ON state (isr) button
  *ddr_d &= 0xF7;
  *port_d |= 0x08;

  //pin38 reset button
  *ddr_d &= 0x7F;
  *port_d |= 0x80;

  //pin 40 off state button
  *ddrG &= 0xFD;
  *portG |= 0x02;

  //pin 18 interrupt
  EIMSK |= 0x08;

  EICRA &= 0x3F;
  EICRA |= 0x80;

  sei();

  //lcd setup
  lcd.begin(16, 2);   // 16 columns, 2 rows
  lcd.clear();

  //rtc setup
  Wire.begin();
  if(!rtc.begin()){
    U0println("ERROR: RTC not found");
    state = 0;
  }
}

void loop(){
  //reset button on pin 38
  if((*pin_d & 0x80) == 0){
    state = 1;
    buzzerOn = false;
    stopBuzzer();
    invalidCount = 0;

    //distance LEDS off
    *port_b &= 0xF7;
    *port_b &= 0xFD;

    printTime();
    U0println("RESETTING to idle");
  }

  //set off state pin 40
  if((*pinG & 0x02) == 0){
    state = 3;
  }

  //change states
  if(state != previousState){
    if(state == 0){
      printTime();
      U0println("ERROR state entered");
    }
    else if(state == 1){
      printTime();
      U0println("IDLE state entered");
    }
    else if(state == 2){
      printTime();
      U0println("ACTIVE state entered");
    }
    else if(state == 3){
      printTime();
      U0println("OFF state entered");
    }

    previousState = state;
  }

  setStateLEDs();
  //if on or idle read the sensor on a milis delay
  if(state == 1 || state == 2){
    if(millis() - prevSensor >= sensorInterval){
      prevSensor = millis();
      readUltrasonic();
    }
  }
  else{
    buzzerOn = false;
    stopBuzzer();
    *port_b &= 0xF7;
    *port_b &= 0xFD;
  }

  //on state
  if(state == 2 && buzzerOn){
    buzzTone();
  }

  //display when buzzer turns on onto the serial monitor
  if(buzzerOn != prevBuzzer){
  if(buzzerOn){
    printTime();
    U0println("Buzzer ON");
  }
  else{
    printTime();
    U0println("Buzzer OFF");
  }
  prevBuzzer = buzzerOn;
}

}

//sets the state LED and also prints to the lcd the state
void setStateLEDs(){
  *portG &= 0xFB;
  *portG &= 0xFE;
  *portL &= 0xBF;

  lcd.setCursor(0, 1);
  if(state == 0){//error blue
    //blue
    *portG |= 0x04;  
    lcd.print("ERROR        ");
  }
  else if(state == 1){//idle yellow
    //green
    *portG |= 0x01;
    //red
    *portL |= 0x40;
    lcd.print("IDLE         ");
  }
  else if(state == 2){//on green
    //green
    *portG |= 0x01;
    lcd.print("ACTIVE       ");
  }
  else if(state == 3){//off red
  //red
    *portL |= 0x40;
    lcd.print("System OFF    ");
  }
}


void readUltrasonic(){
  float distance = readDistanceCM();

  //if sensor is timedout
  if(distance == -1){
    invalidCount++;

    //if to many -1s
    if(invalidCount >= 50){
      state = 0;
      buzzerOn = false;
      stopBuzzer();
      printTime();
      U0println("ERROR: Sensor timeout");

      *port_b &= 0xF7;
      *port_b &= 0xFD;
    }

    return;
  }
  //reset counter
  invalidCount = 0;


  //go into error if distsnce out of bounds
  if(distance < 0 || distance > 400){
    state = 0;
    printTime();
    U0println("ERROR: Invalid distance");
    buzzerOn = false;
    stopBuzzer();
    return;
  }

  if(distance < 7){//red too close
    state = 2; // change state to on
    //green off
    *port_b &= 0xF7;
    //red on
    *port_b |= 0x02;
    buzzerOn = true;

  }
  else if(distance < 16){//yellow close
    if(state == 2){
      state = 1;
    }
    //green on
    *port_b |= 0x08;
    //red on
    *port_b |= 0x02;
    buzzerOn = false;
    stopBuzzer();
  }
  else{//green good

    //green on
    *port_b |= 0x08;
    //red on
    *port_b &= 0xFD;
    buzzerOn = false;
    stopBuzzer();
  }

  char buffer[40];

  //convert distance to int to print
  int d = (int)distance;
  //Dist: 
  buffer[0] = 'D';
  buffer[1] = 'i';
  buffer[2] = 's';
  buffer[3] = 't';
  buffer[4] = ':';
  buffer[5] = ' ';

  //convert int to ascii
  int i = 6;
  if(d >= 100){
    buffer[i++] = (d / 100) + '0';
  }
  if(d >= 10){
    buffer[i++] = ((d / 10) % 10) + '0';
  }
  buffer[i++] = (d % 10) + '0';

  // cm
  buffer[i++] = ' ';
  buffer[i++] = 'c';
  buffer[i++] = 'm';
  buffer[i++] = '\0';

  //print time along with distance in serial monitor
  printTime();
  U0println(buffer);

  //print distance on lcd
  lcd.setCursor(0, 0);
  lcd.print("                ");
  lcd.setCursor(0, 0);
  lcd.print("Dist: ");
  lcd.print((int)distance);
  lcd.print(" cm");
}


//buzzer
void buzzTone(){
  *port_h |= 0x10;
  my_delay(inputFreq);

  *port_h &= 0xEF;
  my_delay(inputFreq);
}
void stopBuzzer(){
  *port_h &= 0xEF;
}

void my_delay(unsigned int freq){
  double period = 1.0 / double(freq);
  double half_period = period / 2.0;
  double clk_period = 0.0000000625;

  unsigned int ticks = half_period / clk_period;

  *myTCCR1B &= 0xF8;
  *myTCNT1 = (unsigned int)(65536 - ticks);

  *myTCCR1A = 0x00;
  *myTCCR1B |= 0x01;

  while((*myTIFR1 & 0x01) == 0);

  *myTCCR1B &= 0xF8;
  *myTIFR1 |= 0x01;
}


float readDistanceCM(){
  *port_h &= 0xBF;
  delayMicroseconds(2);

  *port_h |= 0x40;
  delayMicroseconds(10);
  *port_h &= 0xBF;

  unsigned long timeout = micros();

  while((*pin_b & 0x10) == 0){
    if(micros() - timeout > 30000) return -1;
  }

  unsigned long start = micros();

  while((*pin_b & 0x10) != 0){
    if(micros() - start > 30000) return -1;
  }

  unsigned long end = micros();
  return (end - start) / 58.0;
}

//on button interrupt 
ISR(INT3_vect){
  if(state == 3){
    state = 1;
  }
}

void U0init(unsigned long baud){
  unsigned long FCPU = 16000000;
  unsigned int tbaud = (FCPU / 16 / baud) - 1;

  *myUCSR0A = 0x20;
  *myUCSR0B = 0x18;
  *myUCSR0C = 0x06;
  *myUBRR0 = tbaud;
}

void U0putchar(unsigned char data){
  while((*myUCSR0A & TBE) == 0);
  *myUDR0 = data;
}

void U0print(const char str[]){
  int i = 0;
  while(str[i] != '\0'){
    U0putchar(str[i]);
    i++;
  }
}

void U0println(const char str[]){
  U0print(str);
  U0putchar('\n');
}

void getTimestamp(char buffer[]){
  DateTime now = rtc.now();

  int h = now.hour();
  int m = now.minute();
  int s = now.second();

  buffer[0] = (h/10)+'0';
  buffer[1] = (h%10)+'0';
  buffer[2] = ':';
  buffer[3] = (m/10)+'0';
  buffer[4] = (m%10)+'0';
  buffer[5] = ':';
  buffer[6] = (s/10)+'0';
  buffer[7] = (s%10)+'0';
  buffer[8] = '\0';
}

void printTime(){
  char time[10];
  getTimestamp(time);

  U0print(time);
  U0print(" - ");
}
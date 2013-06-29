/*
  Hexapoduino
  tiny 3D-printed hexapod with a lot of accessories :)
  Mirco Piccin aka Pitusso
  CC BY-SA
  
  It can be driven in different ways:
  - by 2 LDRs: hexapod will follow the light
  - by a Nunchuk
  - by serial command received trought a bluetooth module; 
    you can send commands from a PC or from a SmartPhone (sorry no iPhone :))
    I've developed a quick & dirty app for Android using AppInventor.
  
  It can act also as Drawbot, using 2 additional parts:
  http://www.thingiverse.com/thing:110331
  
  Electronic schemas (as for code) can be found here:
  https://github.com/pictux/Hexapoduino
  
  3D printed parts can be founded:
  - hexabot: http://www.thingiverse.com/thing:34796
  - battery holder: http://www.thingiverse.com/thing:109807
  - example face :) :
  - pen support to update hexapod to drawbot: http://www.thingiverse.com/thing:110331
  
 --------------------
  
 The hexapod movement is based on the ArduSnake oscillator library by Juan Gonzalez-Gomez:
 https://github.com/Obijuan/ArduSnake
 and on the work of :
 (c) Carlos Garcia-Saura (Carlosgs), November-2012
 http://www.thingiverse.com/thing:34796
 CC-BY-SA license
 
 --------------------
 
 The Nunchuk controller uses the library by Gabriel Bianconi
 Project URL: http://www.gabrielbianconi.com/projects/arduinonunchuk/
 
 --------------------

 */

#include <Servo.h>
#include <Oscillator.h>

#include <Wire.h>
#include <ArduinoNunchuk.h>

//Oscillators
Oscillator osc_middle, osc_right, osc_left;
unsigned int A=40;     //Amplitude (higher -> longer steps) set 10-40
unsigned int T=4000;   //Period (lower -> faster moves)

//Nunchuk
ArduinoNunchuk nunchuk = ArduinoNunchuk();
const int nunchukPresent = 9; //to check if using Nunchuk

//LDR
const int RIGHT_LDR = A4;
const int LEFT_LDR = A5;
int RIGHT_LDR_VALUE = 0;
int LEFT_LDR_VALUE = 0;

//hexapod controlled by (one method excludes the others):
const unsigned char CTRL_NUNCHUK = 'N';
const unsigned char CTRL_SERIAL = 'S';
const unsigned char CTRL_LDR = 'L';
unsigned char CTRL_TYPE = CTRL_LDR; //default: drive by Analog / LDR

//available drive commands:
const unsigned char CMD_STOP = 'S';
const unsigned char CMD_RIGHT = 'R';
const unsigned char CMD_LEFT = 'L';
const unsigned char CMD_FORWARD = 'F';
const unsigned char CMD_BACKWARD = 'B';
const unsigned char CMD_INCRSPEED = 'I';
const unsigned char CMD_DECRSPEED = 'D';
unsigned char CMD_ACTUAL = CMD_STOP;
unsigned char CMD_QUEUE = CMD_STOP;

unsigned char inbyte;
boolean playing = false;

//debouce et similia
unsigned long millis_actual;
unsigned long millis_prev_execution;
unsigned long millis_prev_analogread;

const unsigned long millis_period = 1000; //only 1 command executed every second
const unsigned long millis_analogread_period = 500; //only 1 analog read executed every second/2


void setup()
{
  delay(1000);

  Serial.begin(38400); 

  //oscillators attach to servos (pin 2, 3, 4)..
  osc_middle.attach(2);
  osc_right.attach(3);
  osc_left.attach(4);

  //..and set parameters
  osc_middle.SetO(-5);   //Correction for the offset of the servos
  osc_right.SetO(-5); //(was -20)
  osc_left.SetO(25);

  osc_middle.SetA(15);   //Middle motor needs a small amplitude (5-10)
  osc_right.SetA(A);
  osc_left.SetA(A);

  set_period_all();
  refresh_all();

  //phase difference
  osc_middle.SetPh(DEG2RAD( 90 ));
  osc_right.SetPh( DEG2RAD( 0 ));
  osc_left.SetPh( DEG2RAD( 0 ));

  //Nunchuck is connected?
  pinMode(nunchukPresent, INPUT);
  digitalWrite(nunchukPresent, HIGH);

  //if nunchuk is connected (D9 to ground, in the jack used for the nunchuk), initialize it
  //and set control method to CTRL_NUNCHUK
  if(digitalRead(nunchukPresent) == LOW) {
    nunchuk.init();
    CTRL_TYPE = CTRL_NUNCHUK;
  }
}

void loop()
{
  millis_actual = millis();

  //default: do nothing
  inbyte = '0'; 
  CMD_ACTUAL = '0';

  //read & process LDRs analog signals (only if control method is CTRL_LDR - default)
  if(CTRL_TYPE == CTRL_LDR) {
    if(millis_actual - millis_prev_analogread > millis_analogread_period) {
      RIGHT_LDR_VALUE = analogRead(RIGHT_LDR);
      LEFT_LDR_VALUE = analogRead(LEFT_LDR);

      //if there's a difference of at least 75 point from one LDR and the other, TURN L or R 
      if (RIGHT_LDR_VALUE < LEFT_LDR_VALUE - 75) {
        inbyte = CMD_LEFT;               
      } 
      else if (LEFT_LDR_VALUE < RIGHT_LDR_VALUE - 75) {
        inbyte = CMD_RIGHT;               
      }
      //if there's a lot of light, INCREASE SPEED
      else if (LEFT_LDR_VALUE > 900 && RIGHT_LDR_VALUE > 900) {
        inbyte = CMD_INCRSPEED;               
      } 
      //if there isn't so much light, DEREASE SPEED
      else if (LEFT_LDR_VALUE < 250 && RIGHT_LDR_VALUE < 250) {
        inbyte = CMD_DECRSPEED;               
      } 
      //if there's darkness, STOP
      else if (LEFT_LDR_VALUE < 130 && RIGHT_LDR_VALUE < 130) {
        inbyte = CMD_STOP;               
      } 
      else {
        inbyte = CMD_FORWARD;
      }  
      
      millis_prev_analogread = millis_actual;      
      
      //DEBUG
      //Serial.print(RIGHT_LDR_VALUE);
      //Serial.print(" ");
      //Serial.println(LEFT_LDR_VALUE);
    }
  }

  //update & read & process nunchuk (only if control method is CTRL_NUNCHUK)
  if(CTRL_TYPE == CTRL_NUNCHUK) {
    nunchuk.update();
    if (nunchuk.analogX < 50) {
      inbyte = CMD_LEFT;
    }  
    if (nunchuk.analogX > 200) {
      inbyte = CMD_RIGHT;
    }  
    if (nunchuk.analogY < 50) {
      inbyte = CMD_BACKWARD;
    }  
    if (nunchuk.analogY > 200) {
      inbyte = CMD_FORWARD;
    }  
    if (nunchuk.cButton == 1) {
      inbyte = CMD_INCRSPEED;
    }
    if (nunchuk.zButton == 1) {
      inbyte = CMD_DECRSPEED;
    }
  }  

  //if there's incoming serial data (bt or other) accept & change control method to CTRL_SERIAL:
  if (Serial.available() > 0) {
    inbyte = Serial.read();
    CTRL_TYPE = CTRL_SERIAL;
  }

  if(inbyte != '0') {
    //save in queue last "good" command
    CMD_QUEUE = inbyte; 
  }

  if (millis_actual - millis_prev_execution > millis_period) {
    //after period. process queue
    CMD_ACTUAL = CMD_QUEUE;    
    Serial.println(CMD_ACTUAL);

    //reset
    CMD_QUEUE = '0';
    millis_prev_execution = millis_actual;
  }

  //process the command
  switch(CMD_ACTUAL){
  case CMD_INCRSPEED:
    if (T > 1000) {
      T -= 250;
      Serial.println(T);
      set_period_all();
    }
    break;
  case CMD_DECRSPEED:
    if (T < 4000) {
      T += 250;  
      Serial.println(T);
      set_period_all();
    }
    break;    
  case CMD_STOP:
    stop_all();
    break;
  case CMD_RIGHT:
    play_all();
    osc_right.SetPh( DEG2RAD( 0 ));
    osc_left.SetPh( DEG2RAD( 180 ));
    break;
  case CMD_LEFT:
    play_all();
    osc_right.SetPh( DEG2RAD( 180 ));
    osc_left.SetPh( DEG2RAD( 0 ));
    break;
  case CMD_FORWARD:
    play_all();
    osc_right.SetPh( DEG2RAD( 0 ));
    osc_left.SetPh( DEG2RAD( 0 ));
    break;
  case CMD_BACKWARD: 
    play_all();
    osc_right.SetPh( DEG2RAD( 180 ));
    osc_left.SetPh( DEG2RAD( 180 ));
    break;
  }

  refresh_all();
}

void stop_all() {
  playing = false;
  osc_middle.Stop();
  osc_right.Stop();
  osc_left.Stop();
}

void play_all() {
  if (playing == false) {
    osc_middle.Play();
    osc_right.Play();
    osc_left.Play();  
    playing = true;
  } 
}

void refresh_all() {
  //Refresh the oscillators
  osc_middle.refresh();
  osc_right.refresh();
  osc_left.refresh(); 
}

void set_period_all() {
  //Set the period of work
  osc_middle.SetT(T); 
  osc_right.SetT(T);
  osc_left.SetT(T); 
}












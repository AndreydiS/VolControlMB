#define softVer 11.1
//2024NOV removed CANEnabled
//2026MAY changing encoder type https://github.com/M-Reimer/EncoderStepCounter + adding 1.3" sh110x display

#define VolControlType 0 //0-SPI MCP42050
#define DisplayType 2 //0-128x32 b&w, 1-128x64 ssd1306 .96" , 2-128x64 sh1106 1.3" (1437 bytes for local variables required) 3-4 digit TM,    1172 too low
#define LDRenabled 0 // 1-Light Detector Connected 0 - not connected
#define additionalVolControl 2 //2-CAN
#if additionalVolControl == 2 
  #define canVolControlBy 1 //1 -by wheel button
  #define defCanVolControlByCarModel 1 //1- MB C-class 2007-2015(204) CAN-A wires under left kick panel CAN-H Brown-Red, CAN-L Brown; 2-BMW
  #define defProgressiveLvlStep 1 //0- lvlStep constant used, 1 - ProgressiveVolStep used <60 - 4 steps, <90 - 2, >90 -1
#endif
#define defEncoderType 1 //0- EC11, 1- KY-040

#include <EEPROM.h>
//GLOBAL PIN DEFINITION
//Encoder
#define pinEncA 2
#define pinEncB 3
#define pinEncButton 5//4
#define pinDigitalSwOut 6 //6-Helix Lite v6, 8-Helix Lite v3 
#define pinCANShield 10 //Chip Select pin for CAN module MCP2515

#if VolControlType == 0 //SPI MCP Helix 2 digi pot + digital sw
  #define pinSPIDigPotCS 9 //8
  #include <SPI.h>
#endif
//GLOBAL PIN DEFINITION

#if additionalVolControl == 2 // External Vol Control By: 0-none, 1-UART, 2-CAN
  #include <mcp_can.h>
  #define intCANShieldInitRetry 3
  #if defCanVolControlByCarModel == 1 //1- MB C-class 2007-2015(204)
    #define canidWheelButton 0x045
    #define defCanSpeed CAN_125KBPS
  #endif

  bool blnCANinitFailed=false;
  bool blnUpdateCanMFD = false;
  MCP_CAN CAN(pinCANShield);
  INT32U canId = 0x0;
  byte len = 0;
  byte buf[8];
  byte lvlVolCan=0x0e;
  unsigned int battVolt = 0;
#endif

#if additionalVolControl == 2 //CAN
  #define lvlStep 0x04
  #define maxDispLevel 99
  #define maxDispLevelToDigiPotRatio 2.576 // 255/maxDispLevel 2.576(for 0-99)
  #define volBarWidthToMaxVolRatio 1
  #define DispLevelToCANLevelRatio 3.3
  #define ExtVolControlDisplayName "CAN"
  #define ExtVolControlDisplayNameEnc "ENC"
  byte bytVolByCAN = 0;
#endif

#define minDispLevel 0

#if DisplayType < 3 //0-128x32 b&w, 1-128x64 ssd1306 .96" , 2-128x64 sh1106 1.3" 3-4 digit TM
  #include <Wire.h>
  #include <Adafruit_GFX.h>
  #if DisplayType == 2 //128x64 sh1106 1.3"
    #include <Adafruit_SH110X.h>
  #else                // ssd1306 .96"
    #include <Adafruit_SSD1306.h> 
  #endif
  
  #define posVolBigDigX 0
  #define posVolBigDigY 0
  #define posSubBigDigY 0
  #define posVolX 0
  #define posSubX 0
  #define SCREEN_WIDTH 128 // OLED display width, in pixels
  #define oledMaxBri 10

  #if DisplayType == 0 //OLED 128x32 b&w
  #else //OLED 128x64 b&w
    #define SCREEN_HEIGHT 64 //OLED display height, in pixels
    #define offsetBigDigX 3
    #define offsetBigDigY 11
    #define fontSizeBigDig 5
    #define posSubBigDigX 65
    #define posSwBigDigX 0
    #define posSwBigDigY 51
    #define posExtVolCtrlX 64
    #define posExtVolCtrlY 52
    #define posSwWidth 28//26 
    #define posSwHeight 11//9 odd numbers only
    #define posSwRadius 5//4 (posSwHeight-1)/2
    #define posVolBarH 19//16
    #define posVolY 0
    #define posSubY 25//21
    #define oledMidBri 9
    #define oledMinBri 7
    #define menuOledMaxItems 4 //count from 0
    #define posSubMenuX 0
    #define posSubMenuY 41

  #endif

  // Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
  #define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
  #define SCREEN_ADDRESS 0x3C
  #if DisplayType == 2 //128x64 1.3" sh1106
    Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
    #define INVERSE 2
    #define WHITE 1
    #define BLACK 0
  #else
    Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
  #endif
#endif

#if LDRenabled
  #define pinLDR A1
  #define minLDRBri 870 //i=(intLDRValue > minLDRBri)?2:1;
  #define maxLDRBri 250 //if (intLDRValue < maxLDRBri) {i=0;}
  int intLDRValue = 0;
#endif

char chrLblVol[] = "V";
char chrLblSub[] = "S";
char chrLblSwitch[] = "D";

#define eepromAddrblnSub 0 //which control is active by default
#define eepromAddrlvlSub 1 //default Sub level at start
#define eepromAddrLvlVol 2 //default vol level at start
#define eepromAddrBri 3    //default screen brightness level
#define eepromAddrBarType 4 // default Vol_Sub visual
#define eepromAddrVolBy 5 //vol by encoder or by CAN
#define eepromEncoderCCWRotation 6 //default encoder rotation
#define eepromblnSwitch 7 //which control is active by default


//menu
bool blnMenu = false;
bool blnSubMenuActive = false;
bool blnRefreshDisplay = false;
unsigned long timeMenuEnabled = 0;
#define intMenuTimeout 7000 //mseconds
byte intMenuItem = 0;
byte bytDisplayBri = 2;
#define autoOledBri 3
#define minOledBri 0
#define OledBriInterval 5000
#define GrayScaleType 1 // 0- light50% 1- dark75% 
//int blnTMBrightness = false;
bool blnMenuButton = false;

#if DisplayType < 3 //OLEDRenabled
  //char menuOledDigIn[] = "Digital In";
  char menuOledBri[10] = "Disp. bri";                   
  char menuOledVolBarType[13] = "Vol Bar Type";
  char menuOledVolCtrlBy[12] = "Vol Ctrl By";
  char menuOledSaveDefaults[14] = "Save Defaults";
  char menuOledReverseEncoder[12] = "Reverse Enc";
  #if additionalVolControl != 0
    char * arrMenuOled[] = {menuOledBri, menuOledVolBarType, menuOledVolCtrlBy, menuOledReverseEncoder, menuOledSaveDefaults};
    #define MenuOledItems 4 
  #else
    char * arrMenuOled[] = {menuOledBri, menuOledVolBarType, menuOledSaveDefaults, menuOledReverseEncoder};
    #define MenuOledItems 3
  #endif
  
  #if LDRenabled == 1
    #define MenuOledSubBriItems 3 //"Hi" "Mid" "Low" "Auto"
  #else
    #define MenuOledSubBriItems 2 //"Hi" "Mid" "Low"
  #endif
  #define MenuOledSubVolBarTypeItems 2 //"Bar1" "Bar2" "Dig"
  #define MenuOledSubVolBy 1 //"Ext" "Enc"
#endif

volatile byte aFlag = 0;
volatile byte bFlag = 0;
volatile byte encoderPos = 0x80;
volatile byte reading = 0;
bool blnEncoderCCWRotation = false;
#if defEncoderType == 0 //EC11
  void PinA(){
    cli();
    reading = PIND & 0xC;
    if(reading == B00001100 && aFlag) { encoderPos --; bFlag = 0; aFlag = 0;} else if (reading == B00000100) bFlag = 1;
    sei();
  }
  void PinB(){
    cli();
    reading = PIND & 0xC;
    if (reading == B00001100 && bFlag) { encoderPos ++; bFlag = 0; aFlag = 0;} else if (reading == B00001000) aFlag = 1;
    sei();
  }
#else //KY-040
  #include "EncoderStepCounter.h"
  #define ENCODER_INT1 digitalPinToInterrupt(pinEncA)
  #define ENCODER_INT2 digitalPinToInterrupt(pinEncB)
  EncoderStepCounter encoder(pinEncA, pinEncB, HALF_STEP);

  void interrupt() {
    encoder.tick();
  }
#endif

volatile byte lvlVol = 0; //start volume level
volatile byte lvlSub = 0; //start volume level
byte lvlVolOld = 0;
byte lvlSubOld = 0;
volatile int lvlTemp = 0x00;

bool blnSub = true; //set to 1 when we need to change subwoofer level

bool blnSwitch = false;
bool blnSwitchOld = true;
byte blnEncButtonState=1;
byte blnEncButtonStatePrev=1;

unsigned long timeCurrent = 0;
unsigned long timeLDRRead = 0;
unsigned long timeButtonPressed = 0;
unsigned long timeButtonReleasedLastTime = 0;
unsigned long timeButtonPressedDuration = 0;

byte buttonPressedCount=0;

#define addrVol 0x12
#define addrSub 0x11
#define timeButtonWaitForInput 250
#define timeButtonDebounce 30
#define timeShortButtonPress 250

byte bytVisualType=0;

byte i = 0;
byte j = 0;

void setup() {    
  Serial.begin(9600);
  #if VolControlType == 0      //SPI MCP
      SPI.begin();
      pinMode(pinSPIDigPotCS, OUTPUT);
      pinMode(pinDigitalSwOut, OUTPUT);
  #endif
  pinMode(pinEncButton, INPUT_PULLUP);
  #if defEncoderType == 0 //EC11
    pinMode(pinEncA, INPUT_PULLUP); 
    pinMode(pinEncB, INPUT_PULLUP); 
    attachInterrupt(0,PinA,RISING);
    attachInterrupt(1,PinB,RISING);
  #else //KY-040
    encoder.begin();
    attachInterrupt(ENCODER_INT1, interrupt, CHANGE);
    attachInterrupt(ENCODER_INT2, interrupt, CHANGE);
  #endif

  blnSub = EEPROM.read(eepromAddrblnSub);
  lvlSub = checkVolLevel(EEPROM.read(eepromAddrlvlSub));
  lvlSubOld = lvlSub;
  lvlVol = checkVolLevel(EEPROM.read(eepromAddrLvlVol));
  lvlVolOld = lvlVol;
  bytDisplayBri = checkValueMinMax(EEPROM.read(eepromAddrBri),minOledBri,MenuOledSubBriItems);
  bytVisualType = EEPROM.read(eepromAddrBarType);
  if (bytVisualType>MenuOledSubVolBarTypeItems) {bytVisualType=0;}

  #if additionalVolControl != 0
    bytVolByCAN = EEPROM.read(eepromAddrVolBy);
    if (bytVolByCAN>MenuOledSubVolBy) {bytVolByCAN=0;}
  #endif

  blnEncoderCCWRotation = EEPROM.read(eepromEncoderCCWRotation);
  blnSwitch = EEPROM.read(eepromblnSwitch);
  blnSwitchOld = blnSwitch;
  SetVolume(lvlVol,0);
  SetVolume(lvlSub,1);
  #if VolControlType == 0      //Helix SPI MCP
    digitalWrite(pinDigitalSwOut, blnSwitch);
  #endif
  
  #if DisplayType == 2 //128x64 sh1106 1.3"
    //delay(250); // wait for the OLED to power up
    display.begin(SCREEN_ADDRESS, true);
  #else
    display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  #endif

  setOledBri(bytDisplayBri);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextColor(WHITE);

  #if additionalVolControl == 2 // External Vol Control By: 0-none, 1-UART, 2-CAN //#if CANEnabled == 1
    START_CANBUS_INIT:
    if(CAN_OK == CAN.begin(defCanSpeed,MCP_8MHz)) {
      #if defCanVolControlByCarModel == 1 //1- MB C-class 2007-2015(204)
        CAN.init_Mask(1, 1, 0x07ff);
        CAN.init_Filt(2, 1, canidWheelButton);//volume from head unit
        CAN.init_Mask(0, 0, 0x07ff);
        CAN.init_Filt(0, 0, canidWheelButton);
      #endif
    } else {
      if (i <= intCANShieldInitRetry) {
        i++;
        delay(300);
        goto START_CANBUS_INIT;
      } else {
        blnCANinitFailed = true;
        display.print("CAN fail");
        display.display(); 
        delay(1000);  
      }
    }
  #endif
  DisplayLevelsOled();
}

void SetVolume(int lvl, byte bytVolType) {
  lvlTemp = lvl * maxDispLevelToDigiPotRatio; //digipot works with 0-255 
  #if VolControlType == 0     //SPI MCP
        digitalWrite(pinSPIDigPotCS, LOW);
        SPI.transfer((bytVolType)?addrSub:addrVol);
        SPI.transfer(lvlTemp);
        digitalWrite(pinSPIDigPotCS, HIGH);
  #endif
}

void DisplayVolBar(byte posX, byte posY, char lbl[], byte lvlVol, byte barType, byte bytColor) {
  display.fillRect(posX, posY, SCREEN_WIDTH, posVolBarH, BLACK);
  display.setCursor(posX+114, posY+2);
  FormatDigit(lvlVol);
  display.setCursor(posX, posY+2);
  display.print(lbl);
  if (barType == 0) {
    display.fillRect(posX+10, posY, lvlVol*volBarWidthToMaxVolRatio, posVolBarH, WHITE);
  } else {
    display.fillTriangle(posX+10, posY+posVolBarH-1, posX+9+(maxDispLevel*volBarWidthToMaxVolRatio), posY-1, posX+9+(maxDispLevel*volBarWidthToMaxVolRatio), posY+posVolBarH-1, WHITE);
    display.fillRect(posX+10+(lvlVol*volBarWidthToMaxVolRatio), posY-1, ((maxDispLevel-lvlVol)*volBarWidthToMaxVolRatio)+1, posVolBarH+1, BLACK);
    for (i=2;i<(lvlVol*volBarWidthToMaxVolRatio);i=i+4){
      display.fillRect(posX+10+i, posY-1, 2, posVolBarH+1, BLACK);
    }
  }
  if (bytColor) {
    grayout(posX+10,posY,(lvlVol*volBarWidthToMaxVolRatio)+11,posY+posVolBarH,barType);
  }
}

void grayout(byte x, byte y, byte w, byte h, byte force50percent) {
  bool blnOffset=false;
  bool blnLine=true;
    for (i=x;i<w;i=i+2) {
      for (j=y;j<h;j=j+2) {
        if(j==y) {display.drawPixel(i+1, j, BLACK);}
        display.drawPixel(i, j+1, BLACK);
        if ((GrayScaleType == 1) && (force50percent == 0)) {
          if (blnLine) {display.drawLine(i+1,j,i+1,j+2,BLACK);}
          blnLine=!blnLine; 
        } else {
          display.drawPixel(i+1, j, BLACK);
        }
      }
      blnLine = blnOffset;
      blnOffset =!blnOffset;
    }
}

void FormatDigit(byte lvlVol) {
  display.print(lvlVol<10?" ":"");
  display.print(lvlVol,DEC);
}

void DisplayBigDig(byte posX, byte posY, char lbl[], byte lvlVol, byte bytColor) {
  display.fillRect(posX,posY, posSubBigDigX-1, offsetBigDigY+(fontSizeBigDig*7),BLACK);
  display.setCursor(posX,posY);
  display.print(lbl);
  display.setCursor(posX+offsetBigDigX,posY+offsetBigDigY);
  display.setTextSize(fontSizeBigDig); //35x49x7 30x42x6 25x35x5 20x28x4; 15x21x3; 10x14x2; 5+1x7+1
  FormatDigit(lvlVol);
  if (bytColor) {
    grayout(posX+offsetBigDigX,posY+offsetBigDigY,display.getCursorX(),(fontSizeBigDig*7)+posY+offsetBigDigY,0);
  }
}

#if DisplayType == 0 //128x32
#else //128x64
void DisplaySW(byte posX, byte posY, char lbl[], byte state) {
   display.fillRect(posX,posY-1,posSwWidth+11+2,posSwHeight+2,BLACK);
   display.setCursor(posX,posY+1);
   display.print(lbl);
   if (state == 0) {
     display.drawRoundRect(posX+11,posY,posSwWidth,posSwHeight,posSwRadius,WHITE);
     display.fillCircle(posX+11+posSwRadius,posY+posSwRadius,posSwRadius+1,WHITE);
     display.drawCircle(posX+11+posSwRadius,posY+posSwRadius,posSwRadius+2,BLACK);
   } else {
     display.fillRoundRect(posX+11,posY,posSwWidth,posSwHeight,posSwRadius,WHITE);
     display.drawRoundRect(posX+11+1,posY+1,posSwWidth-2,posSwHeight-2,posSwRadius-1,BLACK);
     display.fillCircle(posX+11+posSwWidth-posSwRadius,posY+posSwRadius,posSwRadius+1,WHITE); 
     display.drawCircle(posX+11+posSwWidth-posSwRadius,posY+posSwRadius,posSwRadius+2,BLACK);
   }
}
#endif

void DisplayVolume(byte lvlVol, byte bytGage,byte dispType) {
  display.setTextSize(1); //1 - 7x5
  display.setTextColor(WHITE);
    switch (bytGage) {
      case 0: //vol
        if (dispType < 2) {     
          DisplayVolBar(posVolX,posVolY,chrLblVol,lvlVol,dispType,(blnSub)?1:0);
        }else {
          DisplayBigDig(posVolBigDigX, posVolBigDigY, chrLblVol, lvlVol,(blnSub)?1:0);
        }
      break;
      case 1: //subwoofer
        if (dispType < 2) {     
          DisplayVolBar(posSubX,posSubY,chrLblSub,lvlVol,dispType,(blnSub)?0:1);
        }else {
          DisplayBigDig(posSubBigDigX, posSubBigDigY, chrLblSub, lvlVol,(blnSub)?0:1);
        }
      break;
      case 2: //switch
          #if DisplayType == 0 //128x32
          #else //128x64
            DisplaySW(posSwBigDigX, posSwBigDigY, chrLblSwitch, lvlVol);
          #endif
      break;
      #if additionalVolControl != 0 // 0-none, 1-UART, 2-CAN
      case 3: //ext vol ctrl
        display.setCursor(posExtVolCtrlX,posExtVolCtrlY);
        display.print("CTRL: ");
        if (lvlVol) {
          display.print(ExtVolControlDisplayNameEnc);
        } else {
          display.print(ExtVolControlDisplayName);
        }
      break;
      #endif
    }
    display.display();
}

void DisplayMenuOled(byte selItem) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  if (selItem <= menuOledMaxItems){ //1
    j = 0;
  } else {
    j = (selItem >= MenuOledItems)?(MenuOledItems-menuOledMaxItems):(selItem-1); //2
  }
  for (i=j;i<=j+menuOledMaxItems;i=i+1){  //2
    display.print(i==selItem?"> ":"  ");
    display.println(arrMenuOled[i]);  
  }  
  display.setCursor(SCREEN_WIDTH-27,SCREEN_HEIGHT-7);
  display.print(softVer);
  display.display();
}

void DisplaySubMenuHighLightSelected(byte item,byte selItem,char *txtItem) {
  byte prevItemStart;
  byte currItemStart;
    if (selItem == item) {
      display.setTextColor(BLACK,WHITE);
    } else {
      display.setTextColor(WHITE,BLACK);
    }  
    prevItemStart = display.getCursorX();
    display.print(txtItem);
    currItemStart = display.getCursorX();
    if (selItem == item) {
      display.drawRect(prevItemStart-1,posSubMenuY+1, currItemStart-prevItemStart+1, 10, WHITE);
    }
    display.setCursor(currItemStart+7,posSubMenuY+2);  
}

void DisplaySubMenuOled(byte selItem, byte subMenuType) {
  display.setTextSize(1);
  display.fillRect(posSubMenuX,posSubMenuY+1,SCREEN_WIDTH, 10,BLACK); //cleaning background for submenu
  display.setCursor(posSubMenuX+2,posSubMenuY+2);

  switch (subMenuType) {
    case 0://brightness
      DisplaySubMenuHighLightSelected(0, selItem, "Hi");
      DisplaySubMenuHighLightSelected(1, selItem, "Mid");
      DisplaySubMenuHighLightSelected(2, selItem, "Low");
      DisplaySubMenuHighLightSelected(3, selItem, "Auto");
    break;
    case 1://vol bar type
      DisplaySubMenuHighLightSelected(0, selItem, "Bar1");
      DisplaySubMenuHighLightSelected(1, selItem, "Bar2");
      DisplaySubMenuHighLightSelected(2, selItem, "Dig");
    break;
    #if additionalVolControl != 0 // 0-none, 1-UART, 2-CAN
    case 2://vol By bytVolByCAN
      DisplaySubMenuHighLightSelected(0, selItem, ExtVolControlDisplayName);
      DisplaySubMenuHighLightSelected(1, selItem, ExtVolControlDisplayNameEnc);
    break;
    #endif
  }
  display.setTextColor(WHITE);
  display.display();
}

void DisplayLevelsOled() {
  display.clearDisplay();
  DisplayVolume(lvlSub,1,bytVisualType);
  DisplayVolume(lvlVol,0,bytVisualType);
  DisplayVolume(blnSwitch,2,bytVisualType);  
  #if additionalVolControl != 0
    DisplayVolume(bytVolByCAN,3,bytVisualType);  
  #endif
}

#if additionalVolControl == 2 // External Vol Control By: 0-none, 1-UART, 2-CAN//#if CANEnabled == 1
void writeToCan(INT32U canId, byte len, byte b0, byte b1,byte b2, byte b3,byte b4, byte b5,byte b6, byte b7) {
  byte buf[8];
    buf[0]=b0; buf[1]=b1; buf[2]=b2; buf[3]=b3; buf[4]=b4; buf[5]=b5; buf[6]=b6; buf[7]=b7;
    CAN.sendMsgBuf(canId, 1, len, buf);
    delay(50);
}
#endif

void loop(){  
  if (lvlVolOld != lvlVol) {
    SetVolume(lvlVol,0);
    if (!blnMenu) {
      DisplayVolume(lvlVol,0,bytVisualType);
    }
    lvlVolOld = lvlVol;
    #if additionalVolControl == 2 // External Vol Control By: 0-none, 1-UART, 2-CAN//#if CANEnabled == 1
      blnUpdateCanMFD = true;
    #endif
  }
  if (lvlSubOld != lvlSub) {
    SetVolume(lvlSub,1);
    DisplayVolume(lvlSub,1,bytVisualType);
    lvlSubOld = lvlSub;
    #if additionalVolControl == 2 // External Vol Control By: 0-none, 1-UART, 2-CAN//#if CANEnabled == 1
      blnUpdateCanMFD = true;
    #endif
  }
  if (blnSwitch != blnSwitchOld) {
    digitalWrite(pinDigitalSwOut, blnSwitch);
    DisplayVolume(blnSwitch,2,bytVisualType);
    blnSwitchOld = blnSwitch;
    #if additionalVolControl == 2 // External Vol Control By: 0-none, 1-UART, 2-CAN//#if CANEnabled == 1
      blnUpdateCanMFD = true;
    #endif
  }
  timeCurrent = millis();
  
  #if LDRenabled
  if (bytDisplayBri == autoOledBri) {
    if ((timeCurrent-timeLDRRead) > OledBriInterval) {
      timeLDRRead = timeCurrent;
      intLDRValue = analogRead(pinLDR);
      i=(intLDRValue > minLDRBri)?2:1;
      if (intLDRValue < maxLDRBri) {i=0;}
      setOledBri(i);
    }
  }
  #endif

  if (blnMenu) {
    if (blnMenuButton) {
      blnRefreshDisplay = true;
      if (blnSubMenuActive) {blnMenu=false;}
      if (arrMenuOled[intMenuItem] == menuOledBri) {
        if (blnSubMenuActive) {
          EEPROM.update(eepromAddrBri, bytDisplayBri);
        }
        blnSubMenuActive = !blnSubMenuActive;
      }
      if (arrMenuOled[intMenuItem] == menuOledVolBarType) {
        if (blnSubMenuActive) {
          EEPROM.update(eepromAddrBarType, bytVisualType);
        }
        blnSubMenuActive = !blnSubMenuActive;
      }
      if (arrMenuOled[intMenuItem] == menuOledReverseEncoder) {
        blnEncoderCCWRotation = !blnEncoderCCWRotation;
        EEPROM.update(eepromEncoderCCWRotation, blnEncoderCCWRotation);
        blnMenu = false;
        blnSubMenuActive = false;
      }
      #if additionalVolControl != 0   
        if (arrMenuOled[intMenuItem] == menuOledVolCtrlBy) {
          blnSubMenuActive = !blnSubMenuActive;
        }
      #endif
      if (arrMenuOled[intMenuItem] == menuOledSaveDefaults) { //Save all defaults
          EEPROM.update(eepromAddrblnSub, blnSub);
          EEPROM.update(eepromAddrlvlSub, lvlSub);
          EEPROM.update(eepromAddrLvlVol, lvlVol);
          EEPROM.update(eepromblnSwitch, blnSwitch);
          blnMenu = false;
          blnSubMenuActive = false;
      }
      blnMenuButton = false;
    }

    if ((timeCurrent - timeMenuEnabled) > intMenuTimeout) { //Exit Menu By Timeout
      blnMenu = false;
      blnSubMenuActive = false;
      intMenuItem = 0;
      blnRefreshDisplay = true;
    }
  }

  if (blnRefreshDisplay) {
    if (blnMenu) {
      if (blnSubMenuActive){
        if (arrMenuOled[intMenuItem] == menuOledBri) {       DisplaySubMenuOled(bytDisplayBri,0);}
        if (arrMenuOled[intMenuItem] == menuOledVolBarType) {DisplaySubMenuOled(bytVisualType,1);}
        #if additionalVolControl != 0   
          if (arrMenuOled[intMenuItem] == menuOledVolCtrlBy) { DisplaySubMenuOled(bytVolByCAN     ,2);}   
        #endif
      } else {
        DisplayMenuOled(intMenuItem);
      }
    } else {
      DisplayLevelsOled();
    }
    blnRefreshDisplay = false;
  }

  blnEncButtonState = digitalRead(pinEncButton);

  if (blnEncButtonStatePrev != blnEncButtonState) {
    if (blnEncButtonStatePrev > blnEncButtonState) {
      timeButtonPressed = timeCurrent;
    } else {
      timeButtonReleasedLastTime = timeCurrent;
      if ((timeCurrent-timeButtonPressed) > timeButtonDebounce) {
        buttonPressedCount++;
        timeButtonPressedDuration = timeCurrent-timeButtonPressed;
      }
    }
  }

  if ((buttonPressedCount>=2)||((buttonPressedCount > 0)&&((timeCurrent-timeButtonReleasedLastTime) > timeButtonWaitForInput))) {
      if (buttonPressedCount == 1) {
        if (timeButtonPressedDuration < timeShortButtonPress) { //short enc button press
          if (blnMenu) {
            blnMenuButton = true;
          } else {
            blnRefreshDisplay = true;
            blnSub = !blnSub;
          }
        } else { //long enc button press
          blnMenu = true;
          blnRefreshDisplay = true;
          timeMenuEnabled = timeCurrent;
        }
      } else { //double and more clicks
        blnSwitch = !blnSwitch;
      }
      buttonPressedCount = 0;
  }
  blnEncButtonStatePrev = blnEncButtonState;

  #if additionalVolControl == 2 // External Vol Control By: 0-none, 1-UART, 2-CAN//#if CANEnabled == 1
    if (!blnCANinitFailed) { 
      if (CAN_MSGAVAIL == CAN.checkReceive()) {
        CAN.readMsgBuf(&len, buf);
        canId = CAN.getCanId();
        if (!bytVolByCAN) {
          lvlTemp = 0;
          #if canVolControlBy == 1 // 0 -by vol status from HU, 1 -by wheel button
            if (canId == canidWheelButton) { //wheelbuttons
              //DEBUG++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
              //display.setCursor(0, 55);
              //display.fillRect(0, 55, 18, 7, BLACK);
              //display.print("#");
              //display.print(buf[4],HEX);
              //display.display(); 
              //DEBUG++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                #if defCanVolControlByCarModel == 1 //1- MB C-class 2007-2015(204)
                  if (buf[4] == 0x10) { //vol+
                    lvlTemp = 1;
                  }
                  if (buf[4] == 0x20) { //vol-
                    lvlTemp = -1;
                  }
                  #if defProgressiveLvlStep == 0 //0- lvlStep constant used, 1 - ProgressiveVolStep used <60 - 4 steps, <90 - 2, >90 -1
                    lvlVol = checkVolLevel(lvlVol + (lvlTemp*lvlStep));
                  #else
                    lvlVol = checkVolLevel(lvlVol + (lvlTemp*ProgressiveVolStep(lvlVol)));
                  #endif
                #endif
            }
          #endif
        }
      }
    }
  #endif

  #if defEncoderType == 1 //KY-040
    signed char pos = encoder.getPosition();
    if (pos != 0) {
      encoderPos += pos;
      encoder.reset();
    }
  #endif

  if(encoderPos != 0x80) {
    lvlTemp = (encoderPos - 0x80);
    if (blnEncoderCCWRotation) {
      lvlTemp = -lvlTemp;
    }
    encoderPos = 0x80;
    if (blnMenu) {
      blnRefreshDisplay = true;
      if (blnSubMenuActive){
        if (arrMenuOled[intMenuItem] == menuOledBri) {
          bytDisplayBri =checkValueMinMax(bytDisplayBri + lvlTemp,minOledBri,MenuOledSubBriItems);
          if (bytDisplayBri < 3) {
            setOledBri(bytDisplayBri);
          }
        }
        if (arrMenuOled[intMenuItem] == menuOledVolBarType) { bytVisualType = checkValueMinMax(bytVisualType + lvlTemp,0,2);}
        if (arrMenuOled[intMenuItem] == menuOledVolCtrlBy)  { 
          #if additionalVolControl != 0 // 0-none, 1-UART, 2-CAN
            bytVolByCAN = checkValueMinMax(bytVolByCAN + lvlTemp,0,MenuOledSubVolBy);
            EEPROM.update(eepromAddrVolBy, bytVolByCAN);
          #endif 
        }
      } else {
        intMenuItem = checkValueMinMax(intMenuItem + lvlTemp,0,MenuOledItems);
      }
      timeMenuEnabled = timeCurrent;
    } else {
      if (blnSub) {
        #if defProgressiveLvlStep == 0 //0- lvlStep constant used, 1 - ProgressiveVolStep used <60 - 4 steps, <90 - 2, >90 -1
          lvlSub = checkVolLevel(lvlSub + (lvlTemp*lvlStep));
        #else
          lvlSub = checkVolLevel(lvlSub + (lvlTemp*ProgressiveVolStep(lvlSub)));
        #endif

      } else {
        #if defProgressiveLvlStep == 0 //0- lvlStep constant used, 1 - ProgressiveVolStep used <60 - 4 steps, <90 - 2, >90 -1
          lvlVol = checkVolLevel(lvlVol + (lvlTemp*lvlStep));
        #else
          lvlVol = checkVolLevel(lvlVol + (lvlTemp*ProgressiveVolStep(lvlVol)));
        #endif
      }
    }
  }
} //emd loop ########################################################################################################################################

void setOledBri(byte dimLevel) {
  byte dimContrast = 0xFF;
  byte dimPrecharge = 0xF1;

  switch (dimLevel) {
    case 1://Mid
      #if DisplayType == 2 //128x64 sh1106 1.3"
        dimContrast = 0x80;
      #else
        dimContrast = 0x01;
      #endif
    break;
    case 2://Low
      #if DisplayType == 2 //128x64 sh1106 1.3"
        dimPrecharge = 0x01;
      #else
        dimPrecharge = 0x20;//0x10;
      #endif
      dimContrast = 0x01;
    break;
  }
  #if DisplayType == 2 //128x64 sh1106 1.3"
    display.oled_command(SH110X_SETCONTRAST);
    display.oled_command(dimContrast);
    display.oled_command(SH110X_SETPRECHARGE);
    display.oled_command(dimPrecharge);
  #else
    display.ssd1306_command(SSD1306_SETCONTRAST);//0x81
    display.ssd1306_command(dimContrast);
    display.ssd1306_command(SSD1306_SETPRECHARGE);//0xD9
    display.ssd1306_command(dimPrecharge);
  #endif
}

byte checkValueMinMax(int val, int min, int max) {
  if (val < min) return min;
  if (val > max) return max;
  return val;
}

byte checkVolLevel(int lvl) {
      if (lvl < minDispLevel) return minDispLevel;
      if (lvl > maxDispLevel) return maxDispLevel;
      //if (lvl > 94) return 96;      
      return lvl;
}

#if defProgressiveLvlStep == 1 //0- lvlStep constant used, 1 - ProgressiveVolStep used <60 - 4 steps, <90 - 2, >90 -1
  byte ProgressiveVolStep(int lvl) {
    if (lvl < 60) return 4;
    if (lvl < 90) return 2;
    return 1;
  }
#endif

#if additionalVolControl == 2 // External Vol Control By: 0-none, 1-UART, 2-CAN//#if CANEnabled == 1
  byte splitDigitAny(unsigned int lvl, byte pos) {
    unsigned int mult=1;
    if (lvl == 0) {
      return 0x30;
    } else {
      for (i=1;i<pos;i=i+1){
        mult = mult * 10;
      }
      if (lvl < mult) {
        return 0x20;
      } else {
        return 0x30+((lvl / mult) % 10);
      }
    }
  }
#endif

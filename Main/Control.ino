/*
 * Martabak Duino
 * 
 * Alvin Pratama Tamin
 * Reivin Oktavianus
 * Rudy Nurhadi
 * Tomi
 * 
 */
#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>
#include <Keypad.h>

#define RST_PIN 9
#define SS_PIN 10

byte masterCard[4];
byte readCard[4];
byte storedCard[4];

int pins[10];
char keypin;
int a = 0;

int successRead;
boolean program = false;
boolean match = false;

const byte ROWS = 4; 
const byte COLS = 3; 
char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
byte rowPins[ROWS] = {5, 4, 3, 2}; 
byte colPins[COLS] = {8, 7, 6}; 
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

MFRC522 mfrc522(SS_PIN, RST_PIN);

MFRC522::MIFARE_Key key;

void setup() 
{
  Serial.begin(9600);
  while (!Serial);
  SPI.begin();
  mfrc522.PCD_Init();
  keypad.addEventListener(keypadEvent);

  Serial.println(F("Martabak Duino Safety Box v3.22\n"));
   /*for(int x = 0; x < EEPROM.length(); x = x + 1)
     {    
        if(EEPROM.read(x) == 0) 
        {              
        }
        else 
        {
          EEPROM.write(x, 0);       
        }
      }*/
  if(EEPROM.read(1) != 127)
  {      
    Serial.println(F("Scan A PICC to Define as Master Card"));
      do 
      {
        successRead = getID();            
      }
    while(!successRead);                  
    for(int j = 0; j < 4; j++) 
    {       
      EEPROM.write(2 + j,readCard[j]);
    }
    EEPROM.write(1, 127);      
    Serial.println(F("Master Card Defined"));
  }
  Serial.println(F("-------------------------------------------------"));
  Serial.println(F("Master Card's UID"));
  for(int i = 0; i < 4; i++) 
  {          
    masterCard[i] = EEPROM.read(2 + i);
    Serial.print(masterCard[i], HEX);
  }
  Serial.println("");
  Serial.println(F("-------------------------------------------------"));
}

void loop() 
{
 do
 {
    successRead = getID(); 
    keypin = keypad.getKey();
    if(keypin == '#')
    {
      memset(readCard,0,sizeof(readCard));
      successRead = 1;
    }
 }  while(successRead != 1);
  if(program) 
  {
      if(isMaster(readCard)) 
      {
        Serial.println(F("Master Card Scanned"));
        Serial.println(F("Exiting Program Mode"));
        program = false;
        memset(readCard,0,sizeof(readCard));
        return;
      }  
    else if(keypin == '#')
    {
      Serial.println(F("Input your new pin"));
      inputPin();
      program = false;
      return;
    }
    else
    {
        writeID(readCard);
    }
  }
  else 
  {
    if(isMaster(readCard)) 
    {
      program = true;
      Serial.println(F("Entered Program Mode"));
      Serial.println(F("Scan a new RFID Card / enter new pin by pressing '#'"));
    }
    else 
    {
      if(findID(readCard)) 
      {
        Serial.println(F("Thou shalt pass."));
      }
      else if(checkPin() == true)
      {
        memset(pins, 0, sizeof(pins));
        Serial.println(F("Thou shalt pass."));
      }
      else 
      {
        Serial.println(F("Thou shalt not pass"));
      }
    }
  }
}  

int getID() 
{
  if (!mfrc522.PICC_IsNewCardPresent()) 
  {
    return 0;
  }
  if (!mfrc522.PICC_ReadCardSerial()) 
  {
    return 0;
  }
  Serial.println(F("Scanned PICC's UID:"));
  for (int i = 0; i < 4; i++) 
  { 
    readCard[i] = mfrc522.uid.uidByte[i];
    Serial.print(readCard[i], HEX);
  }
  Serial.println("");
  mfrc522.PICC_HaltA();
  return 1;
}

void writeID(byte a[]) 
{
  if (!findID(a)) 
  {    
    int num = EEPROM.read(0);     
    int start = (num*4)+6; 
    num++;               
    EEPROM.write(0,num);     
    for(int j = 0; j < 4; j++) 
    {   
      EEPROM.write(start + j,a[j]);
    }
  Serial.println(F("Succesfully added RFID Card"));
  }
  else 
  {
  Serial.println(F("Failed! RFID Card already registered"));
  }
}

boolean isMaster(byte test[]) 
{
  if(checkTwo(test,masterCard))
    return true;
  else
    return false;
}

void readID(int number) 
{
  int start = (number*4) + 2;    
  for(int i = 0; i < 4; i++) 
  { 
    storedCard[i] = EEPROM.read(start + i);   
  }
}

boolean findID(byte find[]) 
{
  int count = EEPROM.read(0);      
  for(int i=1; i<=count; i++) 
  {    
    readID(i);          
    if(checkTwo(find,storedCard)) 
    {   
      return true;
      break;  
    }
    else 
    {
    }
  }
  return false;
}

boolean checkTwo(byte a[],byte b[]) 
{
  if(a[0] != NULL)
  {       
    match = true;       
  }
  for(int k = 0; k < 4; k++) 
  {   
    if(a[k] != b[k])
    {
      match = false;
    }
  }
  if(match) 
  {      
    return true;      
  }
  else  
  {
    return false;       
  }
}

void inputPin()
{
  for(int i=0; i<sizeof(pins); i++)
  {
    char key;
    do
    {
        key = keypad.getKey();
    }while(!key);
    if(key == '#')
    {
      EEPROM.write(449, i);
      keypadWrite(i);
      a = 0;
      return;
    }
    else
    {
      pins[i] = key;
    }
  }
  Serial.println(F("Wrong Input"));
  return;
}

void keypadWrite(int i)
{
  for(int j=0; j<i; j++)
  {
    Serial.println(pins[j]-48);
    EEPROM.write(450+j, pins[j]); 
  }
  Serial.println(F("Pin registered"));
  memset(pins, 0, sizeof(pins));
  return;
}

boolean checkPin()
{
  int pinLength = EEPROM.read(449);
  for(int i=0; i<a; i++)
  {
    if(pins[i] != EEPROM.read(450+i))
    {
      a = 0;
      return false;
    }
  }
  a = 0;
  return true;
}

void keypadEvent(KeypadEvent key)
{
  if(keypad.getState() == PRESSED)
  {
    if(key != '#')
    {
      pins[a] = key;
      a++;
    }
  }
}
      



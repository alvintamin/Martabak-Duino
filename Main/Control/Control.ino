/*
 * Martabak Duino
 *
 * Alvin Pratama Tamin
 * Reivin Oktavianus
 * Rudy Nurhadi
 * Tomi
 *
 * WIRING GUIDE
 *
 * ---MFRC 522---
 *
 * PIN RST = 5
 * PIN MISO = 50
 * PIN MOSI = 51
 * PIN SCK = 52
 * PIN SDA = 53
 *
 * ---KEYPAD---
 * ROW PIN = 5, 4, 3, 2
 * COLUMN PIN = 8, 7, 6
 *
 * --SOLENOID--
 * PIN = A0
 *
 *---BUZZER---
 *PIN = 10
 *
 *---PIEZO---
 *PIN = A1
 *
 */
#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>
#include <Keypad.h>

#define RST_PIN 5
#define SS_PIN 53
#define NOTE_E7 2637
#define NOTE_C7 2093
#define NOTE_G7 3136
#define NOTE_G6 1568
 
byte masterCard[4];
byte readCard[4];
byte storedCard[4];

int pins[10];
char keypin;
int song = 0;
int a = 0; // global variable counter pin bit
int b = 0; // global variable counter pin tries

int successRead;
boolean program = false;
boolean match = false;
boolean rfidRead = false;

unsigned long solenoidUnlockTime = 0;
int solenoidLock = A0;

int buzzerPin = 10;
int piezoPin = A1;

int threshold = 400; // batas bawah ketukan dari analog sinyal
// intro mario theme song
int melody[] = {NOTE_E7, NOTE_E7, 0, NOTE_E7, 0, NOTE_C7, NOTE_E7, 0, NOTE_G7, 0, 0,  0, 0, 0, 0, 0};

// tempo mario song
int tempo[] = {12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12};

// knock rahasia yang digunakan untuk reset
const int maxKnocks = 20;
byte secretCode[maxKnocks] = {100, 100, 50, 0, 100, 100, 50, 0, 0, 100, 50, 100, 100, 50, 150, 150, 0, 0, 0, 0}; 
int knockReadings[maxKnocks]; 
const int knockComplete = 1500;
const int rejectValue = 25; 

const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};
byte rowPins[ROWS] = {5, 4, 3, 2};
byte colPins[COLS] = {8, 7, 6};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

MFRC522 mfrc522(SS_PIN, RST_PIN);

MFRC522::MIFARE_Key key;

void setup()
{
  pinMode(A0, OUTPUT); //solenoid pin
  pinMode(buzzerPin, OUTPUT); // buzzer pin output
  sing();
  Serial.begin(9600);
  SPI.begin();
  mfrc522.PCD_Init();
  keypad.addEventListener(keypadEvent); // menambahkan interrupt keypad ke system
  attachInterrupt(digitalPinToInterrupt(piezoPin), masterReset, CHANGE); // program master reset dalam bentuk interrupt system

  Serial.println(F("Martabak Duino Safety Box v3.22\n"));
  if (EEPROM.read(1) != 127) // cek magic value yang apakah ada mastercard pada EEPROM
  {
    Serial.println(F("Scan A PICC to Define as Master Card"));
    do
    {
      successRead = getID();
    } while (!successRead);
    for (int j = 0; j < 4; j++)
    {
      EEPROM.write(2 + j, readCard[j]); // menyimpan UID mastercard ke eeprom
    }
    EEPROM.write(1, 127); //set magic value di EEPROM 1 dengan value 127
    EEPROM.write(449, 4); // set default jumlah pin untuk safety box dengan pin 1234
    EEPROM.write(450, 49);
    EEPROM.write(451, 50);
    EEPROM.write(452, 51);
    EEPROM.write(453, 52);
    Serial.println(F("Master Card Defined"));
  }
  Serial.println(F("-------------------------------------------------"));
  Serial.println(F("Master Card's UID"));
  for (int i = 0; i < 4; i++)
  {
    masterCard[i] = EEPROM.read(2 + i); //membacakan kembali UID mastercard
    Serial.print(masterCard[i], HEX);
  }
  Serial.println("");
  Serial.println(F("-------------------------------------------------"));
}

void loop()
{
  do
  {
    if(analogRead(piezoPin) > threshold)
    {
      masterReset();
    }
    keypad.getKey();
    successRead = getID();
    if ((millis() - solenoidUnlockTime) > 2000) {
      digitalWrite(solenoidLock, LOW);
    }
  } while (successRead != 1);
  if (program) // program mode
  {
    if (isMaster(readCard)) // cek apakah kartu tersebut adalah mastercard
    {
      beep(300);
      beep(300);
      beep(300);
      beep(300);
      beep(300);
      Serial.println(F("Master Card Scanned"));
      Serial.println(F("Exiting Program Mode"));
      program = false;
      memset(readCard, 0, sizeof(readCard));
      return;
    }
    /*else if (keypin == '#') // input # untuk record pin baru
    {
      Serial.println(F("Input your new pin"));
      inputPin();
      program = false;
      return;
    }*/
    else
    {
      if (findID(readCard)) // RFID ditemukan, menghapus RFID tersebut
      {
        Serial.println(F("Removing this card..."));
        deleteID(readCard);
      }
      else // RFID tidak ditemukan, menambahkan RFID tersebut
      {
        Serial.println(F("Adding this card..."));
        writeID(readCard);
      }
    }
  }
  else
  {
    if (isMaster(readCard)) // cek apakah kartu merupakan mastercard
    {
      program = true; // set boolean program menjadi true, memasuki program mode
      Serial.println(F("Entered Program Mode"));
      Serial.println(F("Scan RFID Card / enter new pin by pressing '#'"));
      beep(300);
      beep(300);
      beep(300);
    }
    else
    {
      if (findID(readCard)) // cek kebenaran kartu
      {
        beep(3000);
        beep(3000);
        rfidRead = true;
        memset(pins, 0, sizeof(pins)/sizeof(int));
        Serial.println(F("This is the correct RFID. Enter your pin to procced."));
      }
      /*else if (checkPin() == true) // cek kebanaran pin
      {
        memset(pins, 0, sizeof(pins)/sizeof(int));
        digitalWrite(A0, HIGH);
        solenoidUnlockTime = millis();
        beep(3000);
        beep(3000);
        Serial.println(F("Thou shalt pass."));
      }*/
      else // jika tidak ada yang benar
      {
        beep(3000);
        Serial.println(F("Thou shalt not pass"));
      }
    }
  }
}

/*-----------------------------------------------------------------------------------*/

int getID() // fungsi untuk membaca UID dari RFID Reader
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

/*-----------------------------------------------------------------------------------*/

void writeID(byte a[]) //fungsi untuk memasukkan RFID baru ke dalam EEPROM
{
  if (!findID(a)) //mencari ID tersebut apakah sudah terdaftar
  {
    int num = EEPROM.read(0); //membaca jumlah RFID yang sudah terdaftar
    int start = (num * 4) + 6; //menentukan posisi start untuk EEPROM yang belum terisi
    num++;
    EEPROM.write(0, num);
    for (int j = 0; j < 4; j++)
    {
      EEPROM.write(start + j, a[j]);
    }
    beep(3000);
    beep(3000);
    Serial.println(F("Succesfully added RFID Card"));
  }
  else
  {
    Serial.println(F("Failed!"));
  }
}

/*-----------------------------------------------------------------------------------*/

void deleteID(byte a[]) // fungsi untuk delete kartu RFID
{
  if (!findID(a)) //cek apakah UID tersebut ada di EEPROM
  {
    Serial.println(F("Failed!"));
  }
  else
  {
    int num = EEPROM.read(0);   //mengambil jumlah RFID yang terdaftar pada EEPROM
    int slot;       // variable untuk nantinya ditentukan slot ke berapa
    int start;     // menentukan dimana start penghapusannya
    int looping;    // menentukan berapa loop untuk penghapusan
    int j;
    slot = findIDSLOT(a);  // mencari slotnya terlebih dahulu
    start = (slot * 4) + 2; //set variable - variablenya
    looping = ((num - slot) * 4);
    num--;      // mengurangi jumlah RFID yang terdaftar pada EEPROM
    EEPROM.write( 0, num );   // menuliskan jumlah RFID terbaru ke EEPROM
    for (j = 0; j < looping; j++)
    {
      EEPROM.write(start + j, EEPROM.read(start + 4 + j)); // menghapus EEPROM yang telah ditentukan sebelumnya
    }
    for (int k = 0; k < 4; k++)
    {
      EEPROM.write(start + j + k, 0);
    }
    beep(1000);
    Serial.println(F("Succesfully removed ID record from EEPROM"));
  }
}

/*-----------------------------------------------------------------------------------*/

boolean isMaster(byte test[]) //fungsi untuk mengecek mastercard
{
  if (checkTwo(test, masterCard))
    return true;
  else
    return false;
}

/*-----------------------------------------------------------------------------------*/

int findIDSLOT(byte find[])
{
  int count = EEPROM.read(0);       // Membaca jumlah kartu RFID yang terdaftar pada EEPROM
  for (int i = 1; i <= count; i++) // melakukan iterasi pencarian kartu
  {
    readID(i);
    if (checkTwo(find, storedCard)) // cek apakah kedua UID sesuai
    {
      return i;         // return nilai slot
      break;
    }
  }
}

/*-----------------------------------------------------------------------------------*/

void readID(int number) // fungsi untuk membaca UID dari EEPROM
{
  int start = (number * 4) + 2;
  for (int i = 0; i < 4; i++)
  {
    storedCard[i] = EEPROM.read(start + i); //membaca 4 byte variable pada EEPROM
  }
}

/*-----------------------------------------------------------------------------------*/

boolean findID(byte find[]) //fungsi untuk mencari UID pada EEPROM
{
  int count = EEPROM.read(0); //mengambil jumlah RFID yang terdaftar pada EEPROM
  for (int i = 1; i <= count; i++)
  {
    readID(i);  //membaca UID pada EEPROM secara iterasi
    if (checkTwo(find, storedCard)) //iterasi untuk cek UID pada variable storedCard dengan EEPROM
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

/*-----------------------------------------------------------------------------------*/

boolean checkTwo(byte a[], byte b[]) //fungsi untuk mengecek RFID card yang discan
{
  if (a[0] != NULL)
  {
    match = true;
  }
  for (int k = 0; k < 4; k++) // mengecek 4 byte UID RFID
  {
    if (a[k] != b[k])
    {
      match = false;
    }
  }
  if (match)
  {
    return true;
  }
  else
  {
    return false;
  }
}

/*-----------------------------------------------------------------------------------*/

void keypadWrite(int i) // fungsi untuk menyimpan inputan pin ke EEPROM
{
  for (int j = 0; j < i; j++)
  {
    Serial.println(pins[j] - 48);
    EEPROM.write(450 + j, pins[j]); // pin disimpan mulai dari EEPROM 450 s/d jumlah pin
  }
  beep(2000);
  Serial.println(F("Pin registered"));
  memset(pins, 0, sizeof(pins)/sizeof(int));
  return;
}

/*-----------------------------------------------------------------------------------*/

boolean checkPin() // fungsi untuk mengecek inputan pin
{
  int pinLength = EEPROM.read(449); // membaca jumlah pin yang telah di set pada EEPROM
  if (pinLength != a) // membandingkan jumlah pin pada EEPROM dengan yang dipencet user
  {
        Serial.println(a);
    a = 0;
    Serial.println(F("ASDA"));
    Serial.println(pinLength);
    return false;
  }
  if (a > 0)
  {
    for (int i = 0; i < a; i++)
    {
      if (pins[i] != EEPROM.read(450 + i)) // apabila ada 1 yang tidak sama, berarti pin salah
      {
        a = 0;
        Serial.println(pins[i]);
        Serial.println(EEPROM.read(450+i));
        return false;
      }
    }
    a = 0;
    return true;
  }
  else // counter pin bernilai < 0, maka pin salah
  {
    return false;
  }

}

/*-----------------------------------------------------------------------------------*/

void flushEEPROM() //hapus memori EEPROM, dipanggil jika dibutuhkan
{
  for (int x = 0; x < EEPROM.length(); x = x + 1)
  {
    if (EEPROM.read(x) == 0)
    {
    }
    else
    {
      EEPROM.write(x, 0);
    }
  }
}

/*-----------------------------------------------------------------------------------*/

void beep(unsigned char delayms) // fungsi untuk buzzer beep
{
  analogWrite(buzzerPin, 200); // nyalakan buzzer
  delay(delayms);
  analogWrite(buzzerPin, 0); // matikan buzzer
  delay(delayms);
}

/*-----------------------------------------------------------------------------------*/

void keypadEvent(KeypadEvent key) // interrupt berupa listener event yang diterapkan pada keypad
{
  if (keypad.getState() == PRESSED)
  {
    if (key != '#' && key != '*') // menyimpan setiap input keypad ke dalam array pins
    {
      pins[a] = key;
      Serial.println(pins[a]);
      Serial.println(a);
      a++;
      buzz(buzzerPin, NOTE_C7, 200);
    }
    else if (key == '#' && program == true) // ketika program mode, maka input pin berarti pin baru
    {
      EEPROM.write(449, a);
      Serial.println(a);
      keypadWrite(a);
      program = false;
      a = 0;
    }
    else if (rfidRead == true && key == '#')
    {
      beep(200);
        if(checkPin() == true)
        {
          memset(pins, 0, sizeof(pins)/sizeof(int));
          digitalWrite(solenoidLock, HIGH);
          solenoidUnlockTime = millis();
          beep(3000);
          beep(3000);
          rfidRead = false;
          Serial.println(F("Thou shalt pass."));
        } 
        else
        {
          if(b > 2)
          {
            rfidRead = false;
            Serial.println(F("Martabak duino safety lox box has been locked due to maximum pin tries exceeeded. \nYou have to scan your rfid again"));
            b = 0;
          }
          b++;
          Serial.println(F("WRONG PIN"));
        }
    }
    else if (key == '*') // tombol * adalah untuk reset input pin
    {
      for (int i = 0; i < 10; i++)
      {
        pins[i] = 0;
      }
      beep(100);
      beep(100);
      beep(100);
      a = 0;
    }
  }
}

/*-----------------------------------------------------------------------------------*/

void sing() //fungsi untuk start intro song 
{
  int size = sizeof(melody) / sizeof(int); // set jumlah iterasi
  for (int thisNote = 0; thisNote < size; thisNote++) // iterasi
  {
     int noteDuration = 1000 / tempo[thisNote]; //set durasi nada
     buzz(buzzerPin, melody[thisNote], noteDuration); //buzz nada
     int pauseBetweenNotes = noteDuration * 1.30; //set pause antar nada
     delay(pauseBetweenNotes);
     buzz(buzzerPin, 0, noteDuration);
  }
}

/*-----------------------------------------------------------------------------------*/

void buzz(int targetPin, long frequency, long length) //fungsi untuk membunyikan suara
{
  long delayValue = 1000000 / frequency / 2;
  long numCycles = frequency * length / 1000; 
  for (long i = 0; i < numCycles; i++) 
  { 
    digitalWrite(targetPin, HIGH);
    delayMicroseconds(delayValue);
    digitalWrite(targetPin, LOW);
    delayMicroseconds(delayValue);
  } 
}

/*-----------------------------------------------------------------------------------*/

void masterReset() // mengecek knock rahasia guna reset keseluruhan program
{
  int i = 0;
  for (i=0; i < maxKnocks; i++) // reset array ketukan agar data presisi
  {
    knockReadings[i] = 0;
  }
  
  int currentKnock = 0;  
  int startTime = millis(); 
  int now = millis();   

  do 
  {                            
    if(analogRead(piezoPin) >= threshold) // ketika ada ketukan baru, akan dimasukkan ke dalam array selanjutnya
    {                   
      now = millis();
      knockReadings[currentKnock] = now - startTime;
      currentKnock++;                             
      startTime = now;          
    }

    now = millis();
  } while((now-startTime < knockComplete) && (currentKnock < maxKnocks)); // piezo akan berhenti ketika ketukan telah melebihi batas maks knock atau waktu yang ditentukan
  
  if(validateKnock() == true) // knock yang sesuai
  {
    flushEEPROM(); //program untuk mereset seluruh data yang tersimpan
    asm volatile(" jmp 0");
  } 
  else // knock tidak sesuai, karena ini bersifat rahasia, maka tidak ada indikator keluaran.
  {
  } 
}

/*-----------------------------------------------------------------------------------*/

boolean validateKnock()
{
  int i = 0;
 
  int currentKnock = 0;
  int secretKnock = 0;
  int maxKnockInterval = 0; //digunakan untuk normalisasi data waktu antar ketukan
  
  for(i = 0; i < maxKnocks; i++)
  {
    if(knockReadings[i] > 0) //iterasi ke array knock yang sudah berhasil diinput untuk menghitung jumlah ketukan
    {
      currentKnock++;
    }
    if(secretCode[i] > 0) //iterasi ke secret knock yang sudah ditentukan untuk mencari banyaknya ketukan
    {         
      secretKnock++;
    }
    
    if(knockReadings[i] > maxKnockInterval)
    {  
      maxKnockInterval = knockReadings[i];
    }
  }

  if(currentKnock != secretKnock) // check apakah jumlah ketukan dari data yang didapat sensor dengan yang disimpan sama atau tidak
  {  
    return false;
  }
  

  /*  waktu yang didapat melalui sensor merupakan waktu absolute antar ketukan
   *  waktu absolute ini akan menyebabkan program menjadi sangat sensitif terhadap perbedaan yang sangat kecil 
   *  yang diinginkan adalah program dapat membaca ritme dari ketukan tersebut
   */
  int totaltimeDifferences = 0;
  int timeDiff = 0;
  for(i = 0; i < maxKnocks; i++) // iterasi untuk mapping waktu absolute menjadi waktu "semu" tanpa mengurangi kualitas keamanan 
  {    
    knockReadings[i]= map(knockReadings[i], 0, maxKnockInterval, 0, 100);      
    timeDiff = abs(knockReadings[i] - secretCode[i]);
    if (timeDiff > rejectValue) // penjaga keamanan dari waktu "semu"
    {
      return false;
    }
    totaltimeDifferences += timeDiff;
  } 
  return true;
}

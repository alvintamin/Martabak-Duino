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
 */
#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>
#include <Keypad.h>

#define RST_PIN 5
#define SS_PIN 53

byte masterCard[4];
byte readCard[4];
byte storedCard[4];

int pins[10];
char keypin;
int a = 0; // global variable counter pin

int successRead;
boolean program = false;
boolean match = false;

unsigned long solenoidUnlockTime = 0;

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
  Serial.begin(9600);
  SPI.begin();
  mfrc522.PCD_Init();
  keypad.addEventListener(keypadEvent); // menambahkan interrupt keypad ke system

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
    EEPROM.write(449 , 4); // set default jumlah pin untuk safety box dengan pin 1234
    EEPROM.write(450, 1);
    EEPROM.write(451, 2);
    EEPROM.write(452, 3);
    EEPROM.write(453, 4);
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
    successRead = getID();
    keypin = keypad.getKey();
    if (keypin == '#')
    {
      memset(readCard, 0, sizeof(readCard)); // flush variable readCard
      successRead = 1;
    }
    if ((millis() - solenoidUnlockTime) > 1000) {
      digitalWrite(A0, LOW);
    }
  } while (successRead != 1);
  if (program) // program mode
  {
    if (isMaster(readCard)) // cek apakah kartu tersebut adalah mastercard
    {
      Serial.println(F("Master Card Scanned"));
      Serial.println(F("Exiting Program Mode"));
      program = false;
      memset(readCard, 0, sizeof(readCard));
      return;
    }
    else if (keypin == '#') // input # untuk record pin baru
    {
      Serial.println(F("Input your new pin"));
      inputPin();
      program = false;
      return;
    }
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
    }
    else
    {
      if (findID(readCard)) // cek kebenaran kartu
      {
        digitalWrite(A0, HIGH);
        solenoidUnlockTime = millis();
        Serial.println(F("Thou shalt pass."));
      }
      else if (checkPin() == true) // cek kebanaran pin
      {
        memset(pins, 0, sizeof(pins));
        digitalWrite(A0, HIGH);
        solenoidUnlockTime = millis();
        Serial.println(F("Thou shalt pass."));
      }
      else // jika tidak ada yang benar
      {
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

void inputPin() // fungsi yang dijalankan ketika ingin memprogram pin
{
  for (int i = 0; i < sizeof(pins); i++)
  {
    char key;
    do
    {
      key = keypad.getKey(); // mengambil inputan dari keypad
    } while (!key);
    if (key == '#')
    {
      EEPROM.write(449, i); //menyimpan jumlah pin tersebut ke EEPROM 449
      keypadWrite(i); //memanggil fungsi keypadWrite dengan pass variable jumlah pin
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

/*-----------------------------------------------------------------------------------*/

void keypadWrite(int i) // fungsi untuk menyimpan inputan pin ke EEPROM
{
  for (int j = 0; j < i; j++)
  {
    Serial.println(pins[j] - 48);
    EEPROM.write(450 + j, pins[j]); // pin disimpan mulai dari EEPROM 450 s/d jumlah pin
  }
  Serial.println(F("Pin registered"));
  memset(pins, 0, sizeof(pins));
  return;
}

/*-----------------------------------------------------------------------------------*/

boolean checkPin() // fungsi untuk mengecek inputan pin
{
  int pinLength = EEPROM.read(449); // membaca jumlah pin yang telah di set pada EEPROM
  if (pinLength != a) // membandingkan jumlah pin pada EEPROM dengan yang dipencet user
  {
    a = 0;
    return false;
  }
  if (a > 0)
  {
    for (int i = 0; i < a; i++)
    {
      if (pins[i] != EEPROM.read(450 + i)) // apabila ada 1 yang tidak sama, berarti pin salah
      {
        a = 0;
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

void keypadEvent(KeypadEvent key) // interrupt berupa listener event yang diterapkan pada keypad
{
  if (keypad.getState() == PRESSED)
  {
    if (key != '#' && key != '*') // menyimpan setiap input keypad ke dalam array pins
    {
      pins[a] = key;
      a++;
    }
    else if (key == '*') // tombol * adalah untuk reset input pin
    {
      for (int i = 0; i < 10; i++)
      {
        pins[i] = 0;
      }
      a = 0;
    }
  }
}




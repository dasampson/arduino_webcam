#include <SPI.h>
#include <SD.h>
#include <Ethernet.h>
#include <OneWire.h>
#include <SoftwareSerial.h>
#include <Adafruit_VC0706.h>


/*
  
  Web Server - Temperature Monitor - Web Camera
  
  This program utilizes a VC0706 camera and a DS18B20 temperature sensor, as well as
  the Ethernet shield. It serves a current picture and current temperature reading each
  time the website is accessed via a browser. 
  
*/


// WEB SERVER [IP and MAC Addresses with Port Number]
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192,168,1, 200);
EthernetServer server(90);
#define BUFSIZ 100 // Size of line buffer


// TEMPERATURE SENSOR
OneWire ds(2); // on digital pin 2


// SD CARD
Sd2Card card;
SdVolume volume;
SdFile root;
SdFile file;


// CAMERA
SoftwareSerial cameraconnection(6, 7);
Adafruit_VC0706 cam(&cameraconnection);



void setup(void) 
{
  pinMode(10, OUTPUT);
  digitalWrite(10, HIGH);

  // Initialize the card
  card.init(SPI_FULL_SPEED, 4);
  volume.init(&card);
  root.openRoot(&volume);
  SD.begin(4);
  
  // Initialize camera
  cam.begin(38400);
  cam.setImageSize(VC0706_640x480);
  
  // start the Ethernet connection and the server
  Ethernet.begin(mac, ip);
  server.begin();
}



// Takes a single picture
void take_picture(char * filename)
{ 
  cam.takePicture();
  
  SD.remove(filename);
  
  File imgFile = SD.open(filename, FILE_WRITE);
  // Get the size of the image (frame) taken  
  uint16_t jpglen = cam.frameLength();
  // Read all the data up to # bytes!
  while (jpglen > 0) 
  {
    // read 64 bytes at a time;
    uint8_t *buffer;
    uint8_t bytesToRead = min(32, jpglen);   // change 32 to 64 for a speedup but may not work with all setups!
    buffer = cam.readPicture(bytesToRead);
    imgFile.write(buffer, bytesToRead);
    jpglen -= bytesToRead;
  }
  imgFile.close();   
  cam.reset();
}



// Returns temperature from one DS18S20 in degrees Fahrenheit
float getTemp()
{
  byte data[12];
  byte addr[8];
  
  if (!ds.search(addr)) 
  {
    //no more sensors on chain, reset search
    ds.reset_search();
    return -1000;
  }
  
  if (OneWire::crc8( addr, 7) != addr[7]) 
  {
    return -1000;
  }
  
  if ( addr[0] != 0x10 && addr[0] != 0x28) 
  {
    return -1000;
  }
  
  ds.reset();
  ds.select(addr);
  ds.write(0x44,1); // start conversion, with parasite power on at the end
  
  byte present = ds.reset();
  ds.select(addr);  
  ds.write(0xBE); // Read Scratchpad
  
   
  for (int i = 0; i < 9; i++) 
  {
    data[i] = ds.read();
  }
   
  ds.reset_search();
   
  byte MSB = data[1];
  byte LSB = data[0];
  
  float tempRead = ((MSB << 8) | LSB); //using two's compliment
  float TemperatureSum = tempRead / 16;
   
  // Converting to Fahrenheit
  TemperatureSum = ((TemperatureSum * 9)/5) + 32;
   
  return TemperatureSum;
}



void loop()
{
  char clientline[BUFSIZ];
  char * filename;
  int index = 0;
  
  EthernetClient client = server.available();
  
  if (client) 
  {
    // an http request ends with a blank line
    boolean current_line_is_blank = true;
    
    // reset the input buffer
    index = 0;
    
    while (client.connected())
    {
      if (client.available())
      {
        char c = client.read();
        
        // If it isn't a new line, add the character to the buffer
        if (c != '\n' && c != '\r') 
        {
          clientline[index] = c;
          ++index;
          
          // are we too big for the buffer? start tossing out data
          if (index >= BUFSIZ)
          { 
            index = BUFSIZ -1;
          }
          
          continue;
        }
        
        clientline[index] = 0;
        filename = 0;
        
        if (strstr(clientline, "GET / ") != 0)
        {
          float currentTemp = getTemp();
          take_picture("TEST.JPG");
          
          client.println("<html>");
          client.println("<p>");
          client.println("<img src=\"TEST.JPG\" border=1 />");
          client.println("<p>");
          client.println(F("Current temperature: "));
          client.println(currentTemp);
          client.println("</body></html>");
        }
        else if (strstr(clientline, "GET /TEST.JPG") != 0)
        {
          filename = "TEST.JPG";
          
          file.open(&root, filename, O_READ);

          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: image/jpeg");
          client.println();
          
          int16_t c;
          while ((c = file.read()) >= 0)
          {
            client.print((char)c);
          }
          
          file.close();
        }
        
        break;
      }
    }
    
    client.stop();
  }
}

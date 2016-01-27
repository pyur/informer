#include <EtherCard.h>

#include <Adafruit_NeoPixel.h>


unsigned long loop_count = 0;
unsigned int base_animation_rate = 256;


    // -------- enc26j80 setup -------- //

#define STATIC 1  // set to 1 to disable DHCP (adjust myip/gwip values below)

#if STATIC
static byte myip[] = { 192,168,1,241 };  // ethernet interface ip address
static byte gwip[] = { 192,168,1,1 };  // gateway ip address
#endif

// ethernet mac address - must be unique on your network
static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 };  // tii-01
byte Ethernet::buffer[700]; // tcp/ip send and receive buffer
static BufferFiller bfill;  // used as cursor while filling the buffer


const char okHeader[] PROGMEM = 
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Content-Length: 0\r\n"
    "Pragma: no-cache\r\n\r\n";


    // -------- neopixel setup -------- //

const byte number_of_leds = 96;

  //  params:
  // number of leds
  // neopixel data pin
  // working mode
Adafruit_NeoPixel strip = Adafruit_NeoPixel(number_of_leds, 2, NEO_GRB + NEO_KHZ800);

byte led_num[1];
byte led_data[number_of_leds * 7];
int glightness = -1;
const int glightness_max = 31;
boolean needShowStrip = false;
boolean forceFadeIn = false;
boolean forceFadeOut = false;


    // -------- Motion sensor -------- //
#define PIN_MOTION  9



    // -------------------------------- setup -------------------------------- //

void setup(){
  Serial.begin(57600);
  //while (!Serial) {;} // wait for serial port to connect. Needed for Leonardo only


    // -------- enc26j80 init -------- //

  if (ether.begin(sizeof Ethernet::buffer, mymac, 10) == 0)  Serial.println( "Failed to access Ethernet controller");
  #if STATIC
    ether.staticSetup(myip, gwip);
  #else
    if (!ether.dhcpSetup())  Serial.println("DHCP failed");
  #endif

  ether.printIp("IP:  ", ether.myip);
  ether.printIp("GW:  ", ether.gwip);  
  ether.printIp("DNS: ", ether.dnsip);  
  
  
    // -------- neopixel init -------- //

  // This is for Trinket 5V 16MHz, you can remove these three lines if you are not using a Trinket
  #if defined (__AVR_ATtiny85__)
    if (F_CPU == 16000000) clock_prescale_set(clock_div_1);
  #endif
  // End of trinket special code
  
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  
  
    // -------- Motion sensor -------- //
  
  pinMode(PIN_MOTION, INPUT);
  
}




    // -------------------------------- loop -------------------------------- //

void loop(){
  
  word len = ether.packetReceive();
  word pos = ether.packetLoop(len);
  
  if (pos) {
    bfill = ether.tcpOffset();
    char* data = (char *) Ethernet::buffer + pos;

    //Serial.println(data);

    if (strncmp("GET /", data, 5) == 0) {
      char * pend;
      pend = strchr( data + 5, ' ' );

      int request_length = 0;
      if (pend != NULL)  request_length = (pend - data - 5) / 2;
      //Serial.println(request_length);

        // ---- turn one led ---- //
      if (request_length == 8) {
        // led_num, red-1, green-1, blue-1, red-2, green-2, blue-2, fade_speed
        hexByte( data + 5, led_num, 1);
        if (led_num[0] < number_of_leds) {
          hexByte( data + 7, led_data + (7 * led_num[0]),  7);
        }
        //printBytes(request, request_length);        
      }
      
        // ---- turn all leds ---- //
      else if (request_length == (int)number_of_leds * 3) {
        for (byte i = 0; i < number_of_leds; i++) {
          hexByte( data + 5 + i*6, led_data + i*7, 3);
          //led_data[i*7 + 3] = 0;
          //led_data[i*7 + 4] = 0;
          led_data[i*7 + 5] = 0;
          led_data[i*7 + 6] = 0;
        }
      }
      
        // ---- contorol motion sensor ---- //
      else if (request_length == 1) {
        hexByte( data + 5, led_num, 1);

          // -- leds always off -- //
        if (led_num[0] == 0x00) {
          forceFadeOut = true;
          forceFadeIn = false;
        }

          // -- leds always on -- //
        else if (led_num[0] == 0x01) {
          forceFadeIn = true;
          forceFadeOut = false;
        }

          // -- leds show on motion sensor -- //
        else if (led_num[0] == 0x02) {
          forceFadeIn = false;
          forceFadeOut = false;
        }

      }

    }
    
    bfill.emit_p(PSTR("$F"), okHeader);
        
    ether.httpServerReply(bfill.position());
    }


    // -------- Animation -------- //
    
  if (!(loop_count % base_animation_rate)) {
    unsigned long count = loop_count / base_animation_rate;

    if (glightness >= 0) {
      for (byte i = 0; i < number_of_leds; i++) {

          // ---- dynamic ---- //
        if (led_data[7 * i +6]) {
          float lightness_f = sin((float)(count % led_data[7 * i +6]) / (float)led_data[7 * i +6] * PI);
  
          if (glightness >= glightness_max) {
            //Serial.println(" - dynamic full");
            strip.setPixelColor(i,
            led_data[7 * i +0] * lightness_f + led_data[7 * i +3] * (1-lightness_f),
            led_data[7 * i +1] * lightness_f + led_data[7 * i +4] * (1-lightness_f),
            led_data[7 * i +2] * lightness_f + led_data[7 * i +5] * (1-lightness_f)
            );
          }
          else if (glightness >= 0) {
            float glightness_f = (float)glightness / (float)glightness_max;
            //Serial.print(glightness_f);
            //Serial.println(" - dynamic");
            strip.setPixelColor(i,
            (led_data[7 * i +0] * lightness_f + led_data[7 * i +3] * (1-lightness_f)) * glightness_f,
            (led_data[7 * i +1] * lightness_f + led_data[7 * i +4] * (1-lightness_f)) * glightness_f,
            (led_data[7 * i +2] * lightness_f + led_data[7 * i +5] * (1-lightness_f)) * glightness_f
            );
          }
          needShowStrip = true;
        }  // dynamic
        
          // ---- static ---- //
        else if (glightness >= 0 && glightness <= glightness_max) {
          float glightness_f = (float)glightness / (float)glightness_max;
          //Serial.print(glightness_f);
          //Serial.println(" - static");
          strip.setPixelColor(i,
          led_data[7 * i +0] * glightness_f,
          led_data[7 * i +1] * glightness_f,
          led_data[7 * i +2] * glightness_f
          );
          needShowStrip = true;
        }  // static

          // ---- static new ---- //
        else if (glightness > glightness_max && !led_data[7 * i +5]) {
          strip.setPixelColor(i,
          led_data[7 * i +0],
          led_data[7 * i +1],
          led_data[7 * i +2]
          );
          needShowStrip = true;
          led_data[7 * i +5] = 1;
        }  // static
        
      }  // led loop
      

      if (needShowStrip) {
        strip.show();
        needShowStrip = false;
      }

    }  // glihtness >= 0
    

      // -------- Motion sensor -------- //

    if (!forceFadeOut && glightness <= glightness_max && (digitalRead(PIN_MOTION) || forceFadeIn) ) {
      // maximum value `glightness_max + 1` for lock
      glightness++;
      //Serial.print(glightness);
      //Serial.println(" - fade in");
    }

    else if (!forceFadeIn && glightness >= 0 && (!digitalRead(PIN_MOTION) || forceFadeOut) ) {
      // minimum value `-1` for lock
      glightness--;
      //Serial.print(glightness);
      //Serial.println(" - fade out");
    }


  }

  
  loop_count++;
}
  
  


    // -------------------------------- functions -------------------------------- //

void printBytes ( const unsigned char * request, int length ) {
  for (int i = 0; i < length; i++) {
    Serial.println((char)request[i]);
  }  
}


  // str   - указатель на массив символов
  // bytes - выходной буфер
  // функция возвращает кол-во байт

inline byte toHex( char ch ) {
  return ( ( ch >= 'A' ) ? ( ch - 'A' + 0xA ) : ( ch - '0' ) ) & 0x0F;
}

void hexByte( char* str, byte* bytes , int length) {
  for (int i = 0; i < length; i++) {
    bytes[i] = ( toHex( *str++ ) << 4 ) | toHex( *str++ );
  }
}

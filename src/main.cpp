#include <ESP8266WiFi.h> //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include "WiFiManager.h" //https://github.com/tzapu/WiFiManager
#include <sntp.h>
#include <RTClib.h>
#include <NTPClient.h>
#include <SoftwareSerial.h>

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
SoftwareSerial mySerial(D5, D6);

#define PD_OUT_SIZE 14
#define NUM_LEDS    16
#define LED_SHIFT   4  /**< Shift to have 1st LED at 12 o'clock */
#define NUM_DIGITS 4
#define TIME_ZOME 2*60*60

#define STATE_IDLE      0
#define STATE_TOUCHED   1

/**
 * @brief	LED Light Animation.
 */
typedef enum __attribute__((packed)) ledEffect_E
{
  LED_STATIC,            /**< All LEDs statically ON, no effect. */
  LED_FLASH_ALL,         /**< Flash effect on ALL LEDs simultaneously. */
  LED_PULSE,             /**< Pulse/Fade effect on ALL LEDs simultaneously. */
  LED_THROBBER,          /**< Rotating dot with a tail. */
  LED_ARROW_UP_SOLID,    /**< Solid arrow pointing up. */
  LED_ARROW_DOWN_SOLID,  /**< Solid arrow pointing down. */
  LED_ARROW_LEFT_SOLID,  /**< Solid arrow pointing left. */
  LED_ARROW_RIGHT_SOLID, /**< Solid arrow pointing right. */
  LED_ARROW_UP_FLASH,    /**< Flashing arrow pointing up. */
  LED_ARROW_DOWN_FLASH,  /**< Flashing arrow pointing down. */
  LED_ARROW_LEFT_FLASH,  /**< Flashing arrow pointing left. */
  LED_ARROW_RIGHT_FLASH, /**< Flashing arrow pointing right. */
  LED_ARROW_UP_ANIM,     /**< Animated arrow pointing up. */
  LED_ARROW_DOWN_ANIM,   /**< Animated arrow pointing down. */
  LED_ARROW_LEFT_ANIM,   /**< Animated arrow pointing left. */
  LED_ARROW_RIGHT_ANIM,  /**< Animated arrow pointing right. */
  LED_CIRCLE_POINT_CW,   /**< Circle single LED clockwise */
  LED_CIRCLE_POINT_CCW,  /**< Circle single LED counter clockwise */
  LED_CIRCLE_FILL_CW,    /**< Fill LED ring/circle clockwise */
  LED_CIRCLE_FILL_CCW,   /**< Fill LED ring/circle counter clockwise */
  LED_STATIC_EVEN,       /**< Even-numbered LEDs statically ON */
  LED_STATIC_ODD,        /**< Odd-numbered LEDs statically ON */

  LED_TERMNINATION_ID
} ledAnimationId_t;

/**
 * @brief 	animation Id plus frequency equals LED effect.
 */
typedef struct __attribute__((packed)) ledEffect_S
{
  ledAnimationId_t animationId; /**< Animation identity */
  uint8_t frequency;            /**< Animation frequency (e.g. Flash freq.), 0.1Hz to 10Hz */
} ledEffect_t;

/**
 * @brief 	LED Color + Brightness (8 bit, each).
 */
typedef struct __attribute__((packed)) ledRGBI8_S
{
  uint8_t i; /**< intensity / brightness percentage (0 ... 100 %) *OR* index (in color map) */
  uint8_t r; /**< Red */
  uint8_t g; /**< Green */
  uint8_t b; /**< Blue */
} ledRGBI8_t;

typedef struct __attribute__((packed))
{
  uint16_t activeLedsMask;
  ledRGBI8_t ledRGBI;
  ledEffect_t ledEffect;
  uint8_t displayContent[NUM_DIGITS];
  uint8_t displayBrightness;
  uint8_t touchState;   /**< #STATE_IDLE, #STATE_TOCHED */
} pdDirect_t;

pdDirect_t button_strukt = { 0u };
pdDirect_t pdDate = { 0u };

#if 0
/**
 * @brief Send the time to series40.
 *
 * Display format is `HHMM`.
 *
 * @param theTime	time stamp which time is displayed.
 * @return minute of `theTime`.
 */
static void set_time( const time_t *theTime, char* text )
{
	struct tm *clock;

	clock = localtime( theTime );
	snprintf( &text[0], NUM_DIGITS+1, "%02d%02d", clock->tm_hour, clock->tm_min );
}


static uint32_t calculate_LedMask( const time_t *theTime )
{
	struct tm *clock;
	clock = localtime( theTime );
	const uint32_t seconds = clock->tm_sec;
	const uint32_t numActiveLeds = (16u * seconds + 30u) / 60u;
	uint32_t ledMask = (1u << numActiveLeds) - 1u;
	ledMask = ((ledMask & 0x7FFFu) << 1u) | (((ledMask & 0x8000u)) >> 15u );
	return ledMask;
}

#endif

void setupStrukt(void)
{
  button_strukt.activeLedsMask = 0x0000u;
  button_strukt.ledRGBI.i = 255u;
  button_strukt.ledRGBI.r = 0;
  button_strukt.ledRGBI.g = 255u;
  button_strukt.ledRGBI.b = 128u;
  button_strukt.displayBrightness = 255u;

  pdDate.displayBrightness = 255u;
  pdDate.touchState = STATE_TOUCHED;
}


void configModeCallback(WiFiManager *myWiFiManager)
{
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //reset settings - for testing
  //wifiManager.resetSettings();

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("Martins_Uhr", ""))
  {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
  timeClient.begin();
  setupStrukt();
  mySerial.begin(9600);
}


static void set_date( const time_t date, char text[] )
{
	struct tm *clock;

	clock = localtime( &date );
	snprintf( &text[0], NUM_DIGITS + 1, "%02d%02d", clock->tm_mday, clock->tm_mon + 1 );
}


unsigned long last_millis = 0;
int last_minute = 0;
int start_up = 0;
uint32_t prevNumActiveLeds = NUM_LEDS + 1;
void loop()
{
  // put your main code here, to run repeatedly:
  timeClient.update();
  timeClient.setTimeOffset(TIME_ZOME);
  Serial.print("\n\nNew Output: Time:");
  Serial.println(timeClient.getFormattedTime());
  
  //TODO stimmt in der ersten minute nicht...
  if (last_minute != timeClient.getMinutes())
  {
    last_millis = millis();
  }
  last_minute = timeClient.getMinutes();
  Serial.print("Sec in millis: ");
  Serial.println(millis() - last_millis);
  uint32_t numActiveLeds = (NUM_LEDS * (millis() - last_millis) + 30000u) / 60000u;

  if (numActiveLeds != prevNumActiveLeds) {
    uint32_t activeLedsMask = ((1u << numActiveLeds) - 1u) << LED_SHIFT;
    button_strukt.activeLedsMask = (activeLedsMask >> NUM_LEDS) | (activeLedsMask & 0xFFFFu);
    Serial.print("LEDs on: ");
    Serial.println(numActiveLeds);
    Serial.print("LED Mask: ");
    Serial.println(button_strukt.activeLedsMask, BIN);

    char text[NUM_DIGITS + 1];
    snprintf(&text[0], NUM_DIGITS + 1, "%02d%02d", timeClient.getHours(), timeClient.getMinutes());
    Serial.print("Text on display: ");
    Serial.println(text);

    char textDate[NUM_DIGITS + 1];
    set_date( timeClient.getEpochTime(), textDate);

    for (int i = 0; i < NUM_DIGITS; i++)
    {
      button_strukt.displayContent[i] = text[i];
      pdDate.displayContent[i] = textDate[i];
    }


    Serial.print("button_strukt: ");
    uint8_t *pt = (uint8_t *)&button_strukt;
    for (int i = 0; i < PD_OUT_SIZE; i++)
    {
      mySerial.write(pt[i]);
      Serial.print(pt[i], DEC);
    }

    delay(100);
    pt = (uint8_t *)&pdDate;
    for (int i = 0; i < PD_OUT_SIZE; i++)
    {
      mySerial.write(pt[i]);
    }


    prevNumActiveLeds = numActiveLeds;
  }
}
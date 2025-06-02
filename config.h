/// Config settings

/* Time */
// Timezone string for your region, example: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
#define TIMEZONE "GMT0BST,M3.5.0/1,M10.5.0"


/* Pins */
// Four pins for screen
#define CS_PIN 5
#define DC_PIN 0
#define RST_PIN 2
#define BUSY_PIN 15
// LED pin
#define LED_PIN 22


const unsigned long ntpSyncInterval = 30 * 60 * 1000;  // Sync every 1 minutes (in ms)
const unsigned long httpTimeout = 10 * 1000;           // 10 seconds


/* UI */
const bool SHOW_SPLASH_SCREEN = false;
const int LEFT_MARGIN = 5;
// expand/interpret some behavioral config options

#if !defined(DEBUG)
#define DEBUG 0
#endif

#if defined(PINMODE_SD_LED_OUTPUT) && defined(SD_LED_ON) && defined(SD_LED_OFF)
 #define SD_LED 1
#else
 #define SD_LED 0
#endif

#if !defined(DEBUG_LED)
 #if defined(PINMODE_DEBUG_LED_OUTPUT)
  #undef PINMODE_DEBUG_LED_OUTPUT
 #endif
 #define PINMODE_DEBUG_LED_OUTPUT

 #if defined(DEBUG_LED_ON)
  #undef DEBUG_LED_ON
 #endif
 #define DEBUG_LED_ON

 #if defined(DEBUG_LED_OFF)
  #undef DEBUG_LED_OFF
 #endif
 #define DEBUG_LED_OFF
#endif

#if DEBUG && defined(CONSOLE)
 #define DEBUG_PRINT(x)     CONSOLE.print (x)
 #define DEBUG_PRINTI(x,y)  CONSOLE.print (x,y)
 #define DEBUG_PRINTL(x)    CONSOLE.println (x)
 #define DEBUG_PRINTIL(x,y) CONSOLE.println (x,y)
#else
 #define DEBUG_PRINT(x)
 #define DEBUG_PRINTI(x,y)
 #define DEBUG_PRINTL(x)
 #define DEBUG_PRINTIL(x,y)
#endif

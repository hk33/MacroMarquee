

// Change this to be at least as long as your pixel string (too long will work fine, just be a little slower)

#define PIXELS 96*11  // Number of pixels in the string

// These values depend on which pins your 8 strings are connected to and what board you are using 
// More info on how to find these at http://www.arduino.cc/en/Reference/PortManipulation

// PORTD controls Digital Pins 0-7 on the Uno

// You'll need to look up the port/bit combination for other boards. 

// Note that you could also include the DigitalWriteFast header file to not need to to this lookup.

#define PIXEL_PORT  PORTD  // Port of the pin the pixels are connected to
#define PIXEL_DDR   DDRD   // Port of the pin the pixels are connected to

#define PIXEL_BITMASK 0b11111111  // If you do not want to use all 8 pins, you can mask off the ones you don't want
                                  // Note that these will still get 0 written to them when we send pixels

// These are the timing constraints taken mostly from the WS2812 datasheets 
// These are chosen to be conservative and avoid problems rather than for maximum throughput 

#define T1H  900    // Width of a 1 bit in ns
#define T1L  600    // Width of a 1 bit in ns

#define T0H  400    // Width of a 0 bit in ns
#define T0L  900    // Width of a 0 bit in ns

#define RES 6000    // Width of the low gap between bits to cause a frame to latch

// Here are some convience defines for using nanoseconds specs to generate actual CPU delays

#define NS_PER_SEC (1000000000L)          // Note that this has to be SIGNED since we want to be able to check for negative values of derivatives

#define CYCLES_PER_SEC (F_CPU)

#define NS_PER_CYCLE ( NS_PER_SEC / CYCLES_PER_SEC )

#define NS_TO_CYCLES(n) ( (n) / NS_PER_CYCLE )

// Actually send the next set of 8 bits to the 8 pins.
// We must to drop to asm to enusre that the complier does
// not reorder things and make it so the delay happens in the wrong place.

inline void sendBitX8( uint8_t bitx8 ) {

    const uint8_t allones=0xff;
            
    asm volatile (
      
      "out %[port], %[allones] \n\t"                // 1st step - send T0H high 
      
      ".rept %[T0HCycles] \n\t"         // Execute NOPs to delay exactly the specified number of cycles
        "nop \n\t"
      ".endr \n\t"
      
      "out %[port], %[bits] \n\t"                             // set the output bits to thier values for T0H-T1H
      ".rept %[offCycles] \n\t"                               // Execute NOPs to delay exactly the specified number of cycles
      "nop \n\t"
      ".endr \n\t"
      
      "out %[port],__zero_reg__  \n\t"                // last step - T1L all bits low

      // Don't need an explicit delay here since the overhead that follows will always be long enough
    
      ::
      [port]    "I" (_SFR_IO_ADDR(PIXEL_PORT)),
      [bits]   "d" (bitx8),
      [allones]   "d" (allones),
      
      [T0HCycles]  "I" (NS_TO_CYCLES(T0H) - 2),    // 1-bit width less overhead  for the actual bit setting, note that this delay could be longer and everything would still work
      
      [offCycles]   "I" (NS_TO_CYCLES((T1H-T0H)) - 2)     // Minimum interbit delay. Note that we probably don't need this at all since the loop overhead will be enough, but here for correctness

    );
                                  

    // Note that the inter-bit gap can be as long as you want as long as it doesn't exceed the 5us reset timeout (which is A long time)
    // Here I have been generous and not tried to squeeze the gap tight but instead erred on the side of lots of extra time.
    // This has thenice side effect of avoid glitches on very long strings becuase 
    
}  

// Sends a single color for a single row (1/3 of one pixel per string)
// The row is the bits for each of the strings. 0=off, 1=on to specified color
// This could be so much faster in pure ASM...

inline void sendRowOneColor( uint8_t row , uint8_t colorbyte ) {

  uint8_t bit=8;

  while (bit--) {

    if (colorbyte & (1<<bit)) {   // This ends up sending to color value as a bit stream becuase 
      sendBitX8( row );           
    } else {
      sendBitX8(0);
    }
    
  }

}
    

inline void sendRowRGB( uint8_t row ,  uint8_t r,  uint8_t g,  uint8_t b) {

  sendRowOneColor( row , g);    // WS2812 takes colors in GRB order
  sendRowOneColor( row , r);    // WS2812 takes colors in GRB order
  sendRowOneColor( row , b);    // WS2812 takes colors in GRB order
  
}

/*

  The following three functions are the public API:
  
  ledSetup() - set up the pin that is connected to the string. Call once at the begining of the program.  
  sendPixel( r g , b ) - send a single pixel to the string. Call this once for each pixel in a frame.
  show() - show the recently sent pixel on the LEDs . Call once per frame. 
  
*/


// Set the specified pin up as digital out

void ledsetup() {

  PIXEL_DDR = PIXEL_BITMASK;    // Set all used pins to output
  
  
}

inline void sendPixel( unsigned char r, unsigned char g , unsigned char b )  {  
  
  
}


// Just wait long enough without sending any bots to cause the pixels to latch and display the last sent frame

void show() {
  _delay_us( (RES / 1000UL) + 1);       // Round up since the delay must be _at_least_ this long (too short might not work, too long not a problem)
}


/*

  That is the whole API. What follows are some demo functions rewriten from the AdaFruit strandtest code...
  
  https://github.com/adafruit/Adafruit_NeoPixel/blob/master/examples/strandtest/strandtest.ino
  
  Note that we always turn off interrupts while we are sending pixels becuase an interupt
  could happen just when we were in the middle of somehting time sensitive.
  
  If we wanted to minimize the time interrupts were off, we could instead 
  could get away with only turning off interrupts just for the very brief moment 
  when we are actually sending a 0 bit (~1us), as long as we were sure that the total time 
  taken by any interrupts + the time in our pixel generation code never exceeded the reset time (5us).
  
*/


// Display a single color on the whole string

void showColor( unsigned char r , unsigned char g , unsigned char b ) {
  
  cli();  
  for( int p=0; p<PIXELS; p++ ) {
    sendPixel( r , g , b );
  }
  sei();
  show();
  
}

// Fill the dots one after the other with a color
// rewrite to lift the compare out of the loop
void colorWipe(unsigned char r , unsigned char g, unsigned char b, unsigned  char wait ) {
  for(unsigned int i=0; i<PIXELS; i+= (PIXELS/60) ) {
    
    cli();
    unsigned int p=0;
    
    while (p++<=i) {
        sendPixel(r,g,b);
    } 
     
    while (p++<=PIXELS) {
        sendPixel(0,0,0);  
      
    }
    
    sei();
    show();
    delay(wait);
  }
}

// Theatre-style crawling lights.
// Changes spacing to be dynmaic based on string size

#define THEATER_SPACING (PIXELS/20)

void theaterChase( unsigned char r , unsigned char g, unsigned char b, unsigned char wait ) {
  
  for (int j=0; j< 3 ; j++) {  
  
    for (int q=0; q < THEATER_SPACING ; q++) {
      
      unsigned int step=0;
      
      cli();
      
      for (int i=0; i < PIXELS ; i++) {
        
        if (step==q) {
          
          sendPixel( r , g , b );
          
        } else {
          
          sendPixel( 0 , 0 , 0 );
          
        }
        
        step++;
        
        if (step==THEATER_SPACING) step =0;
        
      }
      
      sei();
      
      show();
      delay(wait);
      
    }
    
  }
  
}
        


// I rewrite this one from scrtach to use high resolution for the color wheel to look nicer on a *much* bigger string
                                                                            
void rainbowCycle(unsigned char frames , unsigned int frameAdvance, unsigned int pixelAdvance ) {
  
  // Hue is a number between 0 and 3*256 than defines a mix of r->g->b where
  // hue of 0 = Full red
  // hue of 128 = 1/2 red and 1/2 green
  // hue of 256 = Full Green
  // hue of 384 = 1/2 green and 1/2 blue
  // ...
  
  unsigned int firstPixelHue = 0;     // Color for the first pixel in the string
  
  for(unsigned int j=0; j<frames; j++) {                                  
    
    unsigned int currentPixelHue = firstPixelHue;
       
    cli();    
        
    for(unsigned int i=0; i< PIXELS; i++) {
      
      if (currentPixelHue>=(3*256)) {                  // Normalize back down incase we incremented and overflowed
        currentPixelHue -= (3*256);
      }
            
      unsigned char phase = currentPixelHue >> 8;
      unsigned char step = currentPixelHue & 0xff;
                 
      switch (phase) {
        
        case 0: 
          sendPixel( ~step , step ,  0 );
          break;
          
        case 1: 
          sendPixel( 0 , ~step , step );
          break;

        case 2: 
          sendPixel(  step ,0 , ~step );
          break;
          
      }
      
      currentPixelHue+=pixelAdvance;                                      
      
                          
    } 
    
    sei();
    
    show();
    
    firstPixelHue += frameAdvance;
           
  }
}

  
// I added this one just to demonstrate how quickly you can flash the string.
// Flashes get faster and faster until *boom* and fade to black.

void detonate( unsigned char r , unsigned char g , unsigned char b , unsigned int startdelayms) {
  while (startdelayms) {
    
    showColor( r , g , b );      // Flash the color 
    showColor( 0 , 0 , 0 );
    
    delay( startdelayms );      
    
    startdelayms =  ( startdelayms * 4 ) / 5 ;           // delay between flashes is halved each time until zero
    
  }
  
  // Then we fade to black....
  
  for( int fade=256; fade>0; fade-- ) {
    
    showColor( (r * fade) / 256 ,(g*fade) /256 , (b*fade)/256 );
        
  }
  
  showColor( 0 , 0 , 0 );
  
    
}

void setup() {
    
  ledsetup();  
  
}


void loop() {

  cli();

  sendRowRGB( 0b10101010 , 0xff , 0 , 0 );
  sendRowRGB( 0b01010101 , 0, 0xff , 0  );
  sendRowRGB( 0b10101010 , 0 , 0 , 0xff  );
  sendRowRGB( 0b01010101 ,  0xff , 0xff, 0xff  );
 
  sei();
  delay(100);
  return;

  // Some example procedures showing how to display to the pixels:
  colorWipe(255, 0, 0, 0); // Red
  colorWipe(0, 255, 0, 0); // Green
  colorWipe(0, 0, 255, 0); // Blue
  
  // Send a theater pixel chase in...
  theaterChase(127, 127, 127, 0); // White
  theaterChase(127,   0,   0, 0); // Red
  theaterChase(  0,   0, 127, 0); // Blue
  
  rainbowCycle(1000 , 20 , 5 );
  detonate( 255 , 255 , 255 , 1000);
  
  return;
  
}



#define DEBUG                              // Comment this to close the Serial port
#include <SimpleTimer.h>
SimpleTimer timer;         
#define BULB_RELAY_PIN         14          // 220V Light bulb switched by an AC relay, connected at this pin.

// ------------------ FastLED definitions
#include <FastLED.h>
#define NEO_PIN                9          // Which pin on the Arduino is connected to the NeoPixels?
#define MAXPIXELS              150         // How many NeoPixels are attached to the Arduino?
CRGB pixels[MAXPIXELS];

// ------------------ Define animation modes:
// These numbers are serial IDs of the selected animation.
#define const_color            1           // Illuminate strip with a single, constant color.

#define train                  2		   // Have a "train" of lit LEDs travel through the strip
int LED_train_length         = 8;          // Number of LEDs in the traveling train

#define rainbow                3           // Illuminate with single, time changing color.

#define cycling_rainbow        4		   // Illuminate a rainbow of colors along the strip, cyclic in time
#define CYCLIC_RAINBOW_STEP    2           // How many pixels to propagate each frame

#define colorful_train         5		   // Similar to the train, but randomize the train color while its traveling
#define COLOR_CHANGE_FRAMES    8           // Every how many frames to randomize color. If less than LED_train_length than the train would have more than 1 color :)

#define purple_green           6           // Purple-Green cross-fade rotation
#define orange_green           7           // Orange-Green cross-fade rotation
#define blue_purple            8           // Blue-Purple  cross-fade rotation
#define glowing_spheres        9           // At some random interval, a random sphere (containing LEDs 1-50, 51-100 or 101-150) fades in from black to a random color.
#define N_FRAMES_GLOW          61          // Should be Odd. How many time frames each glow takes

// ------------------ 08.07.17 To Do:
// 3. shoot video with mirror
// 4. Consider soldering 2nd IR LED in parallel on the remote
// ------------------

CRGBPalette16 currentPalette;
TBlendType    currentBlending;

#define COLOR_CHG_INT          1500        // [msec] Interval for randomizing the train color. Faster than this is a bit epileptic

// ------------------ Global animation related inits
uint8_t current_anim         = 1;
int color_counter            = 0;          // Change constant color in every click on const_color remote button - cycle through colors[][] array
int frame                    = 0;          // Global time frame of current animation
CRGB init_color              = 0xff9637;   // Match the light bulb
CRGB anim_color              = init_color; // Match the light bulb shade
float fps                    = 20;         // Common for both train and colorful_train - animation speed [frames per second]
boolean speed_increase       = false;      // Flag which updates SimpleTimer interval
boolean speed_decrease       = false;      // Flag which updates SimpleTimer interval
#define MAX_FPS                80
#define MIN_FPS                3
uint8_t global_val           = 255*0.4;        // Variable global brightnes (value) for all animations

// ------------------ Color arrays
const uint8_t colors[8][3] = {
	{ 255,   0,   0 },                     // Red
	{ 130,  30,   0 },                     // Orange
	{ 102, 220,   0 },                     // Yellow
	{   0, 255,   0 },                     // Green
	{   0, 110,  35 },                     // Cyan
	{ 0  ,   0, 255 },                     // Blue
	{ 104,   0, 154 },                     // Purple
	{ 255, 255, 255 },                     // White
};

// -------------------- I2C Receiver config
#include <Wire.h>
#define I2C_SlaveAddr   5

// --------------------Magic Lighting Remote Control
const uint32_t BTN_1           = 0xFFB04F;
const uint32_t BTN_2           = 0xFF30CF;
const uint32_t BTN_3           = 0xFF708F;
const uint32_t BTN_4           = 0xFFA857;
const uint32_t BTN_5           = 0xFF28D7;
const uint32_t BTN_6           = 0xFF6897;
const uint32_t BTN_7           = 0xFF9867;
const uint32_t BTN_8           = 0xFF18E7;
const uint32_t BTN_9           = 0xFF58A7;
const uint32_t BTN_10          = 0xFF8877;
const uint32_t BTN_11          = 0xFF08F7;
const uint32_t BTN_12          = 0xFF48B7;
const uint32_t BTN_RED         = 0xFF906F;
const uint32_t BTN_GREEN       = 0xFF10EF;
const uint32_t BTN_WHITE       = 0xFFD02F;
const uint32_t BTN_BLUE        = 0xFF50AF;
const uint32_t BTN_MUTE        = 0xFF609F; // Power off strip
const uint32_t BTN_VOL_UP      = 0xFFF00F; // Speed up
const uint32_t BTN_VOL_DOWN    = 0xFFE817; // Speed down
const uint32_t BTN_CH_UP       = 0xFFA05F; // Brightness up
const uint32_t BTN_CH_DWN      = 0xFF20DF; // Brightness down
const uint32_t BTN_ENTER       = 0xFFE01F; // Toggle light bulb

// ------------------ End of definitions
void setup(){
  pinMode(BULB_RELAY_PIN,OUTPUT);
  digitalWrite(BULB_RELAY_PIN,HIGH); //  Start with the bulb on

  FastLED.addLeds<WS2812B,NEO_PIN, GRB>(pixels, MAXPIXELS).setCorrection( TypicalSMD5050 );
  memset8(pixels,0,MAXPIXELS*sizeof(struct CRGB));
  
  // Set initil brightness (not necessarily 255)
  FastLED.setBrightness(global_val);
  illuminate_strip(init_color);
  
  // Join I2C bus as a slave
  Wire.begin(I2C_SlaveAddr);
  Wire.onReceive(IR_callback);
  
  #if defined(DEBUG)
	Serial.begin(9600);
	// while(!Serial);                         // If entered debug mode, Arduino will wait here for the Serial monitor to come alive.
	Serial.println("NeoPixel + IR Remote started.");
  #endif
  randomSeed(analogRead(0)); // random analog noise will cause the call to randomSeed() to generate different seed numbers each time the sketch runs.
}

// ------------------ loop
void loop(){ // Available animation types e.g. const_color are defined at the beginning
	switch (current_anim){
		case const_color:
		{
			illuminate_strip(anim_color);
			break;
		}

		case train:
		{
			// Slow down this animation wrt to all the rest
			fps = fps*0.8;
			int train_timeID;  
			train_timeID = timer.setInterval(int(1000/fps),propagate_train);
			
			turn_off_light_bulb();
			blank_strip();		
			while (current_anim == train){
				update_anim_speed(train_timeID,propagate_train);
				timer.run(); // Run the propagate_train routine
			}
			// Exited the loop as animation type had changed.
			timer.deleteTimer(train_timeID);
			// Return to normal speed fps
			fps = fps*1.2;
			break;
		}
		
		case rainbow:
		{
			int rainbow_timeID;
			rainbow_timeID = timer.setInterval(int(1000/fps),crossFade);
			turn_off_light_bulb();
			frame = 0;
			while (current_anim == rainbow){
				update_anim_speed(rainbow_timeID,crossFade);
				timer.run(); // Run the crossFade routine
			}
			// Exited the loop as animation type had changed.
			timer.deleteTimer(rainbow_timeID);
		}
		
		case cycling_rainbow:
		{
			int rainbow_timeID;
			rainbow_timeID = timer.setInterval(int(1000/fps),propagate_cyclic_rainbow);
			turn_off_light_bulb();
			frame = 0;
			while (current_anim == cycling_rainbow){
				update_anim_speed(rainbow_timeID,propagate_cyclic_rainbow);
				timer.run(); // Run the propagate_cyclic_rainbow routine
			}
			// Exited the loop as animation type had changed.
			timer.deleteTimer(rainbow_timeID);
			break;
		}
		
		case colorful_train:
		{
			// Slow down this animation wrt to all the rest
			fps = fps*0.8;
			int color_train_timeID;  
			color_train_timeID = timer.setInterval(int(1000/fps),propagate_train);
			int color_change_timer;
			color_change_timer  = timer.setInterval(int(COLOR_CHANGE_FRAMES * 1000/fps),randomize_color);
			
			turn_off_light_bulb();
			blank_strip();		
			while (current_anim == colorful_train){
				update_anim_speed(color_train_timeID,propagate_train);
				update_anim_speed(color_change_timer,randomize_color);
				// Active timers: Propagate train and Randomize color
				timer.run();
			}
			timer.deleteTimer(color_train_timeID);
			timer.deleteTimer(color_change_timer);
			// Return to normal speed fps
			fps = fps*1.2;
			break;
		}
		
		case purple_green:
		{
			SetupTwoColorPalette(CRGB(CHSV(HUE_PURPLE,255,255)),CRGB::Green);
			currentBlending = LINEARBLEND;
			turn_off_light_bulb();
			frame = 0;

			while (current_anim == purple_green){
				frame = frame + 1; /* motion speed */
				if (speed_decrease){fps = fps*0.85; speed_decrease=0;}
				if (speed_increase){fps = fps*1.15; speed_increase=0;}
				FillLEDsFromPaletteColors(frame);
				FastLED.show();
				FastLED.delay(1000 / fps);
			}
			break;
		}
		
		case orange_green:
		{
			SetupTwoColorPalette(CRGB::DarkOrange,CRGB::Green);
			currentBlending = LINEARBLEND;
			turn_off_light_bulb();
			frame = 0;
			
			while (current_anim == orange_green){
				frame = frame + 1; /* motion speed */
				if (speed_decrease){fps = fps*0.85; speed_decrease=0;}
				if (speed_increase){fps = fps*1.15; speed_increase=0;}
				FillLEDsFromPaletteColors(frame);
				FastLED.show();
				FastLED.delay(1000 / fps);
			}
			break;
		}
		case blue_purple:
		{
			SetupTwoColorPalette(CRGB::Blue,CRGB(CHSV(HUE_PURPLE,255,255)));
			currentBlending = LINEARBLEND;
			turn_off_light_bulb();
			frame = 0;
			
			while (current_anim == blue_purple){
				frame = frame + 1; /* motion speed */
				if (speed_decrease){fps = fps*0.85; speed_decrease=0;}
				if (speed_increase){fps = fps*1.15; speed_increase=0;}
				FillLEDsFromPaletteColors(frame);
				FastLED.show();
				FastLED.delay(1000 / fps);
			}
			break;
		}
		
		case glowing_spheres:
		{
			turn_off_light_bulb();
			frame = 0;
			uint8_t sphere, hue, sat;
			while (current_anim == glowing_spheres){
				if (frame == 0){
					// Randomize sphere - starting either at LED #1, #51 or #101
					sphere = random(1,4);
					hue = random(0,256);
					sat = random(64,256);
				}			
				if (speed_decrease){fps = fps*0.85; speed_decrease=0;}
				if (speed_increase){fps = fps*1.15; speed_increase=0;}
				Glowing_spheres_advance(frame,sphere,hue,sat);
				FastLED.show();
				FastLED.delay(1000 / fps);
				frame = (frame+1) % N_FRAMES_GLOW;
				// Randomize the break between glows
				if (frame==0){
					float frames_break = random(15,60);
					FastLED.delay(frames_break*1000/fps);
				}
			}
			break;
		}		
	}
}

void blank_strip(){
	FastLED.clear();
}

void switch_to_next_color(){ // Change anim_color to cycle through the colors[][] array
	anim_color = CRGB(colors[color_counter][0],colors[color_counter][1],colors[color_counter][2]);
	color_counter = (color_counter+1) % (sizeof(colors)/3);
}

void illuminate_strip(CRGB color){
fill_solid(pixels, MAXPIXELS, color);
FastLED.show();
}

int update_anim_speed(int timerID, void (*f)()){ 
// This function, if needed, changes the interval of a timer by recreating a new timer.
// Inputs:  timerID - the timer's ID.
//          *f - pointer to the timer's callback function e.g. propagate_train.
// Output:  timerID of the new timer.
				if (speed_increase){
					speed_increase = false;
					fps = min(fps * 1.4,MAX_FPS);
					timer.deleteTimer(timerID);
					timerID = timer.setInterval(int(1000/fps),(*f));
				}
				else if (speed_decrease){
					speed_decrease = false;
					fps = max(fps * 0.7,MIN_FPS);
					timer.deleteTimer(timerID);
					timerID = timer.setInterval(int(1000/fps),(*f));
				}
}

//----------------------------- Before starting an animation, turn off the 220V light bulb 
void turn_off_light_bulb(){
// This function could potentially cross fade the light from on to off using variable PWM
	digitalWrite(BULB_RELAY_PIN,LOW);
}

//----------------------------- Propagate an LED train by one pixel
void propagate_train(){
	if (frame>(MAXPIXELS+LED_train_length)){
		frame = LED_train_length+1;
	}
	// Illuminate next LED and blank last one	
	pixels[(frame-1) % MAXPIXELS] = anim_color;
	if (frame>LED_train_length){
		pixels[frame-LED_train_length-1] = 0;
	}
	FastLED.show();	
	frame++;
}

//----------------------------- Set anim_color to a random hue
void randomize_color(){
// maintain a similar brightness level by fixng (R+G+B)/3=target_avg.
	// set the global color variable.
	anim_color = CRGB(CHSV(random(0,256),255,255));
}

//----------------------------- Rainbow in time - cross fade animation
void crossFade(){
illuminate_strip(CHSV(frame,255,255));
frame = (frame+1) % 255;
}

//----------------------------- Rainbow along the strip - cycling colors
void propagate_cyclic_rainbow(){
fill_rainbow(pixels, MAXPIXELS, frame, ceil(1.1*255/MAXPIXELS));
FastLED.show();
frame = (frame+1) % 255;
}

//----------------------------- This function sets up a two-colored palette.
void SetupTwoColorPalette(CRGB Color1, CRGB Color2)
{
    CRGB black  = CRGB::Black;
    
    currentPalette = CRGBPalette16(
                                   Color2,  Color2,  black,  black,
                                   Color1, Color1, black,  black,
                                   Color2,  Color2,  black,  black,
                                   Color1, Color1, black,  black );
}

//----------------------------- Make a single frame of any palette-defined animation
void FillLEDsFromPaletteColors( uint8_t colorIndex)
{
    uint8_t brightness = 255;
    
    for( int i = 0; i < MAXPIXELS; i++) {
        pixels[i] = ColorFromPalette( currentPalette, colorIndex, brightness, currentBlending);
        colorIndex += 3;
    }
}

//----------------------------- Make a single frame of the glowing spheres animation
void Glowing_spheres_advance(uint8_t frame, uint8_t sphere, uint8_t hue, uint8_t sat)
{
	// Which sphere was selected?
	//Set up these numbers to represent actual number of LEDs in each of your lanterns.
	uint8_t first_LED_ind, last_LED_ind;
	switch (sphere){
		case 1:
			first_LED_ind = 0;
			last_LED_ind = 40;
		break;
		case 2:
			first_LED_ind = 41;
			last_LED_ind = 80;
		break;
		case 3:
			first_LED_ind = 81;
			last_LED_ind = MAXPIXELS;
		break;
	}
	
	// Ramp up the HSV "value" from 0 to 255 during N_FRAMES_GLOW/2, then ramp down to 0 in the remaining frames
	uint8_t val = 255/(N_FRAMES_GLOW/2) * (N_FRAMES_GLOW/2 - abs(frame%N_FRAMES_GLOW - N_FRAMES_GLOW/2)) ;
	uint8_t pixel_val; // will hold 0 for pixels outside the selected sphere, val for pixels within the sphere.
	for( int i = 0; i<MAXPIXELS; i++){
		
		pixel_val = (i>first_LED_ind && i<=last_LED_ind) ? val : 0;
		pixels[i] = CHSV(hue,sat,pixel_val);
		
	}
	
}
//----------------------------- Map the IR signals to global variable actions
void interpret_IR_signal(uint32_t irCode){ 
		switch (irCode){ // Don't forget to add 0x before the hex code given from My_Decoder.DumpResults(); and convert it to lowercase.
			case BTN_1: // 1
				current_anim = 1;
				if (anim_color){
					switch_to_next_color();
				} else {
					anim_color = init_color;
				}
				illuminate_strip(anim_color);
			break;			
			case BTN_2: // 2
				current_anim = 2;
				if (anim_color){
					switch_to_next_color();
				} else {
					anim_color = init_color;
				}
				break;
			case BTN_3: // 3
				current_anim = 3;
			break;
			case BTN_4: // 4
				current_anim = 4;
			break;
			case BTN_5: // 5
				current_anim = 5;
			break;
			case BTN_6: // 6
				current_anim = 6;
			break;
			case BTN_7: // 7
				current_anim = 7;
			break;
			case BTN_8: // 8
				current_anim = 8;
			break;
			case BTN_9: // 9
				current_anim = 9;
			break;
			case BTN_CH_DWN: // Brightness down
				global_val = max(FastLED.getBrightness(),10);
				FastLED.setBrightness(uint8_t(float(global_val)*0.4));
			break;
			case BTN_CH_UP: // Brightness up
				global_val = max(FastLED.getBrightness(),10);
				FastLED.setBrightness(uint8_t(min(float(global_val)*2.5,255)));
			break;
			case BTN_RED: // red
				current_anim = 1;
				anim_color = CRGB::Red;
			break;
			case BTN_GREEN: // green
				current_anim = 1;
				anim_color = CRGB::Green;
			break;	
			case BTN_WHITE: // white
				current_anim = 1;
				anim_color = CRGB::White;
			break;
			case BTN_BLUE: // blue
				current_anim = 1;
				anim_color = CRGB::Blue;
			break;
			case BTN_MUTE: // Toggle strip power
				current_anim = const_color;
				if (anim_color){
					anim_color = CRGB::Black;
				}else{
					anim_color = init_color;
				}
			break;
			case BTN_VOL_UP: // Speed up
				speed_increase = true;
			break;
			case BTN_VOL_DOWN: // Speed down
				speed_decrease = true;
			break;
			case BTN_ENTER:   // Toggle 220V light bulb
				digitalWrite(BULB_RELAY_PIN,!digitalRead(BULB_RELAY_PIN));
			break;
		}
		#if defined(DEBUG)
  			Serial.println("Current animation code = "+String(current_anim));
		#endif
}

//----------------------------- Receive 32-bit code over I2C
void IR_callback(int n_Bytes){ 
  byte irArray[4];
  uint32_t irCode=0;
  #if defined(DEBUG)
	Serial.println("Received "+String(n_Bytes)+" bytes.");
  #endif
  for(int i=0; i<4; i++){
	irArray[i] = Wire.read();
  }
  
  // Build 32-bit uint from the 4 transmitted bits.
  irCode += (long)irArray[3]<<24;
  irCode += (long)irArray[2]<<16;
  irCode += (long)irArray[1]<<8;
  irCode += (long)irArray[0];
  #if defined(DEBUG)
	Serial.println(irCode,HEX);
  #endif
  
  interpret_IR_signal(irCode);
}
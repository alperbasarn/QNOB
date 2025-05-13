// On a Trinket or Gemma we suggest changing this to 1:
#define LED_PIN 9
// How many NeoPixels are attached to the Arduino?
#define LED_COUNT 50
// Declare the NeoPixel strip object
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_RGB + NEO_KHZ800);

bool IncreaseAnimating = false;  // Tracks if increase animation is active
bool DecreaseAnimating = false;  // Tracks if decrease animation is active
// Timing variables for animations
unsigned long previousMillis = 0;   // Timing control variable
const unsigned long interval = 5;  // Interval between LED updates
// Animation state tracking variables
int currentIncreaseLED = 0;
int currentDecreaseLED = LED_COUNT - 1;

// Helper function to set the color of all pixels
void writeColor(uint8_t red, uint8_t green, uint8_t blue) {
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(red, green, blue));
  }
  strip.show();  // Ensure changes are applied immediately
}

void writeWhite() {
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(255, 255, 255, 255));
    delay(1);
  }
  strip.show();  // Ensure changes are applied immediately
}


// Function to set the color
void setColor(int i) {
  currentColor = i;
  switch (i) {
    case 1:
      writeColor(255, 0, 0);  // Set all pixels to red
      break;
    case 2:
      writeColor(0, 255, 0);  // Set all pixels to green
      break;
    case 3:
      writeColor(0, 0, 255);  // Set all pixels to blue
      break;
    case 4:
      writeWhite();  // Set all pixels to white
      break;
    case 5:
      writeColor(255, 255, 0);  // Set all pixels to yellow
      break;
  }
}

// Function to cycle to the next color
void nextColor() {
  currentColor = (currentColor + 1) % 5;  // Move to the next color, wrap around after yellow
  setColor(currentColor);                 // Set all pixels to the new current color
}

void initLED() {

  strip.begin();  // Initialize NeoPixel strip object
  strip.show();   // Turn off all pixels ASAP
  strip.setBrightness(10);

  // Precompute the colors for each LED
  uint8_t red[LED_COUNT], green[LED_COUNT], blue[LED_COUNT];
  for (int pixel = 0; pixel < LED_COUNT; pixel++) {
    float position = (float)pixel / (LED_COUNT - 1);  // Position in the strip

    if (position < 0.2) {
      // Red to Yellow
      red[pixel] = 255;
      green[pixel] = map(position * 10, 0, 2, 0, 255);
      blue[pixel] = 0;
    } else if (position < 0.4) {
      // Yellow to Green
      red[pixel] = map(position * 10, 2, 4, 255, 0);
      green[pixel] = 255;
      blue[pixel] = 0;
    } else if (position < 0.6) {
      // Green to Cyan
      red[pixel] = 0;
      green[pixel] = 255;
      blue[pixel] = map(position * 10, 4, 6, 0, 255);
    } else if (position < 0.8) {
      // Cyan to Blue
      red[pixel] = 0;
      green[pixel] = map(position * 10, 6, 8, 255, 0);
      blue[pixel] = 255;
    } else {
      // Blue to Violet
      red[pixel] = map(position * 10, 8, 10, 0, 255);
      green[pixel] = 0;
      blue[pixel] = 255;
    }
  }

  // Animation loop for simultaneous glowing
  for (int t = 0; t < 1000; t += 20) {  // Run for 5000 ms (5 seconds) in steps of 20 ms
    for (int pixel = 0; pixel < LED_COUNT; pixel++) {
      // Frequency range from 5 Hz to 15 Hz
      float frequency = 5.0 + (10.0 * (float)pixel / (LED_COUNT - 1));
      float brightnessFactor = 170 + (sin(t * frequency * 2 * PI / 1000) * 30 + 30);  // Fluctuate between 10 and 70

      // Set the color with the fluctuated brightness
      strip.setPixelColor(pixel, strip.Color(
                                   (red[pixel] * brightnessFactor) / 255,
                                   (green[pixel] * brightnessFactor) / 255,
                                   (blue[pixel] * brightnessFactor) / 255));
    }
    strip.show();
    delay(10);  // 20 ms delay for smooth animation
  }
  setColor(1);
  strip.setBrightness(3);
  strip.show();
}

void updateLED() {
  //strip.setBrightness(brightness);
  //setColor(currentColor);
  //strip.show();
  if (IncreaseAnimating) {
    if (increaseAnimate()) {
      IncreaseAnimating = false;
    }
  } else if (DecreaseAnimating) {
    if (decreaseAnimate()) {
      DecreaseAnimating = false;
    }
  }
}


// Method to enable increase animation
void setIncreaseAnimate() {
  if (!IncreaseAnimating && !DecreaseAnimating) {
    IncreaseAnimating = true;
    currentIncreaseLED = 0;  // Reset the LED counter for the animation
  }
}

// Method to enable decrease animation
void setDecreaseAnimate() {
  if (!IncreaseAnimating && !DecreaseAnimating) {
    DecreaseAnimating = true;
    currentDecreaseLED = LED_COUNT - 1;  // Reset the LED counter for the animation
  }
}

// Non-blocking increase animation
bool increaseAnimate() {
  strip.setBrightness(25);  // Maximum brightness
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    strip.clear();  // Clear all LEDs

    // Turn on 3 LEDs around the current LED
    for (int i = currentIncreaseLED; i < currentIncreaseLED + 3; i++) {
      if (i < LED_COUNT) {  // Ensure the index is within bounds
        strip.setPixelColor(i, strip.Color(255, 0, 0));  // Red color
      }
    }
    strip.show();

    currentIncreaseLED++;
    if (currentIncreaseLED >= LED_COUNT) {
      IncreaseAnimating = false;  // Animation complete
      strip.clear();
      strip.show();
      return true;  // Return true when animation finishes
    }
  }
  return false;  // Return false if animation is still ongoing
}

// Non-blocking decrease animation
bool decreaseAnimate() {
  strip.setBrightness(25);  // Maximum brightness
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    strip.clear();  // Clear all LEDs

    // Turn on 3 LEDs around the current LED
    for (int i = currentDecreaseLED; i > currentDecreaseLED - 3; i--) {
      if (i >= 0) {  // Ensure the index is within bounds
        strip.setPixelColor(i, strip.Color(0, 255, 0));  // Green color
      }
    }
    strip.show();

    currentDecreaseLED--;
    if (currentDecreaseLED < 0) {
      DecreaseAnimating = false;  // Animation complete
      strip.clear();
      strip.show();
      return true;  // Return true when animation finishes
    }
  }
  return false;  // Return false if animation is still ongoing
}


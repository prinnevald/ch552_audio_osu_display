
#define BUTTON_Z 31  // P3.1
#define BUTTON_X 11  // P1.1
#define BUZZER   30  // P3.0

unsigned long lastToggle = 0;
unsigned int halfPeriod = 0;

unsigned char lastMode = 255;
unsigned char buzzerState = LOW;

void setup() {
  pinMode(BUTTON_Z, INPUT_PULLUP);
  pinMode(BUTTON_X, INPUT_PULLUP);

  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);
}

void loop() {
  // Buttons are active LOW
  unsigned char z = (digitalRead(BUTTON_Z) == LOW);
  unsigned char x = (digitalRead(BUTTON_X) == LOW);

  unsigned char mode = (z ? 1 : 0) | (x ? 2 : 0);

  unsigned long now = micros();

  // Select tone based on buttons
  if (mode != lastMode) {
    lastMode = mode;

    switch (mode) {
      case 1:
        halfPeriod = 625; // 800 Hz
        break;

      case 2:
        halfPeriod = 313; // ~1600 Hz
        break;

      case 3:
        halfPeriod = 208; // ~2400 Hz
        break;

      default:
        halfPeriod = 0;   // Silence
        break;
    }

    buzzerState = LOW;
    digitalWrite(BUZZER, LOW);
    lastToggle = now;
  }

  // Generate square wave without blocking
  if (halfPeriod > 0 &&
      (now - lastToggle) >= halfPeriod) {

    lastToggle = now;
    buzzerState = !buzzerState;
    digitalWrite(BUZZER, buzzerState);
  }
}

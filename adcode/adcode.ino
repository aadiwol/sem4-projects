int xPin = A1;             // Analog pin for X-axis
int yPin = A0;             // Analog pin for Y-axis
int buttonPin = 7;         // Digital pin for button (switch)
int xVal;                  // Variable to store X-axis value
int yVal;                  // Variable to store Y-axis value
int buttonState;           // Variable to store button (switch) state

void setup() {
  pinMode(xPin, INPUT);            // Not strictly needed for analog pins
  pinMode(yPin, INPUT);            // Not strictly needed for analog pins
  pinMode(buttonPin, INPUT_PULLUP); // Use internal pull-up resistor for button
  Serial.begin(9600);              // Start serial communication
}

void loop() {
  xVal = analogRead(xPin);                   // Read X-axis value
  yVal = analogRead(yPin);                   // Read Y-axis value
  buttonState = digitalRead(buttonPin);      // Read button (switch) state

  Serial.print("X: ");
  Serial.print(xVal);
  Serial.print(" | Y: ");
  Serial.print(yVal);
  Serial.print(" | Switch: ");
  Serial.println(buttonState);

  delay(200);                                // Small delay
}

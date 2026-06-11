
const int pinLW = 12;   
const int pinRW = 13;   
const int sensorPins[] = {A7, A6, A3, A2, A1, A0}; 

// --- ENCODER PINS ---
const int leftEncA = 2;  
const int rightEncA = 3; 
const int leftEncB = 4;  
const int rightEncB = 5; 

// --- MOTOR PINS (MX1616) ---
const int leftMotorPWM = 6;   
const int leftMotorDir = 7;   
const int rightMotorPWM = 9;  
const int rightMotorDir = 10; 

// 
float Kp = 16.0; 
float Kd = 10.0;

int baseSpeed = 90;      
int turnSpeed = 130;     
int slowSpeed = 60;      
int threshold = 500;     



const long INCH_TICKS = 120; 


const long TURN_90_TICKS = 320; 


const long GAP_TIMEOUT = 400; 

// --- STATE MACHINE ---
enum State { FOLLOW, JUNCTION_HANLDING, TURN_LEFT, TURN_RIGHT, UTURN, FINISHED };
State state = FOLLOW;

// --- VARIABLES ---
volatile long leftEncoderCount = 0;
volatile long rightEncoderCount = 0;
int lastError = 0;
int sensorValues[8]; 

// Logic flags
unsigned long gapTimer = 0;
bool inGap = false;
unsigned long startTime = 0;

void setup() {
  Serial.begin(115200);
  
  // Setup Pins
  pinMode(pinLW, INPUT); pinMode(pinRW, INPUT);
  for(int i=0; i<6; i++) pinMode(sensorPins[i], INPUT);
  
  pinMode(leftEncA, INPUT); pinMode(leftEncB, INPUT);
  pinMode(rightEncA, INPUT); pinMode(rightEncB, INPUT);
  
  attachInterrupt(digitalPinToInterrupt(leftEncA), updateLeftEncoder, RISING);
  attachInterrupt(digitalPinToInterrupt(rightEncA), updateRightEncoder, RISING);
  
  pinMode(leftMotorPWM, OUTPUT); pinMode(leftMotorDir, OUTPUT);
  pinMode(rightMotorPWM, OUTPUT); pinMode(rightMotorDir, OUTPUT);
  
  Serial.println("READY. Waiting 3 seconds...");
  delay(3000);
  Serial.println("GO!");
  startTime = millis(); // Record start time
}

void loop() {
  readSensors();
  
  switch(state) {
    case FOLLOW:
      runLineFollower();
      break;
    
    case JUNCTION_HANLDING:
      handleJunction();
      break;
      
    case TURN_LEFT:
      executeHybridTurn('L');
      break;
      
    case TURN_RIGHT:
      executeHybridTurn('R');
      break;
      
    case UTURN:
      executeUTurn();
      break;
      
    case FINISHED:
      stopMotors();
      break;
  }
}

void runLineFollower() {
  
  if (millis() - startTime > 3000) {
    if (sensorValues[2] == 1 && sensorValues[3] == 1 && isCenterSolid()) {
      state = FINISHED;
      return;
    }
  }

  if (sensorValues[2] == 1 || sensorValues[3] == 1) {
    state = JUNCTION_HANLDING;
    resetEncoders();
    return;
  }

  // C. GAP / DEAD END HANDLING
  if (isAllWhite()) {
    if (!inGap) {
      inGap = true;
      gapTimer = millis();
    }
    
    // If we've been in white too long -> It's a Dead End
    if (millis() - gapTimer > GAP_TIMEOUT) {
      state = UTURN;
      inGap = false;
      return;
    }
    
    // Otherwise, it's a DASHED LINE gap.
    // Drive STRAIGHT blindly (Flywheel effect)
    setMotorSpeed(baseSpeed, baseSpeed);
    return;
    
  } else {
    // We see the line, reset gap timer
    inGap = false; 
  }

  // D. NORMAL PID CONTROL
  int error = calculateError();
  int correction = (error * Kp) + ((error - lastError) * Kd);
  lastError = error;

  int L = constrain(baseSpeed + correction, -255, 255);
  int R = constrain(baseSpeed - correction, -255, 255);

  setMotorSpeed(L, R);
}

// ==========================================================
// 2. JUNCTION LOGIC (Left Hand Rule)
// ==========================================================
void handleJunction() {
  // 1. Inch forward to align wheels with the turn
  //    (This puts the sensors AHEAD of the junction, into the empty space)
  setMotorSpeed(slowSpeed, slowSpeed);
  
  long avgDist = 0;
  while(avgDist < INCH_TICKS) {
    avgDist = (abs(leftEncoderCount) + abs(rightEncoderCount)) / 2;
    // Safety break
    if (isAllWhite() && avgDist > INCH_TICKS/2) break; 
  }
  
  stopMotors();
  delay(100); // Stabilize
  readSensors(); // Read what is directly under us now

  // 2. DECIDE TURN (Left Hand Rule)
  // Since we inched forward, we need to rely on what triggered the event
  // OR check if we can turn left now.
  
  
  // Let's sweep:
  // We can't see the Left Wing anymore because we drove past it.
  // But we know we entered this state because a wing was triggered.
  
  // SIMPLIFIED LOGIC:
  // We will blindly turn LEFT if we suspect a left path, then scan.
  // Actually, let's use the "Check Turn" spin method.
  
  // Since we assume Left Hand Rule, let's try to find a line to the Left.
  state = TURN_LEFT; // Default to Left priority
}

// ==========================================================
// 3. HYBRID TURNS (The "Catch" Logic)
// ==========================================================
void executeHybridTurn(char dir) {
  resetEncoders();
  
  // PHASE 1: BLIND SPIN
  // Spin 70% of a 90-degree turn using encoders.
  // This gets us off the current line and close to the new one.
  long blindTarget = TURN_90_TICKS * 0.7; 
  
  if (dir == 'L') setMotorSpeed(-turnSpeed, turnSpeed);
  else setMotorSpeed(turnSpeed, -turnSpeed);

  long avgDist = 0;
  while(avgDist < blindTarget) {
    avgDist = (abs(leftEncoderCount) + abs(rightEncoderCount)) / 2;
  }

  // PHASE 2: SENSOR CATCH
  // Continue spinning until the CENTER sensors see black.
  // Slow down slightly for precision.
  if (dir == 'L') setMotorSpeed(-slowSpeed, slowSpeed);
  else setMotorSpeed(slowSpeed, -slowSpeed);
  
  unsigned long timeout = millis();
  while(true) {
    readSensors();
    // If center sensors (S3 or S4) see black, STOP.
    if (sensorValues[4] == 1 || sensorValues[5] == 1) {
      break;
    }
    
    // Safety: If we spin too long (360 deg?), give up and go straight
    if (millis() - timeout > 2000) break;
  }
  
  stopMotors();
  delay(100);
  state = FOLLOW; // Resume following
  resetEncoders();
}

void executeUTurn() {
  resetEncoders();
  setMotorSpeed(turnSpeed, -turnSpeed);
  
  // For U-Turn, we trust encoders more because we are usually in a dead end
  long target = TURN_90_TICKS * 2.1; 
  
  long avgDist = 0;
  while(avgDist < target) {
    avgDist = (abs(leftEncoderCount) + abs(rightEncoderCount)) / 2;
  }
  
  stopMotors();
  delay(100);
  state = FOLLOW;
}

void readSensors() {
  sensorValues[2] = digitalRead(pinLW);
  sensorValues[3] = digitalRead(pinRW);
  sensorValues[0] = (analogRead(sensorPins[0]) > threshold) ? 1 : 0;
  sensorValues[1] = (analogRead(sensorPins[1]) > threshold) ? 1 : 0;
  sensorValues[4] = (analogRead(sensorPins[2]) > threshold) ? 1 : 0;
  sensorValues[5] = (analogRead(sensorPins[3]) > threshold) ? 1 : 0;
  sensorValues[6] = (analogRead(sensorPins[4]) > threshold) ? 1 : 0;
  sensorValues[7] = (analogRead(sensorPins[5]) > threshold) ? 1 : 0;
}

bool isAllWhite() {
  for(int i=0; i<8; i++) if(sensorValues[i] == 1) return false;
  return true;
}

bool isCenterSolid() {
  int count = 0;
  // Check inner 6 sensors
  if(sensorValues[0]) count++;
  if(sensorValues[1]) count++;
  if(sensorValues[4]) count++;
  if(sensorValues[5]) count++;
  if(sensorValues[6]) count++;
  if(sensorValues[7]) count++;
  return (count >= 4);
}

int calculateError() {
  long avg = 0;
  int sum = 0;
  // Use indices 0,1,4,5,6,7 (The array sensors)
  // Mapping positions: -3, -2, -1, 1, 2, 3
  int pos[] = {-30, -20, -10, 10, 20, 30}; 
  int map[] = {0, 1, 4, 5, 6, 7};
  
  for(int i=0; i<6; i++) {
    if(sensorValues[map[i]] == 1) {
      avg += pos[i];
      sum++;
    }
  }
  
  if(sum == 0) return lastError;
  return avg / sum;
}

// --- MOTOR & ENCODER ---
void setMotorSpeed(int L, int R) {
  if (L >= 0) { digitalWrite(leftMotorDir, HIGH); analogWrite(leftMotorPWM, L); }
  else { digitalWrite(leftMotorDir, LOW); analogWrite(leftMotorPWM, abs(L)); }
  
  if (R >= 0) { digitalWrite(rightMotorDir, LOW); analogWrite(rightMotorPWM, R); }
  else { digitalWrite(rightMotorDir, HIGH); analogWrite(rightMotorPWM, abs(R)); }
}

void stopMotors() {
  setMotorSpeed(0, 0);
}

void resetEncoders() {
  leftEncoderCount = 0;
  rightEncoderCount = 0;
}

void updateLeftEncoder() {
  if (digitalRead(leftEncB) == HIGH) leftEncoderCount++; else leftEncoderCount--;
}

void updateRightEncoder() {
  if (digitalRead(rightEncB) == HIGH) rightEncoderCount++; else rightEncoderCount--;
}

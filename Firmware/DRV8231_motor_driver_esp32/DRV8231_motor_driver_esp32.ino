#include <Arduino.h>

// Motor control pins (connected to IN1 and IN2 on DRV8231A)
const int IN1 = 4;  // GPIO pin connected to IN1
const int IN2 = 5;  // GPIO pin connected to IN2

// Current sense pin (ADC pin connected to ISENSE output through buffer)
const int CURRENT_SENSE_PIN = 33;  // GPIO pin connected to current sense buffer

// PWM control for speed (optional)
const int PWM_PIN = 25;  // PWM pin for speed control (use any PWM-capable pin)

// Encoder pins (connected to phase A and phase B of the encoder)
const int ENCODER_A = 21;  // GPIO pin connected to encoder phase A
const int ENCODER_B = 22;  // GPIO pin connected to encoder phase B

// Motor speed (0 to 255 for PWM control)
int motorSpeed = 128;  // 50% duty cycle

volatile int encoderPosition = 0;  // To store the position of the encoder
volatile int lastEncoderStateA = LOW;
volatile float motorCurrent;
volatile unsigned long lastInterruptTime = 0;
const unsigned long debounceDelay = 5;  // Adjust as needed

void IRAM_ATTR encoderISR() {
  // unsigned long currentInterruptTime = millis();
  // if (currentInterruptTime - lastInterruptTime > debounceDelay) {
    int stateA = digitalRead(ENCODER_A);
    int stateB = digitalRead(ENCODER_B);
    
    if (stateA != lastEncoderStateA) {
      if (stateB != stateA) {
        encoderPosition++;
      } else {
        encoderPosition--;
      }
    }
    
    lastEncoderStateA = stateA;
    // lastInterruptTime = currentInterruptTime;
  // }
}

void motorControlTask(void *pvParameters) {
   digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
  while (1) 
  {
    if(motorCurrent >= 350)
    {
      Serial.print("Stop");
      digitalWrite(IN1, LOW);
      digitalWrite(IN2, LOW);
    }
    // Forward motor direction with 50% speed
    // digitalWrite(IN1, HIGH);
    // digitalWrite(IN2, LOW);
    // ledcWrite(0, motorSpeed);

    // // Run for 5 seconds
    // vTaskDelay(5000 / portTICK_PERIOD_MS);

    // // Reverse motor direction
    // digitalWrite(IN1, LOW);
    // digitalWrite(IN2, HIGH);

    // // Run for another 5 seconds
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

void setup() {
  // Initialize serial communication for debugging
  Serial.begin(115200);

  // Set up motor control pins
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
   pinMode(CURRENT_SENSE_PIN, INPUT);

  // Set up encoder pins
 pinMode(ENCODER_A, INPUT_PULLUP);
pinMode(ENCODER_B, INPUT_PULLUP);

  // Attach interrupts to the encoder
  attachInterrupt(digitalPinToInterrupt(ENCODER_A), encoderISR, FALLING);

  // Configure PWM frequency and resolution
  ledcSetup(0, 1000, 8);  // Channel 0, 1 kHz frequency, 8-bit resolution
  ledcAttachPin(PWM_PIN, 0);  // Attach PWM pin to channel 0

  // Create a task for motor control
  xTaskCreate(motorControlTask, "Motor Control", 2048, NULL, 1, NULL);

  // Wait for setup to complete
  delay(1000);
}

void loop() {
  // Read the current sense voltage (scaled between 0 and 3.3V)
  uint16_t currentSenseValue = analogRead(CURRENT_SENSE_PIN);
  
  // Convert the ADC value (0 to 4095 for ESP32's 12-bit ADC) to current
  float voltage = (currentSenseValue / 4095.0) * 3.3;  // Convert to voltage
  motorCurrent = voltage / 0.72;  // Assuming 0.72 ohm current sense resistor (R6)
  motorCurrent *= 1000.0; // to convert into mA 
  // Print the motor current
  Serial.print("Motor Current: ");
  Serial.print(motorCurrent, 4);
  Serial.println(" A");

  

  // Print the encoder position
  Serial.print("Encoder Position: ");
  Serial.println(encoderPosition);

  // Wait for 500ms before next reading
  delay(500);
}

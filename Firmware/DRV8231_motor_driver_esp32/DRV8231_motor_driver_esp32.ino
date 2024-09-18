#include <Wire.h>
#include <Adafruit_ADS1X15.h>

// Motor control pins (connected to IN1 and IN2 on DRV8231A)
const int IN1 = 4;  // GPIO pin connected to IN1
const int IN2 = 5;  // GPIO pin connected to IN2

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

// Create an ADS1115 object
Adafruit_ADS1115 ads;

// Current limit in mA
const float currentLimit = 7.0;  // 7.0 mA

bool motorStopped = false;  // Flag to indicate if the motor has been stopped

void IRAM_ATTR encoderISR() {
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
}

void motorControlTask(void *pvParameters) {
    while (1) {
        // Skip motor operation if it has been stopped due to current overload
        if (motorStopped) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);  // Wait before checking again
            continue;
        }

        // Forward motor direction with 50% speed
        digitalWrite(IN1, HIGH);
        digitalWrite(IN2, LOW);
        ledcWrite(0, motorSpeed);

        // Run for 5 seconds
        vTaskDelay(5000 / portTICK_PERIOD_MS);

        // Reverse motor direction
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, HIGH);

        // Run for another 5 seconds
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

void setup() {
    // Initialize serial communication for debugging
    Serial.begin(115200);

    // Initialize the ADS1115
    ads.setGain(GAIN_TWOTHIRDS);  // Adjust gain as needed
    ads.begin();

    // Set up motor control pins
    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);

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
    // Read the current sense value from ADS1115
    int16_t currentSenseValue = ads.readADC_SingleEnded(0);  // Read from A0
    float voltage = currentSenseValue * 0.1875 / 1000.0;  // Convert to voltage (0.1875mV per bit)

    motorCurrent = voltage / 0.72;  // Assuming 0.72 ohm current sense resistor (R6)
    motorCurrent *= 1000.0;  // Convert to mA

    // Check if motor current exceeds the limit
    if (motorCurrent > currentLimit && !motorStopped) {
        // Stop the motor by setting both IN1 and IN2 to LOW
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, LOW);
        motorStopped = true;  // Set the flag to prevent restarting the motor
        Serial.println("Current limit exceeded! Motor stopped.");
    }

    // Print the motor current
    Serial.print("Motor Current: ");
    Serial.print(motorCurrent);
    Serial.println(" mA");

    // Print the encoder position
    Serial.print("Encoder Position: ");
    Serial.println(encoderPosition);

    // Wait for 500ms before the next reading
    delay(500);
}

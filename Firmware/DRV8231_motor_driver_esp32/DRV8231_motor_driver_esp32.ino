#include <Wire.h>
#include <Adafruit_ADS1X15.h>

// Motor control pins (connected to IN1 and IN2 on DRV8231A)
const int IN1 = 4;  // GPIO pin connected to IN1
const int IN2 = 5;  // GPIO pin connected to IN2

// PWM control for speed (optional)
// const int PWM_PIN = 25;  // PWM pin for speed control (use any PWM-capable pin)

// Encoder pins (connected to phase A and phase B of the encoder)
const int ENCODER_A = 21;  // GPIO pin connected to encoder phase A
const int ENCODER_B = 22;  // GPIO pin connected to encoder phase B

// Motor speed (0 to 255 for PWM control)
int motorSpeed = 128;  // 50% duty cycle

volatile int encoderPosition = 0;  // To store the position of the encoder
volatile int lastEncoderStateA = LOW;
volatile float motorCurrent;
volatile float motorCurrent_2;
volatile unsigned long lastInterruptTime = 0;
const unsigned long debounceDelay = 5;  // Adjust as needed

// Create an ADS1115 object
Adafruit_ADS1115 ads;

// Current limit in mA
const float currentLimit = 160.0;  // Set a threshold for motor current // 140 9v, 185 11V, 
const float voltageLimit = 60.0;

bool motorStopped = false;  // Flag to indicate if the motor has been stopped
bool ccwFlag = false;       // Tracks CCW rotation
bool cwFlag = false;        // Tracks CW rotation

// ISR for handling encoder signals
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

// Motor control functions
void CWDirection() {
    if (!motorStopped) {
        ccwFlag = false;
        cwFlag = true;
        digitalWrite(IN1, HIGH);  // CW direction
        digitalWrite(IN2, LOW);
        //ledcWrite(0, motorSpeed);  // Set speed
    }
}

void CCWDirection() {
    if (!motorStopped) 
    {
        ccwFlag = true;
        cwFlag = false;
        digitalWrite(IN1, LOW);  // CCW direction
        digitalWrite(IN2, HIGH);
        //ledcWrite(0, motorSpeed);  // Set speed
    }
}

void stopMotor() {
    digitalWrite(IN1, LOW);  // Stop motor
    digitalWrite(IN2, LOW);
   // ledcWrite(0, 0);  // Set speed to 0
}

void motorControlTask(void *pvParameters) {
    while (1) 
    {
        if (motorStopped) 
        {
            stopMotor();  // Stop the motor if the current exceeds the limit
            vTaskDelay(1000 / portTICK_PERIOD_MS);  // Wait before checking again
            continue;
        }

        if (cwFlag) 
        {
            CWDirection();  // Keep motor in CW direction
        } 
        else 
        {
            CCWDirection();  // Keep motor in CCW direction
        }
        
        vTaskDelay(100 / portTICK_PERIOD_MS);  // Adjust delay for smoother control
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
   // ledcSetup(0, 50, 8);  // Channel 0, 1 kHz frequency, 8-bit resolution


    // Create a task for motor control
    xTaskCreate(motorControlTask, "Motor Control", 2048, NULL, 1, NULL);

    // Wait for setup to complete
    delay(1000);
}

void loop() 
{
    // Read the current sense value from ADS1115
    int16_t currentSenseValue = ads.readADC_SingleEnded(0);  // Read from A0
    int16_t currentSenseValue_2 = ads.readADC_SingleEnded(2);  // Read from A0
    float voltage = currentSenseValue * 0.1875 / 1000.0;  // Convert to voltage (0.1875mV per bit)
    float voltage_2 = currentSenseValue_2 * 0.1875 / 1000.0;  // Convert to voltage (0.1875mV per bit)

    motorCurrent = voltage / 0.72;  // Assuming 0.72 ohm current sense resistor
    motorCurrent_2 = voltage_2 / 0.72;  // Assuming 0.72 ohm current sense resistor
    motorCurrent *= 1000.0;  // Convert to mA
    motorCurrent_2 *= 1000.0;  // Convert to mA
    voltage = voltage*1000.0;
    voltage_2 = voltage_2 * 1000.0;
    // Print the motor current
    Serial.print("Motor ADC Voltage 1 : ");
    Serial.print(voltage);
    Serial.println(" mV");

    Serial.print("Motor ADC Voltage 2 : ");
    Serial.print(voltage_2);
    Serial.println(" mV");
    // Serial.print("Motor Current 2: ");
    // Serial.print(motorCurrent_2);
    // Serial.println(" mA");

    // Check if motor current exceeds the limit
   // if (motorCurrent > currentLimit && !motorStopped)
   //if (voltage < 95.0 && voltage > voltageLimit && !motorStopped) 
    if (voltage > voltageLimit && !motorStopped) 
    {
        //delay (1000);
        motorStopped = true;
        Serial.println("Current limit exceeded! Motor stopped.");
        //delay to see motor is stop
        delay (5000);
        // Reverse direction after stopping
        if (cwFlag) 
        {
            ccwFlag = true;
            cwFlag = false;
        } 
        else if (ccwFlag) 
        {
            cwFlag = true;
            ccwFlag = false;
        }
        delay(2000);// to limit starting current that should be less than starting torque 
        motorStopped = false;
        if (voltage > voltageLimit && !motorStopped)
          delay(2000);
        //delay(1000);
    }

    

    // // Print the encoder position
    // Serial.print("Encoder Position: ");
    // Serial.println(encoderPosition);

    // Wait for 500ms before the next reading
   // delay(500);
}

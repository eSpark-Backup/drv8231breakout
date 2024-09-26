#include <Wire.h>
#include <Adafruit_ADS1X15.h>

const int IN1 = 4;  // Motor control pins (connected to IN1 and IN2 on DRV8231A)
const int IN2 = 5;  

const int ENCODER_A = 21;  //  encoder phase A
const int ENCODER_B = 22;  // encoder phase B

// Motor control flags
volatile bool motorStopped = false;
volatile bool ccwFlag = false;
volatile bool cwFlag = false;

// Encoder variables
volatile int encoderPosition = 0; 
volatile int lastEncoderStateA = LOW;

const unsigned long debounceDelay = 5;  // Debounce delay for interrupts (in milliseconds)
volatile unsigned long lastInterruptTime = 0;


const float voltageLimit = 35.0;  // Rsense voltage limit in mV for motor control

Adafruit_ADS1115 ads;


void IRAM_ATTR encoderISR() // ISR for handling encoder signals
{
    int stateA = digitalRead(ENCODER_A);
    int stateB = digitalRead(ENCODER_B);

    // Check for encoder direction
    if (stateA != lastEncoderStateA) 
    {
        if (stateB != stateA) 
        {
            encoderPosition++;  // CW direction
        } else 
        {
            encoderPosition--;  // CCW direction
        }
    }
    lastEncoderStateA = stateA;  // Update last state
}

// Function to control the motor in CW direction
void CWDirection() 
{
    if (!motorStopped) 
    {
        ccwFlag = false;
        cwFlag = true;
        digitalWrite(IN1, HIGH);  // Set IN1 high for CW rotation
        digitalWrite(IN2, LOW);   // Set IN2 low
    }
}

// Function to control the motor in CCW direction
void CCWDirection() 
{
    if (!motorStopped) 
    {
        ccwFlag = true;
        cwFlag = false;
        digitalWrite(IN1, LOW);   // Set IN1 low for CCW rotation
        digitalWrite(IN2, HIGH);  // Set IN2 high
    }
}

// Function to stop the motor
void stopMotor() 
{
    digitalWrite(IN1, LOW);  // Set both IN1 and IN2 low to stop motor
    digitalWrite(IN2, LOW);
}

// Task to control the motor based on direction flags
void motorControlTask(void *pvParameters) 
{
    while (1) 
    {
        if (motorStopped) 
        {
            stopMotor();  // Stop motor if stopped flag is set
        } 
        else if (cwFlag) 
        {
            CWDirection();  // Rotate CW if cwFlag is set
        } 
        else 
        {
            CCWDirection();  // Rotate CCW if ccwFlag is set
        }

        // Small delay to allow smoother motor control
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

// Function to handle overcurrent detection and motor reversal
void handleMotorOperation(float voltage1, float voltage2) 
{
    if ((voltage1 > voltageLimit || voltage2 > voltageLimit) && !motorStopped) 
    {
        motorStopped = true;
        Serial.println("Current limit exceeded! Motor stopped.");

        delay(5000);  // Pause to let motor stop completely

        // Reverse motor direction after stop
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

        delay(2000);  // Delay before restarting in opposite direction
        motorStopped = false;
    }
}

// Setup function
void setup() 
{
    Serial.begin(115200);  
    // Initialize the ADS1115 ADC
    ads.setGain(GAIN_TWOTHIRDS);  // Adjust gain as needed
    ads.begin();

    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);

    pinMode(ENCODER_A, INPUT_PULLUP);
    pinMode(ENCODER_B, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(ENCODER_A), encoderISR, FALLING);
    xTaskCreate(motorControlTask, "Motor Control", 2048, NULL, 1, NULL);
    delay(1000);  
}

// Main loop function
void loop() 
{
    int16_t currentSenseValue1 = ads.readADC_SingleEnded(0);  // Read from A0
    int16_t currentSenseValue2 = ads.readADC_SingleEnded(2);  // Read from A2

    
    float voltage1 = currentSenseValue1 * 0.1875;  // 0.1875mV per bit
    float voltage2 = currentSenseValue2 * 0.1875; // Convert ADC readings to voltage (mV)

    Serial.print("Motor ADC Voltage 1: ");
    Serial.print(voltage1);
    Serial.println(" mV");

    Serial.print("Motor ADC Voltage 2: ");
    Serial.print(voltage2);
    Serial.println(" mV");

    // Handle motor stop and direction reversal if current exceeds the limit
    handleMotorOperation(voltage1, voltage2);
}

#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/kernels/fully_connected.h"
#include "tensorflow/lite/micro/kernels/softmax.h"
#include "model.h"  // Include your quantized .tflite model

// Globals for TensorFlow Lite
namespace {
    const tflite::Model* model = nullptr;
    tflite::MicroInterpreter* interpreter = nullptr;
    TfLiteTensor* input = nullptr;
    TfLiteTensor* output = nullptr;

    constexpr int kTensorArenaSize = 16000;
    uint8_t tensor_arena[kTensorArenaSize];

    // GPIO pin for the on-board LED
    const int kOnBoardLED = 2; // GPIO 2 on ESP32
}

// ADS1115 instance
Adafruit_ADS1115 ads;

// Function to initialize GPIO pin for the on-board LED
void InitGPIO() {
    pinMode(kOnBoardLED, OUTPUT);
    // Ensure the on-board LED is off initially
    digitalWrite(kOnBoardLED, LOW);
}

void setup() {
    // Initialize Serial
    Serial.begin(115200);

    // Initialize GPIO pins
    InitGPIO();

    // Initialize ADS1115
    if (!ads.begin()) {
        Serial.println("Failed to initialize ADS1115!");
        while (1);  // Halt execution
    }

    Serial.println("ADS1115 initialized.");

    // Load TensorFlow Lite model
    model = tflite::GetModel(eye_movement_model_tflite);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        Serial.println("Model schema version mismatch.");
        return;
    }

    // Set up the op resolver
    static tflite::MicroMutableOpResolver<4> resolver; // Adjust size to match the total number of required ops
    if (resolver.AddFullyConnected() != kTfLiteOk || 
        resolver.AddSoftmax() != kTfLiteOk) {  // No need for quantization ops
        Serial.println("Failed to add required ops.");
        while (1);  // Halt execution
    }
    
    // Set up TensorFlow Lite interpreter
    static tflite::MicroInterpreter static_interpreter(model, resolver, tensor_arena, kTensorArenaSize);
    interpreter = &static_interpreter;

    if (interpreter->AllocateTensors() != kTfLiteOk) {
        Serial.println("AllocateTensors failed.");
        while (1);  // Halt execution
    }

    input = interpreter->input(0);
    output = interpreter->output(0);

    Serial.println("Setup complete.");
}

void loop() {
    // Read and scale sensor input
    int16_t raw_adc_value = ads.readADC_SingleEnded(0);  // Reading from channel 0
    float sensor_input = raw_adc_value * 0.1875 / 1000.0;  // Convert to voltage
    sensor_input *= 10.0;  // Apply the same amplification as in your training dataset

    // Scale input to [0, 1] if necessary (adjust based on your data)
    float min_input = 0.0;    // Adjust based on training data minimum
    float max_input = 40.0;   // Adjust based on training data maximum
    float scaled_input = (sensor_input - min_input) / (max_input - min_input);
    scaled_input = constrain(scaled_input, 0.0, 1.0);

    // Set the input tensor (no quantization here)
    input->data.f[0] = scaled_input;

    // Debug input values
    Serial.print("Raw ADC Value: ");
    Serial.println(raw_adc_value);
    Serial.print("Amplified Input: ");
    Serial.println(sensor_input, 6);
    Serial.print("Scaled Input: ");
    Serial.println(scaled_input, 6);

    // Perform inference
    Serial.println("Performing inference...");
    if (interpreter->Invoke() != kTfLiteOk) {
        Serial.println("Inference failed.");
        return;
    }

    // Debug output tensor
    float* output_data = output->data.f;  // Use float array for output
    float max_score = -FLT_MAX;
    int predicted_class = -1;

    for (int i = 0; i < output->dims->data[1]; i++) {
        float score = output_data[i];  // Directly use the output values
        Serial.print("Class ");
        Serial.print(i);
        Serial.print(" Raw Output: ");
        Serial.print(output_data[i]);
        Serial.print(" Score: ");
        Serial.println(score);

        if (score > max_score) {
            max_score = score;
            predicted_class = i;
        }
    }

    // Debug predicted class
    Serial.print("Input Tensor Type: ");
    Serial.println(input->type);
    Serial.print("Input Tensor Dimensions: ");
    for (int i = 0; i < input->dims->size; i++) {
        Serial.print(input->dims->data[i]);
        if (i < input->dims->size - 1) Serial.print(" x ");
    }
    Serial.println();  


    // Turn off the on-board LED before updating
    digitalWrite(kOnBoardLED, LOW);

    // Control the on-board LED based on the predicted class
    switch (predicted_class) {
        case 0:  // Left
            digitalWrite(kOnBoardLED, HIGH); // Turn on the LED
            Serial.println("Predicted: Left");
            break;
        case 1:  // Saccade
            digitalWrite(kOnBoardLED, LOW); // Turn on the LED
            Serial.println("Predicted: Saccade");
            break;
        case 2:  // Right
            digitalWrite(kOnBoardLED, HIGH); // Turn on the LED
            Serial.println("Predicted: Right");
            break;
        case 3:  // Neutral
            digitalWrite(kOnBoardLED, LOW); // Turn on the LED
            Serial.println("Predicted: Neutral");
            break;
        default:
            Serial.println("Unknown prediction.");
            break;
    }


    // Delay to observe LED changes
    delay(50);  // LED on for 500 ms
    digitalWrite(kOnBoardLED, LOW); // Turn off the LED
    delay(50);  // LED off for 500 ms
}

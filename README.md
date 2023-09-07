# NanoBle33Sense_LowPower
A code is provided to use the Arduino nano Nano Ble 33 Sense board into the lowest power mode possible. Also, there is an addional example is provided in order to use this example with a LoRa communication module and Edge Impulse TinyML neuronal network for embedded devices.
 This repository hosts Arduino code designed to accomplish [brief description of your project]. The code includes features such as Low-Power mode for optimizing power consumption and LoRa transmission for sending classification results wirelessly. This README provides comprehensive information about the code, its purpose, hardware requirements, usage instructions, and licensing.



## Code Features

1. **Low-Power Mode**: The code examples incorporates low-power optimizations to minimize energy consumption when running on an Arduino device. Low-power mode is especially useful for battery-powered applications to extend battery life.

2. **LoRa Transmission**: It enables the transmission of classification results using LoRa technology. LoRa (Long Range) communication is ideal for long-distance wireless data transfer, making it suitable for remote monitoring and IoT applications.

3. **Sensor Acquisition and Edge AI**: The code integrates with sensors, such as the LSM9DS1 accelerometer, to collect data for classification. In this specific code, we gather accelerometer data for inference.

## Hardware Modifications

In order to run your Arduino Nano into low power mode, you need to provide an external 3.3V power supply. And ir order to be able to feed your board with an external 3.3V supply, you need to cut the following copper trace:

<img src="./pictures/board_cut.png" width=50% align="center"> 



## Flowchart

Below is a flowchart illustrating the high-level logic and components of the Arduino code:

```mermaid
graph TD;
    Start --> Initialization
    Initialization --> Setup
    Setup --> Data_Acquisition
  subgraph Main Loop
    Data_Acquisition --> PassDatatoInferenceEngine
    PassDatatoInferenceEngine --> WaitUntilNextAcquisition
    WaitUntilNextAcquisition --> Data_Acquisition
    end
  PassDatatoInferenceEngine --> Classification
  subgraph Second Thread
    Classification --> Transmission
    Transmission --> WaitUntilNextClassification
    WaitUntilNextClassification --> Classification
  end
```
This flowchart provides an overview of how the code operates, from initialization to continuous loop execution, inference, classification, and potential data transmission.

## License

This code is distributed under the [License Name] license. You can find the full terms and conditions in the LICENSE file provided in this repository.


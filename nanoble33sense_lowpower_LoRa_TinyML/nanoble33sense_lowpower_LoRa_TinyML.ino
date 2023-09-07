/*
Code added in order to support Low-Power mode
Code added in order to support LoRa Transmission of classificcation result
*/
#define ENABLE_LOW_POWER /*Disable UART printing and other features to reduce the power consumption of the device*/
#define ENABLE_LORA      /*ALLOW THE TRANSMITION of the classification Information every 600 seconds*/
/* Edge Impulse ingestion SDK
 * Copyright (c) 2022 EdgeImpulse Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
/* Includes ---------------------------------------------------------------- */
#include <ble_test_accelerometer_inferencing.h>
#include <Arduino_LSM9DS1.h> //Click here to get the library: https://www.arduino.cc/reference/en/libraries/arduino_lsm9ds1/

/* Constant defines -------------------------------------------------------- */
#define CONVERT_G_TO_MS2    9.80665f
#define MAX_ACCEPTED_RANGE  2.0f        // starting 03/2022, models are generated setting range to +-2, but this example use Arudino library which set range to +-4g. If you are using an older model, ignore this value and use 4.0f instead

/*
 ** NOTE: If you run into TFLite arena allocation issue.
 **
 ** This may be due to may dynamic memory fragmentation.
 ** Try defining "-DEI_CLASSIFIER_ALLOCATION_STATIC" in boards.local.txt (create
 ** if it doesn't exist) and copy this file to
 ** `<ARDUINO_CORE_INSTALL_PATH>/arduino/hardware/<mbed_core>/<core_version>/`.
 **
 ** See
 ** (https://support.arduino.cc/hc/en-us/articles/360012076960-Where-are-the-installed-cores-located-)
 ** to find where Arduino installs cores on your machine.
 **
 ** If the problem persists then there's not enough memory for this model and application.
 */

/* Private variables ------------------------------------------------------- */
static bool debug_nn = false; // Set this to true to see e.g. features generated from the raw signal
static uint32_t run_inference_every_ms = 1000;
static rtos::Thread inference_thread(osPriorityLow);
static float buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] = { 0 };
static float inference_buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];

/* Forward declaration */
void run_inference_background();

/*****************************************
 ***********LOW POWER********************************
 **********************************************/
#ifdef ENABLE_LOW_POWER //disables uart print to reduce power consumption
//Ultra Low Power blink example. Tested on Arduino nano 33 BLE board and nRF528x (Mbed OS) V1.1.6 core
//https://forum.arduino.cc/t/low-power-consumption-on-the-arduino-nano-33-ble-sense/1013562
//Override the default main function to remove USB CDC feature - @farome contribution
//Summarizing, here's a very low power version of the blink example, without shutting down the main CPU (up to 11µA with all LEDs off - 4mA with power LED on).
int main(void){
init();
initVariant();
//Disabling UART0 (saves around 300-500µA) - @Jul10199555 contribution
NRF_UART0->TASKS_STOPTX = 1;
NRF_UART0->TASKS_STOPRX = 1;
NRF_UART0->ENABLE = 0;

*(volatile uint32_t *)0x40002FFC = 0;
*(volatile uint32_t *)0x40002FFC;
*(volatile uint32_t *)0x40002FFC = 1; //Setting up UART registers again due to a library issue

  setup();
  for(;;){
    loop();
//If you won't be using serial communication comment next line
//    if(arduino::serialEventRun) arduino::serialEventRun();
  }
  return 0;
}

void setup_low_power(void){
  //pinMode(pin, OUTPUT) is already set for these 3 pins on variants.cpp
  digitalWrite(LED_PWR, LOW); // @pert contribution
  //Pins are currently swapped. Lower current achieved if setting both pins to HIGH
  digitalWrite(PIN_ENABLE_SENSORS_3V3, HIGH); //PIN_ENABLE_SENSORS_3V3 - @pert contribution
  digitalWrite(PIN_ENABLE_I2C_PULLUP, HIGH); //PIN_ENABLE_I2C_PULLUP - @pert contribution
  }
 
#endif
/******************************************************************
******************************************************
***************************************************
*************************************************
*****************************************
 **API USER EXTERNALS SETUP ADDED TO THE CODE TO ENABLE LORA *****
 *****************************************************************/
#ifdef ENABLE_LORA
/****************************************************/
#include <LoRa-E5.h>             /*main LoRa lib*/
/****************************************************/
/************************LORA SET UP*******************************************************************/
/*LoRa radio Init Parameters. Info:  https://www.thethingsnetwork.org/docs/lorawan/architecture/ */
#define LoRa_APPKEY              "2B7E151628AED2A609CF4F3CABF71588" ///*Custom key for this App*/
#define LoRa_FREQ_standard       EU868   /*International frequency band. see*/
#define LoRa_DEVICE_CLASS        CLASS_A /*CLASS_A for power restriction/low power nodes. Class C for other device applications */
#define LoRa_POWER               14      /*Node Tx (Transmition) power*/
#define LoRa_PORT                7       /*Node Tx (Transmition) port for string example*/
#define LoRa_CHANNEL             0       /*Node selected Tx channel. Default is 0, we use 2 to show only to show how to set up*/
#define LoRa_ADR_FLAG            false   /*ADR(Adaptative Dara Rate) status flag (True or False). Use False if your Node is moving*/
/*FOR SETTING DATA RATE USING THE OTHER MODE*/
#define LoRa_SF                  SF7     /*DR5=5.2kbps //data rate. see at https://www.thethingsnetwork.org/docs/lorawan/regional-parameters/    */
#define LoRa_BW                  BW125    /*DR5=5.2kbps //data rate. see at https://www.thethingsnetwork.org/docs/lorawan/regional-parameters/    */
/*Time to wait for transmiting a packet again*/
#define Tx_delay_s               600.     /*delay between transmitions expressed in seconds*/
/*Packet information size: https://lora-developers.semtech.com/documentation/tech-papers-and-guides/lora-and-lorawan */
#define PAYLOAD_TX               20   /*bytes to send. 11 bytes */
#define Tx_and_ACK_RX_timeout_ms 6000 /*Time to wait for response.6000 for SF12,4000 for SF11,3000 for SF11, 2000 for SF9/8/, 1500 for SF7. All examples consering 50 bytes payload and BW125*/     
/*Buffers used*/
unsigned char lora_tx_buffer[51] = {0};
#define lora_tx_buffer_TinyML_result &lora_tx_buffer[10]//pointer to part of payload lora frame
#define lora_tx_max_size PAYLOAD_TX//max size of message to tx
/**/                  
static int lora_elapsed_time_ms=-1000000000;/*set as minimum for proper working*/
/*Auxiliar functions*/
/*Set up the LoRa module with the desired configuration */
void LoRa_setup(void){
  lora.setDeviceMode(LWOTAA);/*LWOTAA or LWABP. We use LWOTAA in this example*/
  lora.setFrequencyBand((_physical_type_t)LoRa_FREQ_standard);
  lora.setSpreadFactor(LoRa_SF,LoRa_BW ,(_physical_type_t)LoRa_FREQ_standard); /*Set your Spread Factor and SW*/
  lora.setKey(NULL, NULL, LoRa_APPKEY); /*Only App key is seeted when using OOTA*/
  lora.setClassType((_class_type_t) LoRa_DEVICE_CLASS); /*set device class*/
  lora.setPort(LoRa_PORT);/*set the default port for transmiting data*/
  lora.setPower(LoRa_POWER); /*sets the Tx power*/
  lora.setChannel(LoRa_CHANNEL);/*selects the channel*/
  lora.setAdaptiveDataRate(LoRa_ADR_FLAG);/*Enables adaptative data rate*/
  lora.setDeviceLowPowerAutomode(true);
  /*Set baud rate before joining for a better performance*/
  lora.setDeviceBaudRate(BR_115200); /*Supported baud rates:BR_9600,BR_38400,BR_115200*/
}
#endif
/******************************************************************
******************************************************
***************************************************
*************************************************
*****************************************/
/**
* @brief      Arduino setup function
*/
void setup()
{
    //digitalWrite(LED_PWR, LOW); // @pert contribution
    #ifdef ENABLE_LOW_POWER
    setup_low_power();
    #endif
    // put your setup code here, to run once:
    #ifndef ENABLE_LOW_POWER
    Serial.begin(115200);
    // comment out the below line to cancel the wait for USB connection (needed for native USB)
    //while (!Serial);
    Serial.println("Edge Impulse Inferencing Demo");
    #endif
    int imu_state=IMU.begin();//=IMU.begin();
        #ifdef ENABLE_LOW_POWER
        delay(100);
        IMU.writeRegister(0x6b, 0x23, 0x40);//SLEEP gyro, keep FIFO off. Go to .../Arduino/libraries/Arduino_LSM9DS1/src/LSM9DS1.h and move the "int writeRegister(..." function to public declaration
        IMU.writeRegister(0x6b,0x10, 0x00);//turn off gyro.
        IMU.writeRegister(0x1e, 0x22, 0x03);//turn off mag.
        #endif
    //Turn off gyroand magnetometer:
    //IMU.writeRegister(LSM9DS1_ADDRESS_M, LSM9DS1_CTRL_REG3_M, 0x03);
    //IMU.writeRegister(LSM9DS1_ADDRESS, LSM9DS1_CTRL_REG1_G, 0x00);
    #ifndef ENABLE_LOW_POWER
      if (imu_state==0) {
        ei_printf("Failed to initialize IMU!\r\n");
          }
     else {
        ei_printf("IMU initialized\r\n");
      }
    Serial.print(EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME);
   #endif
    if (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME != 3) {
        #ifdef ENABLE_LOW_POWER      
        ei_printf("ERR: EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME should be equal to 3 (the 3 sensor axes)\n");
        #endif
        return;
    }
    #ifdef ENABLE_LORA
     //Serial.print("\r\nIniting and setting Lora Module \r\n");
     lora.init();/* call lora.init(Arduino_Tx_PIN,Arduino_Rx_PIN) to use software serial. Example: lora.init(D2,D3) */
     LoRa_setup();
     #ifndef ENABLE_LOW_POWER
     Serial.print("\r\nSetting done.Attempting to join LoRa network in OTA mode \r\n");
     #endif
     /*Attempts to join*/
     while(lora.setOTAAJoin(JOIN, 10000)==0);//will attempt to join network until the ends of time. https://www.thethingsnetwork.org/docs/lorawan/message-types/
     #ifndef ENABLE_LOW_POWER/*IF it is outside loop, join was succesfull*/
     Serial.print("\r\nJoin to LoRa network in OTA mode SUCCESFULL \r\n");
     #endif
    #endif
    inference_thread.start(mbed::callback(&run_inference_background));
}

/**
 * @brief Return the sign of the number
 * 
 * @param number 
 * @return int 1 if positive (or 0) -1 if negative
 */
float ei_get_sign(float number) {
    return (number >= 0.0) ? 1.0 : -1.0;
}

/**
 * @brief      Run inferencing in the background.
 */
void run_inference_background()
{
    // wait until we have a full buffer
    delay((EI_CLASSIFIER_INTERVAL_MS * EI_CLASSIFIER_RAW_SAMPLE_COUNT) + 100);
    // This is a structure that smoothens the output result
    // With the default settings 70% of readings should be the same before classifying.
    ei_classifier_smooth_t smooth;
    ei_classifier_smooth_init(&smooth, 2 /* no. of readings */, 1 /* min. readings the same */, 0.3 /* min. confidence */, 0.6 /* max anomaly */);
    while (1) {
        // copy the buffer
        memcpy(inference_buffer, buffer, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE * sizeof(float));
        // Turn the raw buffer in a signal which we can the classify
        signal_t signal;
        int err = numpy::signal_from_buffer(inference_buffer, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
        if (err != 0) {
            #ifndef ENABLE_LOW_POWER
            ei_printf("Failed to create signal from buffer (%d)\n", err);
            #endif
            return;
        }

        // Run the classifier
        ei_impulse_result_t result = { 0 };
        err = run_classifier(&signal, &result, debug_nn);
        if (err != EI_IMPULSE_OK) {
            #ifndef ENABLE_LOW_POWER
            ei_printf("ERR: Failed to run classifier (%d)\n", err);
            #endif
            return;
        }
        // get the prediction
         const char *prediction = ei_classifier_smooth_update(&smooth, &result);
        #ifndef ENABLE_LOW_POWER// print the predictions
        ei_printf("Predictions ");
        ei_printf("(DSP: %d ms., Classification: %d ms., Anomaly: %d ms.)",
            result.timing.dsp, result.timing.classification, result.timing.anomaly);
        ei_printf(": ");
        // ei_classifier_smooth_update yields the predicted label
        ei_printf("%s", prediction2);
        // print the cumulative results
        ei_printf(" [ ");
        for (size_t ix = 0; ix < smooth.count_size; ix++) {
            ei_printf("%u", smooth.count[ix]);
            if (ix != smooth.count_size + 1) {ei_printf(", ");}
                                        else {ei_printf(" "); }
        }
        ei_printf("]\n");
        #endif
        #ifdef ENABLE_LORA
        //if ()
          if ((millis()-lora_elapsed_time_ms)>(Tx_delay_s*1000)){
            lora_elapsed_time_ms=millis();//update current time
            /*Protection to avoid buffers overflow*/
            if((sizeof(lora_tx_buffer),(&lora_tx_buffer[0]-lora_tx_buffer_TinyML_result+sizeof(lora_tx_buffer))-lora_tx_max_size)<0){
              Serial.print("\r\nERROR: LoRa BUFFER POSSIBLE OVERFLOW Transmission no possible. Please increase buffer size");
            }
            else{
             if (strlen(prediction)<lora_tx_max_size){memcpy(lora_tx_buffer_TinyML_result,prediction,strlen(prediction));} 
             else                                    {memcpy(lora_tx_buffer_TinyML_result,prediction,lora_tx_max_size);}
             Serial.print("\r\nPerforming LoRa Transmission");
             lora.transferPacketWithConfirmed(lora_tx_buffer,sizeof(lora_tx_buffer),Tx_and_ACK_RX_timeout_ms); 
             }
           }
        #endif
        delay(run_inference_every_ms);
    }
    ei_classifier_smooth_free(&smooth);
}

/**
* @brief      Get data and run inferencing
*
* @param[in]  debug  Get debug info if true
*/
void loop()
{
    while (1) {
        // Determine the next tick (and then sleep later)
        uint64_t next_tick = micros() + (EI_CLASSIFIER_INTERVAL_MS * 1000);
        // roll the buffer -3 points so we can overwrite the last one
        numpy::roll(buffer, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, -3);
        // read to the end of the buffer
        IMU.readAcceleration(
            buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 3],//x
            buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 2],//y
            buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 1]//z
        );
        //verify the max value
        for (int i = 0; i < 3; i++) {
            if (fabs(buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 3 + i]) > MAX_ACCEPTED_RANGE) {
                buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 3 + i] = ei_get_sign(buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 3 + i]) * MAX_ACCEPTED_RANGE;
            }
        }
        buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 3] *= CONVERT_G_TO_MS2;
        buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 2] *= CONVERT_G_TO_MS2;
        buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 1] *= CONVERT_G_TO_MS2;
        // and wait for next tick
        uint64_t time_to_wait = next_tick - micros();
        delay((int)floor((float)time_to_wait / 1000.0f));
        delayMicroseconds(time_to_wait % 1000);
    }
}

//#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_ACCELEROMETER
//#error "Invalid model for current sensor"
//#endif

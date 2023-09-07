//Ultra Low Power blink example. Tested on Arduino nano 33 BLE board and nRF528x (Mbed OS) V1.1.6 core
//https://forum.arduino.cc/t/low-power-consumption-on-the-arduino-nano-33-ble-sense/1013562
//Override the default main function to remove USB CDC feature - @farome contribution
//Summarizing, here's a very low power version of the blink example, without shutting down the main CPU (up to 11µA with all LEDs off - 4mA with power LED on).

//lower value got with delay: 315 uA for  sleep in arduino nano 33 BLE sense board 
//lower value got with shutdown: 5.2 uA for shutdown in arduino nano 33 BLE sense board 
//https://forum.arduino.cc/t/nano-33-ble-sense-system-on-off-minimum-power-consumption/677478
//Note: If you add the shutdown function, you get 5.2uA as power consumption . This means 
//that there is an issue with the low power mode of the module (the delay function), and not with the hardware or sensrs

#include <Scheduler.h>

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
  }
  return 0;
}

void setup(){
//pinMode(pin, OUTPUT) is already set for these 3 pins on variants.cpp
  digitalWrite(LED_PWR, LOW); // @pert contribution
//Pins are currently swapped. Lower current achieved if setting both pins to HIGH
  digitalWrite(PIN_ENABLE_SENSORS_3V3, HIGH); //PIN_ENABLE_I2C_PULLUP - @pert contribution
  digitalWrite(PIN_ENABLE_I2C_PULLUP,  HIGH); //PIN_ENABLE_SENSORS_3V3 - @pert contribution
  //NRF_POWER->DCDCEN=1;//ENABLE BUCK CONVERTER, HELPS A LOT
   Scheduler.startLoop(loop);



}
void rtos_idle_callback(void)
{
  // Don't call any other FreeRTOS blocking API()
  // Perform background task(s) here
}
void loop(){
  digitalWrite(LED_PWR, HIGH);
  Serial1.begin(9600);
  delay(5*1000);
  Serial1.end();
  digitalWrite(LED_PWR, LOW);
  yield();
  //NRF_QDEC ->TASKS_STOP = 1;
  delay(10*1000); //332 µA USB
//  NRF_POWER->SYSTEMOFF = 1; //5.3 uA, but shuts down all the MCU.
  delay(30*1000); //332 µA USB 
}

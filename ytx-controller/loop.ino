/*

Author/s: Franco Grassano - Franco Zaccra

MIT License

Copyright (c) 2020 - Yaeltex

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

//----------------------------------------------------------------------------------------------------
// MAIN LOOP
//----------------------------------------------------------------------------------------------------

void loop() { 
  antMicrosLoop = micros();
  
  // Update status LED
  UpdateStatusLED();

  static uint32_t antMicrosTest = micros();  

  // Check for incoming Serial messages
  if(cdcEnabled) CheckSerialUSB();

  // if configuration is valid, and not in kwhat mode
  if(enableProcessing){

    // Read all inputs
    encoderHw.Read();

    analogHw.Read();        
    analogHw.SendNRPN();
    
    digitalHw.Read();       
       
    // and update feedback
    feedbackHw.Update();  
    
    // Release keys that 
    if(keyboardReleaseFlag && millis() > millisKeyboardPress){
      keyboardReleaseFlag = false;
      YTXKeyboard->releaseAll();
    }

    // // TO DO: Add feature "SAVE CONTROLLER STATE" enabled check
    if(config->board.saveControllerState && (millis()-antMillisSaveControllerState > SAVE_CONTROLLER_STATE_MS)){
      if(!receivingConfig && !fbShowInProgress && feedbackHw.fbItemsToSend == 0){   
        SetStatusLED(STATUS_BLINK, 1, statusLEDtypes::STATUS_FB_EEPROM);
        uint timeout = 50; //ms
        if(memHost->handleSaveControllerState(timeout))
          antMillisSaveControllerState = millis(); //if completed Reset millis
        // SERIALPRINTLN(millis()-antMillisSaveControllerState);
        // SERIALPRINTLN(F("Backup"));
      } 
    }
  }
  
  // If there was an interrupt because the power source changed, re-set brightness
  if(enableProcessing && powerChangeFlag && millis() - antMillisPowerChange > 50){
    if(testMode){
      SERIALPRINT(F("\nPOWER SUPPLY CONNECTED? ")); 
      SERIALPRINT(IsPowerConnected() ? F("YES\n") : F("NO\n"));
    }
    powerChangeFlag = false;
    feedbackHw.SendCommand(CMD_ALL_LEDS_OFF);
    feedbackHw.SetBankChangeFeedback(FB_BANK_CHANGED);
  }
 
  if(millis()-antMillisWD > WATCHDOG_CHECK_MS){   
    Watchdog.reset();               // Reset count for WD
    antMillisWD = millis();         // Reset millis
  } 

  if(receivingConfig){
    if(millis()-antMicrosSysex > WATCHDOG_CONFIG_CHECK_MS){
      receivingConfig = false;
      // Set watchdog time to normal and reset it
      Watchdog.disable();
      Watchdog.enable(WATCHDOG_RESET_NORMAL);
      Watchdog.reset();
    }
  }

  if(encoderHw.EncodersInMotion() && !analogHw.IsPriorityModeOn()){   // If encoders are being used and analogs aren't in priority mode
    analogHw.SetPriority(true);
    // SERIALPRINTLN("Analog priority mode on");
  }else if(!encoderHw.EncodersInMotion() && analogHw.IsPriorityModeOn()){
    analogHw.SetPriority(false);
    // SERIALPRINTLN("Analog priority mode off");
  }

    if(testMicrosLoop) 
      SERIALPRINTLN(micros()-antMicrosLoop);    
}

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

#include "headers/FeedbackClass.h"

//----------------------------------------------------------------------------------------------------
// FEEDBACK FUNCTIONS
//----------------------------------------------------------------------------------------------------
void FeedbackClass::Init(uint8_t maxBanks, uint8_t maxEncoders, uint16_t maxDigital, uint16_t maxIndependent) {
  nBanks = maxBanks;
  nEncoders = maxEncoders;
  nDigitals = maxDigital;
  nIndependent = maxIndependent;
  begun = false;
  feedbackDataToSend = false;
  updatingBankFeedback = false;
  antMicrosAck = 0;

  noInterrupts();
  fbShowInProgress = false;
  antMicrosAuxShow = 0;
  waitingForAck = false;
  auxAckReceived = false;
  auxInitAckTagged = false;
  auxCommandAckTagged = false;
  auxBurstTransmissionActive = false;
  auxBurstAckExpected = false;
  auxQueueOverflowed = false;
  auxMemoryError = false;
  auxMemoryErrorReportCount = 0;
  auxMemoryErrorLastMicros = 0;
  waitingForRainbow = false;
  auxResetPending = false;
  burstErrorOccurred = false;
  burstErrorIndex = 0;
  receivingErrorIndex = false;
  errorIndexBytesReceived = 0;
  errorIndexByte1 = 0;
  errorIndexByte2 = 0;
  interrupts();

  feedbackUpdateWriteIdx = 0;
  feedbackUpdateReadIdx = 0;
  fbItemsToSend = 0;
  fbMessagesSent = 0;
  burstEntriesProcessed = 0;
  burstEntriesBeforeFirstFrame = 0;
  burstRetryCount = 0;
  burstItemsRemaining = 0;
  burstSendIdx = 0;
  burstInProgress = false;
  burstAwaitingAck = false;
  auxInitRecoveryInProgress = false;
  auxInitWaitingForBoot = false;
  auxInitRetryCount = 0;
  auxInitResetCount = 0;
  antMicrosAuxInit = 0;
  bankControlSlotReserved = false;
  waitingMoreData = false;
  antMillisWaitMoreData = 0;
  encoderCoalesceSlots = NULL;
  digitalCoalesceSlots = NULL;

  for(int f = 0; f < FEEDBACK_UPDATE_BUFFER_SIZE; f++){
    feedbackUpdateBuffer[f].type = 0;
    feedbackUpdateBuffer[f].indexChanged = 0;
    feedbackUpdateBuffer[f].newValue = 0;
    feedbackUpdateBuffer[f].newOrientation = 0;
    feedbackUpdateBuffer[f].isShifter = 0;
    feedbackUpdateBuffer[f].updatingBank = false;
    feedbackUpdateBuffer[f].rotaryValueToColor = false;
    feedbackUpdateBuffer[f].valueToIntensity = false;
  }
  
  flagBlinkStatusLED = 0;
  blinkCountStatusLED = 0;
  statusLEDfbType = 0;
  lastStatusLEDState = LOW;
  millisStatusPrev = 0;
  ClearCoalesceIndex();

  if(!nBanks) return;    // If the number of banks is zero, initialization is complete.
    
  // First dimension is an array of pointers, each pointing to a column - https://www.eskimo.com/~scs/cclass/int/sx9b.html

  // ESTO ADELANTE DEL MALLOC DE LOS ENCODERS HACE QUE FUNCIONEN LOS ENCODERS PARA 1 BANCO CONFIGURADO
  // EL PRIMER PUNTERO QUE LLAMA A MALLOC, CAMBIA LA DIRECCIÓN A LA QUE APUNTA DESPUÉS DE LA INICIALIZACIÓN,
  // EN LA SEGUNDA VUELTA DE LOOP, SOLO CUANDO HAY 1 BANCO CONFIGURADO
  if(nDigitals){
    digFbData = (digFeedbackData**) memHost->AllocateRAM(nBanks*sizeof(digFeedbackData*));
    digitalCoalesceSlots = (uint16_t*) memHost->AllocateRAM(2 * 2 * nDigitals * sizeof(uint16_t));
  }
  if(nEncoders){
    encFbData = (encFeedbackData**) memHost->AllocateRAM(nBanks*sizeof(encFeedbackData*));
    encoderCoalesceSlots = (uint16_t*) memHost->AllocateRAM(8 * 2 * nEncoders * sizeof(uint16_t));
  }
  ClearCoalesceIndex();

  // Reset to bootloader if there isn't enough RAM
  if(FreeMemory() < nBanks*nEncoders*sizeof(encFeedbackData) + nBanks*nDigitals*sizeof(digFeedbackData) + 800){
    SERIALPRINTLN("NOT ENOUGH RAM / FEEDBACK -> REBOOTING TO BOOTLOADER...");
    
    delay(500);

    SelfReset(RESET_TO_BOOTLOADER);
  }

  for (int b = 0; b < nBanks; b++) {
    if(nEncoders){
      #if defined(DISABLE_ENCODER_BANKS)
        // Allocate only first bank and reference other banks to firs bank
        if(b==0){
          encFbData[b] = (encFeedbackData*) memHost->AllocateRAM(nEncoders*sizeof(encFeedbackData));
        }else{
          encFbData[b] = encFbData[0];
        }
      #else
        // Allocate all banks
        encFbData[b] = (encFeedbackData*) memHost->AllocateRAM(nEncoders*sizeof(encFeedbackData));
      #endif

      for (int e = 0; e < nEncoders; e++) {
        encFbData[b][e].encRingState = 0;
        encFbData[b][e].encRingStatePrev = 0;
        encFbData[b][e].vumeterValue = 0;
        encFbData[b][e].colorIndexRotary = 127;
        encFbData[b][e].colorIndexSwitch = 0;  
        encFbData[b][e].rotIntensityFactor = MAX_INTENSITY;
        encFbData[b][e].swIntensityFactor = MAX_INTENSITY;
      }
    }
    
    if(nDigitals){
      #if defined(DISABLE_DIGITAL_BANKS)
        // Allocate only first bank and reference other banks to firs bank
        if(b==0){
          digFbData[b] = (digFeedbackData*) memHost->AllocateRAM(nDigitals*sizeof(digFeedbackData));
        }else{
          digFbData[b] = digFbData[0];
        }
      #else
        // Allocate all banks
        digFbData[b] = (digFeedbackData*) memHost->AllocateRAM(nDigitals*sizeof(digFeedbackData));
      #endif

      for (uint16_t d = 0; d < nDigitals; d++) {
        digFbData[b][d].colorIndexPrev = 127;
        digFbData[b][d].digIntensityFactor = MAX_INTENSITY;
      }
    }
  } 
}

void FeedbackClass::InitFb(){
  // POWER MANAGEMENT - READ FROM POWER PIN, IF POWER SUPPLY IS PRESENT AND SET LED BRIGHTNESS ACCORDINGLY
  feedbackHw.SendCommand(CMD_ALL_LEDS_OFF);
  delay(10);
    
  if(digitalRead(externalVoltagePin)){
    if(nEncoders >= 28)  currentBrightness = BRIGHTNESS_WOP_32_ENC;
    else                 currentBrightness = BRIGHTNESS_WOP;
  }else{
    currentBrightness = BRIGHTNESS_WITH_POWER;
  }
  // Set External ISR for the power adapter detector pin
  attachInterrupt(digitalPinToInterrupt(externalVoltagePin), ChangeBrightnessISR, CHANGE);
                            
  begun = true;
  if(!InitAuxController(false)){
    auxResetPending = true;
  }
}

void FeedbackClass::SendAuxInitializationFrame(){
  byte initFrameArray[] = { nEncoders, amountOfDigitalInConfig[0], amountOfDigitalInConfig[1]};
                            // currentBrightness,
                            // config->board.rainbowOn};

  Serial.write9bit(INIT_VALUES);

  for (int i = 0; i < sizeof(initFrameArray); i++) {
    Serial.write(initFrameArray[i]);   // FRAME BODY
  }

  Serial.write9bit(END_OF_FRAME_BYTE);
}

void FeedbackClass::PrepareAuxAckWait(){
  noInterrupts();
  waitingForAck = true;
  auxAckReceived = false;
  auxInitAckTagged = false;
  auxCommandAckTagged = false;
  auxResetPending = false;
  interrupts();
}

bool FeedbackClass::WaitForAuxAck(uint32_t timeoutUs, bool acceptTaggedInitAck){
  uint32_t startedAt = micros();

  while((uint32_t)(micros() - startedAt) < timeoutUs){
    noInterrupts();
    bool taggedAck = auxAckReceived;
    bool ordinaryAck = !waitingForAck && !auxResetPending;
    if(taggedAck){
      auxAckReceived = false;
      if(acceptTaggedInitAck){
        waitingForAck = false;
      }
    }
    interrupts();

    if((acceptTaggedInitAck && taggedAck) || ordinaryAck){
      return true;
    }
  }

  noInterrupts();
  waitingForAck = false;
  auxAckReceived = false;
  auxCommandAckTagged = false;
  if(acceptTaggedInitAck){
    auxInitAckTagged = false;
  }
  interrupts();
  return false;
}

bool FeedbackClass::SendAuxInitializationWithRetry(uint8_t maxRetries, uint32_t ackTimeoutUs){
  for(uint8_t attempt = 0; attempt < maxRetries; attempt++){
    PrepareAuxAckWait();
    SendAuxInitializationFrame();
    if(WaitForAuxAck(ackTimeoutUs, true)){
      return true;
    }
  }
  return false;
}

bool FeedbackClass::SendAuxCommandWithRetry(uint8_t command, int16_t data){
  for(uint8_t attempt = 0; attempt < AUX_COMMAND_MAX_RETRIES; attempt++){
    PrepareAuxAckWait();
    Serial.write9bit(command);
    if(data >= 0){
      Serial.write((uint8_t)data);
    }
    if(WaitForAuxAck(AUX_COMMAND_ACK_TIMEOUT_US, false)){
      return true;
    }
  }
  return false;
}

bool FeedbackClass::InitAuxController(bool resetHappened){
  // SEND INITIAL VALUES AND LED BRIGHTNESS TO SAMD11
  bool makeRainbowAnimation = resetHappened ? 0 : config->board.rainbowOn;

  if(!SendAuxInitializationWithRetry(AUX_BOOT_INIT_MAX_RETRIES, AUX_BOOT_INIT_ACK_TIMEOUT_US)){
    return false;
  }
  if(auxMemoryError){
    return true;
  }

  if(makeRainbowAnimation){
    if(!SendAuxCommandWithRetry(CHANGE_BRIGHTNESS, currentBrightness)){
      return false;
    }

    bool rainbowComplete = false;
    for(uint8_t attempt = 0; attempt < AUX_RAINBOW_MAX_RETRIES && !rainbowComplete; attempt++){
      waitingForRainbow = true;
      Serial.write9bit(CMD_RAINBOW_START);
      uint32_t startedAt = micros();
      while(waitingForRainbow && (uint32_t)(micros() - startedAt) < AUX_RAINBOW_TIMEOUT_US);
      rainbowComplete = !waitingForRainbow;
    }

    if(!SendAuxCommandWithRetry(CHANGE_BRIGHTNESS, 255)){
      return false;
    }
  }

  noInterrupts();
  auxResetPending = false;
  interrupts();
  return true;
}

void FeedbackClass::RecoverAuxControllerReset(){
  if(!begun) return;

  fbMessagesSent = 0;
  burstEntriesProcessed = 0;
  burstEntriesBeforeFirstFrame = 0;
  burstRetryCount = 0;
  burstItemsRemaining = 0;
  burstSendIdx = feedbackUpdateReadIdx;
  burstInProgress = false;
  burstAwaitingAck = false;
  auxInitRecoveryInProgress = false;
  auxInitWaitingForBoot = false;
  auxInitRetryCount = 0;
  auxInitResetCount = 0;
  feedbackDataToSend = false;

  noInterrupts();
  fbShowInProgress = false;
  waitingForAck = false;
  auxAckReceived = false;
  auxInitAckTagged = false;
  auxCommandAckTagged = false;
  auxBurstTransmissionActive = false;
  auxBurstAckExpected = false;
  auxQueueOverflowed = false;
  auxMemoryError = false;
  auxMemoryErrorReportCount = 0;
  auxMemoryErrorLastMicros = 0;
  burstErrorOccurred = false;
  burstErrorIndex = 0;
  receivingErrorIndex = false;
  errorIndexBytesReceived = 0;
  errorIndexByte1 = 0;
  errorIndexByte2 = 0;
  interrupts();

  RebuildCoalesceIndex();
  auxInitRecoveryInProgress = true;
  waitingForAck = true;
  SendAuxInitializationFrame();
  antMicrosAuxInit = micros();
}

void FeedbackClass::Update() {

  if(!begun) return;

  if(auxInitRecoveryInProgress){
    uint32_t now = micros();
    noInterrupts();
    bool initAckReceived = auxAckReceived;
    if(initAckReceived){
      auxAckReceived = false;
      waitingForAck = false;
      receivingErrorIndex = false;
      errorIndexBytesReceived = 0;
      errorIndexByte1 = 0;
      errorIndexByte2 = 0;
      burstErrorOccurred = false;
      burstErrorIndex = 0;
    }
    interrupts();

    if(initAckReceived){
      auxInitRecoveryInProgress = false;
      auxInitWaitingForBoot = false;
      auxInitRetryCount = 0;
      auxInitResetCount = 0;
      SetBankChangeFeedback(FB_BANK_CHANGED);
      return;
    }

    if(auxInitWaitingForBoot){
      if((uint32_t)(now - antMicrosAuxInit) < AUX_RESET_BOOT_GRACE_US){
        return;
      }
      auxInitWaitingForBoot = false;
      auxInitRetryCount = 0;
      SendAuxInitializationFrame();
      antMicrosAuxInit = micros();
      return;
    }

    if((uint32_t)(now - antMicrosAuxInit) >= AUX_INIT_ACK_TIMEOUT_US){
      if(auxInitRetryCount >= AUX_INIT_MAX_RETRIES){
        if(auxInitResetCount >= AUX_INIT_MAX_RESET_ATTEMPTS){
          auxInitRecoveryInProgress = false;
          noInterrupts();
          waitingForAck = false;
          interrupts();
          SetStatusLED(STATUS_BLINK, 3, STATUS_FB_ERROR);
          return;
        }

        ResetFBMicro();
        auxInitResetCount++;
        auxInitWaitingForBoot = true;
        antMicrosAuxInit = micros();
        return;
      }

      noInterrupts();
      waitingForAck = true;
      burstErrorOccurred = false;
      burstErrorIndex = 0;
      receivingErrorIndex = false;
      errorIndexBytesReceived = 0;
      errorIndexByte1 = 0;
      errorIndexByte2 = 0;
      interrupts();
      SendAuxInitializationFrame();
      auxInitRetryCount++;
      antMicrosAuxInit = micros();
    }
    return;
  }

  if(auxResetPending || auxMemoryError) return;

  if((waitingMoreData && (millis()-antMillisWaitMoreData > MAX_WAIT_MORE_DATA_MS)) || (fbItemsToSend >= MSG_BUFFER_AUX)){
    waitingMoreData = false;
  }

  // Non-blocking ACK stage
  //
  if(burstAwaitingAck){
    uint32_t now = micros();
    noInterrupts();
    bool resetPending = auxResetPending;
    bool ackPending = waitingForAck;
    bool burstFailed = burstErrorOccurred;
    uint8_t failedAtFrame = burstErrorIndex;
    interrupts();

    if(resetPending){
      return;
    }

    bool ackTimedOut = ackPending && ((uint32_t)(now - antMicrosAck) >= 5000);
    if(ackPending && !ackTimedOut){
      return;
    }

    uint8_t framesSucceeded = 0;
    uint16_t entriesSucceeded = 0;
    bool shouldRetry = false;

    if(burstFailed){
      framesSucceeded = failedAtFrame;
      shouldRetry = true;
    }else if(ackPending){
      // Timeout
      framesSucceeded = 0;
      shouldRetry = true;
    }else{
      // ACK received
      framesSucceeded = fbMessagesSent;
      shouldRetry = false;
    }

    if(framesSucceeded > fbMessagesSent){
      framesSucceeded = fbMessagesSent;
    }

    if(burstFailed){
      if(framesSucceeded == 0){
        entriesSucceeded = burstEntriesBeforeFirstFrame;
      }else{
        entriesSucceeded = burstFrameEntryCounts[framesSucceeded - 1];
      }
    }else if(!shouldRetry){
      entriesSucceeded = burstEntriesProcessed;
    }

    if(entriesSucceeded > fbItemsToSend){
      entriesSucceeded = fbItemsToSend;
    }

    for(uint16_t i = 0; i < entriesSucceeded; i++){
      IncreaseBufferIndex(READ_INDEX);
    }

    fbMessagesSent = 0;
    burstEntriesProcessed = 0;
    burstEntriesBeforeFirstFrame = 0;
    burstInProgress = false;
    burstAwaitingAck = false;

    noInterrupts();
    waitingForAck = false;
    auxBurstTransmissionActive = false;
    auxBurstAckExpected = false;
    receivingErrorIndex = false;
    errorIndexBytesReceived = 0;
    errorIndexByte1 = 0;
    errorIndexByte2 = 0;
    burstErrorOccurred = false;
    burstErrorIndex = 0;
    interrupts();

    if(shouldRetry){
      burstRetryCount++;
      if(burstRetryCount >= 20){
        RecoverAuxControllerReset();
        return;
      }
      RebuildCoalesceIndex();
    }else{
      burstRetryCount = 0;
    }

    return;
  }

  // Start a new burst only when we're not in "wait for more data" window and show isn't busy
  //
  if(!burstInProgress){
    if(waitingMoreData || fbShowInProgress || fbItemsToSend == 0){
      return;
    }

    if(!IsWireFeedbackType(feedbackUpdateBuffer[feedbackUpdateReadIdx].type)){
      uint8_t controlEntryIndex = feedbackUpdateReadIdx;
      IncreaseBufferIndex(READ_INDEX);
      ProcessQueuedFeedbackEntry(controlEntryIndex);
      return;
    }

    burstInProgress = true;
    fbMessagesSent = 0;
    burstEntriesProcessed = 0;
    burstEntriesBeforeFirstFrame = 0;
    burstSendIdx = feedbackUpdateReadIdx;
    burstItemsRemaining = fbItemsToSend;

    noInterrupts();
    burstErrorOccurred = false;
    burstErrorIndex = 0;
    receivingErrorIndex = false;
    errorIndexBytesReceived = 0;
    errorIndexByte1 = 0;
    errorIndexByte2 = 0;
    bool showStarted = fbShowInProgress;
    if(!showStarted){
      auxCommandAckTagged = false;
      auxBurstTransmissionActive = true;
      auxBurstAckExpected = false;
    }
    interrupts();

    if(showStarted){
      burstInProgress = false;
      return;
    }

    Serial.write9bit(BURST_INIT);
  }

  // Send a bounded number of frames per loop iteration (non-blocking)
  //
  uint8_t framesSentNow = 0;
  while(burstItemsRemaining && fbMessagesSent < MSG_BUFFER_AUX &&
        framesSentNow < FB_MAX_FRAMES_PER_UPDATE && !auxResetPending){
    noInterrupts();
    bool burstFailedDuringPayload = burstErrorOccurred;
    interrupts();
    if(burstFailedDuringPayload){
      break;
    }

    uint8_t fbUpdateQueueIndex = burstSendIdx;

    if(!IsWireFeedbackType(feedbackUpdateBuffer[fbUpdateQueueIndex].type)){
      burstItemsRemaining = 0;
      break;
    }

    int8_t processResult = ProcessQueuedFeedbackEntry(fbUpdateQueueIndex);
    if(processResult < 0){
      // If the first frame was paused, the aux may never have observed the
      // burst header. Abandon only the transmission attempt and reissue
      // BURST_INIT after SHOW_END; no queue entry has been consumed.
      if(fbMessagesSent == 0){
        burstInProgress = false;
        noInterrupts();
        auxBurstTransmissionActive = false;
        interrupts();
        RebuildCoalesceIndex();
      }
      break;
    }

    burstSendIdx = NextFeedbackIndex(burstSendIdx);

    burstItemsRemaining--;
    burstEntriesProcessed++;
    UnregisterCoalesceSlot(fbUpdateQueueIndex);

    if(processResult > 0){
      burstFrameEntryCounts[fbMessagesSent] = burstEntriesProcessed;
      fbMessagesSent++;
    }else if(fbMessagesSent == 0){
      burstEntriesBeforeFirstFrame = burstEntriesProcessed;
    }
    framesSentNow++;
  }

  if(auxResetPending){
    return;
  }

  noInterrupts();
  bool burstFailedBeforeTail = burstErrorOccurred;
  interrupts();
  if(burstFailedBeforeTail){
    // The aux has already rejected this burst. Stop transmitting immediately
    // and enter the retry stage without re-arming the ACK timeout.
    burstInProgress = false;
    burstAwaitingAck = true;
    return;
  }

  if(burstItemsRemaining && fbMessagesSent < MSG_BUFFER_AUX){
    return;
  }

  // A show can begin after the last payload frame. Keep the burst open until
  // SHOW_END so the aux can receive the terminator and both count bytes.
  if(fbShowInProgress){
    return;
  }

  // Burst payload is done, now wait ACK asynchronously
  //
  antMicrosAck = micros();
  noInterrupts();
  waitingForAck = true;
  auxBurstAckExpected = true;
  interrupts();
  Serial.write9bit(BURST_END);
  Serial.write(fbMessagesSent);
  Serial.write(fbMessagesSent);
  burstInProgress = false;
  burstAwaitingAck = true;
}

bool FeedbackClass::IsBegun(){
  return begun;
}

bool FeedbackClass::AuxRecoveryInProgress(){
  return auxInitRecoveryInProgress;
}

bool FeedbackClass::IsWireFeedbackType(uint8_t type){
  return type >= FB_ENCODER && type <= FB_DIG_VAL_TO_INT;
}

int8_t FeedbackClass::ProcessQueuedFeedbackEntry(uint8_t fbUpdateQueueIndex){
  uint8_t fbUpdateType = feedbackUpdateBuffer[fbUpdateQueueIndex].type;
  feedbackDataToSend = false;

  switch(fbUpdateType)
  {
    case FB_ENCODER:
    case FB_ENC_2CC:
    case FB_ENC_SWITCH:
    case FB_ENC_VAL_TO_COLOR:
    case FB_ENC_VAL_TO_INT:
    case FB_ENC_SHIFT:
    case FB_ENC_SW_VAL_TO_INT:
    case FB_ENC_VUMETER:
    {
      FillFrameWithEncoderData(fbUpdateQueueIndex);
      return SendDataIfReady();
    }
    break;
    case FB_DIGITAL:
    case FB_DIG_VAL_TO_INT:
    {
      FillFrameWithDigitalData(fbUpdateQueueIndex);
      return SendDataIfReady();
    }
    break;
    case FB_ANALOG:
    {
    }
    break;
    case FB_INDEPENDENT:
    {
    }
    break;
    case FB_BANK_CHANGED:
    {
      // A bank change consists of several burst of data:
      // First we update the encoders and encoder switches
      // Then the DIGITAL 1 port
      // Then, if necessary, the DIGITAL 2 port
      //
      updatingBankFeedback = true;
      bankControlSlotReserved = true;

      // Update all rotary encoders
      //
      for(uint8_t n = 0; n < nEncoders; n++)
      {
        // If it's configured as vumeter encoder, send vumeter feedback first and then value indicator
        //
        if(encoder[n].rotaryFeedback.message == rotaryMessageTypes::rotary_msg_vu_cc)
        {
          SetChangeEncoderFeedback(FB_ENC_VUMETER, n, encFbData[currentBank][n].vumeterValue,
                                   encoderHw.GetModuleOrientation(n/4), NO_SHIFTER, BANK_UPDATE);
          SetChangeEncoderFeedback(FB_ENC_2CC, n, encoderHw.GetEncoderValue(n),
                                   encoderHw.GetModuleOrientation(n/4), NO_SHIFTER, BANK_UPDATE);
        }
        else
        {
          SetChangeEncoderFeedback(FB_ENCODER, n, encoderHw.GetEncoderValue(n),
                                   encoderHw.GetModuleOrientation(n/4), NO_SHIFTER, BANK_UPDATE);
          // If it's a double CC encoder, update second CC after first
          //
          if(encoder[n].switchConfig.mode == switchModes::switch_mode_2cc)
          {
            SetChangeEncoderFeedback(FB_ENC_2CC, n, encoderHw.GetEncoderValue2(n),
                                     encoderHw.GetModuleOrientation(n/4), NO_SHIFTER, BANK_UPDATE);
          }
        }
      }

      // Update all encoder switches that aren't shifters
      //
      for(uint8_t n = 0; n < nEncoders; n++)
      {
        bool isShifter = false;
        // Is it a shifter?
        //
        if(config->banks.count > 1)
        {
          for(int bank = 0; bank < config->banks.count; bank++)
          {
            byte bankShifterIndex = config->banks.shifterId[bank];
            if(GetHardwareID(ytxIOBLOCK::Encoder, n) == bankShifterIndex)
            {
              isShifter = true;
            }
          }
        }

        if(!isShifter)
        {
          SetChangeEncoderFeedback(FB_ENC_SWITCH, n, encoderHw.GetEncoderSwitchValue(n),
                                   encoderHw.GetModuleOrientation(n/4), NO_SHIFTER, BANK_UPDATE);
        }
      }
      SetBankChangeFeedback(FB_BANK_DIGITAL1);
      bankControlSlotReserved = false;
    }
    break;
    case FB_BANK_DIGITAL1:
    {
      // Update all digitals that aren't shifters
      //
      if(amountOfDigitalInConfig[DIGITAL_PORT_2] > 0)
      {
        bankControlSlotReserved = true;
        // If there are digitals on the second port
        //
        for(uint16_t n = 0; n < amountOfDigitalInConfig[DIGITAL_PORT_1]; n++)
        {
          bool isShifter = false;
          // Is it a shifter?
          //
          if(config->banks.count > 1)
          {
            for(int bank = 0; bank < config->banks.count; bank++)
            {
              byte bankShifterIndex = config->banks.shifterId[bank];
              if(GetHardwareID(ytxIOBLOCK::Digital, n) == bankShifterIndex)
              {
                isShifter = true;
              }
            }
          }

          if(!isShifter)
          {
            SetChangeDigitalFeedback(n, digitalHw.GetDigitalValue(n), digitalHw.GetDigitalState(n), NO_SHIFTER, BANK_UPDATE);
          }
        }
        SetBankChangeFeedback(FB_BANK_DIGITAL2);
        bankControlSlotReserved = false;
      }
      else
      {
        for(uint16_t n = 0; n < nDigitals; n++)
        {
          bool isShifter = false;

          // Is it a shifter?
          //
          if(config->banks.count > 1)
          {
            for(int bank = 0; bank < config->banks.count; bank++)
            {
              byte bankShifterIndex = config->banks.shifterId[bank];
              if(GetHardwareID(ytxIOBLOCK::Digital, n) == bankShifterIndex)
              {
                isShifter = true;
              }
            }
          }

          if(!isShifter)
          {
            SetChangeDigitalFeedback(n, digitalHw.GetDigitalValue(n), digitalHw.GetDigitalState(n), NO_SHIFTER, BANK_UPDATE);
          }
        }

        // Set shifters feedback
        //
        SetShifterFeedback();
      }
      updatingBankFeedback = false;
    }
    break;
    case FB_BANK_DIGITAL2:
    {
      for(uint16_t n = amountOfDigitalInConfig[DIGITAL_PORT_1]; n < nDigitals; n++)
      {
        bool isShifter = false;
        if(config->banks.count > 1)
        {
          for(int bank = 0; bank < config->banks.count; bank++)
          {
            byte bankShifterIndex = config->banks.shifterId[bank];
            if(GetHardwareID(ytxIOBLOCK::Digital, n) == bankShifterIndex)
            {
              isShifter = true;
            }
          }
        }

        if(!isShifter)
        {
          SetChangeDigitalFeedback(n, digitalHw.GetDigitalValue(n), digitalHw.GetDigitalState(n), NO_SHIFTER, BANK_UPDATE);
        }
      }

      SetShifterFeedback();
      updatingBankFeedback = false;
    }
    break;
    default:
      break;
  }
  return 0;
}

void FeedbackClass::SetShifterFeedback(){
  if(config->banks.count > 1){    // If there is more than one bank
    for(int bank = 0; bank < config->banks.count; bank++){
      byte bankShifterIndex = config->banks.shifterId[bank];

      if(currentBank == bank){
        if(bankShifterIndex < config->inputs.encoderCount){              
          SetChangeEncoderFeedback(FB_ENC_SWITCH, bankShifterIndex, true, encoderHw.GetModuleOrientation(bankShifterIndex/4), IS_SHIFTER, NO_BANK_UPDATE);  // HARDCODE: N° of encoders in module     
        }else{
          // DIGITAL
          SetChangeDigitalFeedback(bankShifterIndex - (config->inputs.encoderCount), true, true, IS_SHIFTER, NO_BANK_UPDATE);
        }
      }else{
        if(bankShifterIndex < config->inputs.encoderCount){
//              SERIALPRINTLN(F("FB SWITCH BANK OFF"));
          SetChangeEncoderFeedback(FB_ENC_SWITCH, bankShifterIndex, false, encoderHw.GetModuleOrientation(bankShifterIndex/4), IS_SHIFTER, NO_BANK_UPDATE);  // HARDCODE: N° of encoders in module     
        }else{
          // DIGITAL
          SetChangeDigitalFeedback(bankShifterIndex - (config->inputs.encoderCount), false, false, IS_SHIFTER, NO_BANK_UPDATE);
        }
      }
    }
  }
}

void FeedbackClass::FillFrameWithEncoderData(byte updateIndex){
  // FIX FOR SHIFT ROTARY ACTION
  uint8_t colorR = 0, colorG = 0, colorB = 0;
  uint8_t colorIndex = 0;
  uint8_t ringStateIndex= false;
  bool encoderSwitchChanged = false;
  bool isRotaryShifted = false;
  bool isFb2cc = false;
  uint16_t minValue = 0, maxValue = 0;
  uint8_t msgType = 0;
  bool is14bits = false;
  bool onCenterValue = false;
  bool useSpotBlendFrame = false;
  uint8_t spotBlendMeta = 0;
  
  uint8_t indexChanged = feedbackUpdateBuffer[updateIndex].indexChanged;
  uint8_t newOrientation = feedbackUpdateBuffer[updateIndex].newOrientation;
  uint16_t newValue = feedbackUpdateBuffer[updateIndex].newValue;
  uint8_t fbUpdateType = feedbackUpdateBuffer[updateIndex].type;
  bool isShifter = feedbackUpdateBuffer[updateIndex].isShifter;
  bool bankUpdate = feedbackUpdateBuffer[updateIndex].updatingBank;
  bool rotaryValueToColor = feedbackUpdateBuffer[updateIndex].rotaryValueToColor;
  bool valueToIntensity = feedbackUpdateBuffer[updateIndex].valueToIntensity;

  // Get state for alternate switch functions
  isRotaryShifted = encoderHw.IsShiftActionOn(indexChanged);
  isFb2cc = (fbUpdateType == FB_ENC_2CC);
  
  // Get config info for this encoder
  if(fbUpdateType == FB_ENC_SWITCH || isRotaryShifted || (isFb2cc && encoder[indexChanged].rotaryFeedback.message != rotaryMessageTypes::rotary_msg_vu_cc)){ // If encoder is shifted
    minValue = encoder[indexChanged].switchConfig.parameter[switch_minValue_MSB]<<7 | encoder[indexChanged].switchConfig.parameter[switch_minValue_LSB];
    maxValue = encoder[indexChanged].switchConfig.parameter[switch_maxValue_MSB]<<7 | encoder[indexChanged].switchConfig.parameter[switch_maxValue_LSB];
    msgType = encoder[indexChanged].switchConfig.message;
  }else{
    minValue = encoder[indexChanged].rotaryConfig.parameter[rotary_minMSB]<<7 | encoder[indexChanged].rotaryConfig.parameter[rotary_minLSB];
    maxValue = encoder[indexChanged].rotaryConfig.parameter[rotary_maxMSB]<<7 | encoder[indexChanged].rotaryConfig.parameter[rotary_maxLSB];
    msgType = encoder[indexChanged].rotaryConfig.message;
  }
  
  // IF NOT 14 BITS, USE LOWER PART FOR MIN AND MAX
  if( msgType == rotary_msg_nrpn || msgType == rotary_msg_rpn || msgType == rotary_msg_pb || 
      msgType == switch_msg_nrpn || msgType == switch_msg_rpn || msgType == switch_msg_pb){
    is14bits = true;      
  }else if(fbUpdateType == FB_ENCODER && msgType == rotary_msg_key){
    minValue = 0;
    maxValue = S_SPOT_SIZE;
  }else{
    minValue = minValue & 0x7F;
    maxValue = maxValue & 0x7F;
  }

  bool invert = false;
  uint16_t lowerValue = minValue;
  uint16_t higherValue = maxValue;
  if(minValue > maxValue){
    invert = true;
    lowerValue = maxValue;
    higherValue = minValue;
  }

  uint8_t rotaryMode = encoder[indexChanged].rotaryFeedback.mode;
  
  if((fbUpdateType == FB_ENC_VUMETER && encoder[indexChanged].rotaryFeedback.message == rotaryMessageTypes::rotary_msg_vu_cc) ||
     (fbUpdateType == FB_ENCODER && msgType == rotaryMessageTypes::rotary_msg_key)){
    rotaryMode = encoderRotaryFeedbackMode::fb_spot;
  }

  if(fbUpdateType == FB_ENCODER){
    if(!rotaryValueToColor && !valueToIntensity){
      switch(rotaryMode){
        case encoderRotaryFeedbackMode::fb_spot: {
          // Spot mode uses 13 ring LEDs: map min/max exactly to first/last LED.
          const uint8_t ringLedCount = S_SPOT_SIZE - 1;
          const uint8_t lastRingSlot = ringLedCount - 1;

          uint16_t clampedValue = newValue;
          if(clampedValue < lowerValue) clampedValue = lowerValue;
          if(clampedValue > higherValue) clampedValue = higherValue;

          uint8_t primarySlot = 0;
          uint8_t secondarySlot = 0;
          uint8_t secondaryWeightQ8 = 0;

          if(higherValue > lowerValue){
            uint32_t posQ8 = ((uint32_t)(clampedValue - lowerValue) * lastRingSlot * 256UL) / (higherValue - lowerValue);
            primarySlot = (uint8_t)(posQ8 >> 8);
            if(primarySlot > lastRingSlot) primarySlot = lastRingSlot;
            secondaryWeightQ8 = (uint8_t)(posQ8 & 0xFF);
          }

          secondarySlot = (primarySlot < lastRingSlot) ? (primarySlot + 1) : primarySlot;
          if(primarySlot == secondarySlot){
            secondaryWeightQ8 = 0;
          }

          if(invert){
            primarySlot = lastRingSlot - primarySlot;
            secondarySlot = lastRingSlot - secondarySlot;
          }

          uint8_t primaryStateIndex = primarySlot + 1;
          uint8_t secondaryStateIndex = secondarySlot + 1;

          uint16_t primaryMask = pgm_read_word(&simpleSpot[newOrientation][primaryStateIndex]);
          uint16_t secondaryMask = pgm_read_word(&simpleSpot[newOrientation][secondaryStateIndex]);

          encFbData[currentBank][indexChanged].encRingState &= newOrientation ? ENCODER_SWITCH_V_ON : ENCODER_SWITCH_H_ON;
          encFbData[currentBank][indexChanged].encRingState |= primaryMask;

          // Encode blend metadata in Orientation byte when between adjacent LEDs.
          if(secondaryWeightQ8 && (primaryStateIndex != secondaryStateIndex)){
            uint8_t secondaryWeight6 = (uint8_t)(((uint16_t)secondaryWeightQ8 * 63U + 127U) / 255U);
            if(secondaryWeight6){
              uint8_t primaryBit = 0;
              uint8_t secondaryBit = 0;
              for(uint8_t bit = 0; bit < 16; bit++){
                if(primaryMask & ((uint16_t)1 << bit)){
                  primaryBit = bit;
                  break;
                }
              }
              for(uint8_t bit = 0; bit < 16; bit++){
                if(secondaryMask & ((uint16_t)1 << bit)){
                  secondaryBit = bit;
                  break;
                }
              }

              bool secondaryIsLowerBit = (secondaryBit < primaryBit);
              spotBlendMeta = (newOrientation & 0x01) |
                              ((secondaryIsLowerBit ? 1 : 0) << 1) |
                              ((secondaryWeight6 & 0x3F) << 2);

              encFbData[currentBank][indexChanged].encRingState |= secondaryMask;
              useSpotBlendFrame = true;
            }
          }
        }
        break;
        case encoderRotaryFeedbackMode::fb_fill: {
          float fbStep = abs(maxValue-minValue);
          fbStep =  fbStep/FILL_SIZE;
          if(fbStep){
            for(int step = 0; step < FILL_SIZE-1; step++){
              if((newValue >= lowerValue + step*fbStep) && (newValue <= lowerValue + (step+1)*fbStep)){
                ringStateIndex = invert ? (FILL_SIZE-1 - step) : step;
              }else if(newValue > lowerValue + (step+1)*fbStep){
                ringStateIndex = invert ? 0 : FILL_SIZE-1;
              }
            }
          }else{
            if(!invert)
              ringStateIndex = mapl(newValue, lowerValue, higherValue, 0, FILL_SIZE-1);
            else
              ringStateIndex = mapl(newValue, lowerValue, higherValue, FILL_SIZE-1, 0);
          }

          encFbData[currentBank][indexChanged].encRingState &= newOrientation ? ENCODER_SWITCH_V_ON : ENCODER_SWITCH_H_ON;
          encFbData[currentBank][indexChanged].encRingState |= pgm_read_word(&fill[newOrientation][ringStateIndex]);
        }
        break;
        case encoderRotaryFeedbackMode::fb_pivot: {
          float fbStep = abs(maxValue-minValue);
          fbStep =  fbStep/PIVOT_SIZE;
          if(fbStep){
            for(int step = 0; step < PIVOT_SIZE-1; step++){
              if((newValue >= lowerValue + step*fbStep) && (newValue <= lowerValue + (step+1)*fbStep)){
                ringStateIndex = invert ? (PIVOT_SIZE-1 - step) : step;
              }else if(newValue > lowerValue + (step+1)*fbStep){
                ringStateIndex = invert ? 0 : PIVOT_SIZE-1;
              }
            }
          }else{
            if(!invert)
              ringStateIndex = mapl(newValue, lowerValue, higherValue, 0, PIVOT_SIZE-1);
            else
              ringStateIndex = mapl(newValue, lowerValue, higherValue, PIVOT_SIZE-1, 0);
          }  
          encFbData[currentBank][indexChanged].encRingState &= newOrientation ? ENCODER_SWITCH_V_ON : ENCODER_SWITCH_H_ON;
          encFbData[currentBank][indexChanged].encRingState |= pgm_read_word(&pivot[newOrientation][ringStateIndex]);

          uint16_t centerValue = 0;
          if((minValue+maxValue)%2)   centerValue = (minValue+maxValue+1)/2;
          else                        centerValue = (minValue+maxValue)/2;
          if(newValue == centerValue) onCenterValue = true;    // Flag center value to change color
        }
        break;
        case encoderRotaryFeedbackMode::fb_mirror: {
          float fbStep = abs(maxValue-minValue);
          fbStep =  fbStep/MIRROR_SIZE;
          if(fbStep){
            for(int step = 0; step < MIRROR_SIZE-1; step++){
              if((newValue >= lowerValue + step*fbStep) && (newValue <= lowerValue + (step+1)*fbStep)){
                ringStateIndex = invert ? (MIRROR_SIZE-1 - step) : step;
              }else if(newValue > lowerValue + (step+1)*fbStep){
                ringStateIndex = invert ? 0 : MIRROR_SIZE-1;
              }
            }
          }else{
            if(!invert)
              ringStateIndex = mapl(newValue, lowerValue, higherValue, 0, MIRROR_SIZE-1);
            else
              ringStateIndex = mapl(newValue, lowerValue, higherValue, MIRROR_SIZE-1, 0);
          }
          encFbData[currentBank][indexChanged].encRingState &= newOrientation ? ENCODER_SWITCH_V_ON : ENCODER_SWITCH_H_ON;
          encFbData[currentBank][indexChanged].encRingState |= pgm_read_word(&spread[newOrientation][ringStateIndex]);
        }
        break;
        default: break;
      }
    }
    // FEATURE NEXT STATE SHOW ON EACH ENCODER CHANGE
    // if(encFbData[currentBank][indexChanged].encRingStatePrev == encFbData[currentBank][indexChanged].encRingState){
    //   encFbData[currentBank][indexChanged].nextStateOn = true;
    //   encFbData[currentBank][indexChanged].millisStateUpdate = millis();
    // }

    // If encoder isn't shifted, use rotary feedback data to get color, otherwise use switch feedback data
    if(!isRotaryShifted){ 
      if(onCenterValue){
        colorR = pgm_read_byte(&gamma8[255-encoder[indexChanged].rotaryFeedback.color[R_INDEX]]);
        colorG = pgm_read_byte(&gamma8[255-encoder[indexChanged].rotaryFeedback.color[G_INDEX]]);
        colorB = pgm_read_byte(&gamma8[255-encoder[indexChanged].rotaryFeedback.color[B_INDEX]]);
      }else{
        // encoder[indexChanged].rotaryFeedback.rotaryValueToColor = true;
        if(encoder[indexChanged].rotaryFeedback.rotaryValueToColor){
          // SERIALPRINTLN("FB 1");
          if(rotaryValueToColor){
            if(newValue <= 127)       // Safe guard
              encFbData[currentBank][indexChanged].colorIndexRotary = newValue;
          }
          
          colorIndex = encFbData[currentBank][indexChanged].colorIndexRotary;

          colorR = pgm_read_byte(&gamma8[pgm_read_byte(&colorRangeTable[colorIndex][R_INDEX])]);
          colorG = pgm_read_byte(&gamma8[pgm_read_byte(&colorRangeTable[colorIndex][G_INDEX])]);
          colorB = pgm_read_byte(&gamma8[pgm_read_byte(&colorRangeTable[colorIndex][B_INDEX])]);
        }else{
          // Check if raw color mode (set by SetEncoderRingLedColorDirect, skip gamma)
          //
          if(encFbData[currentBank][indexChanged].colorIndexRotary == 0xFF){
            colorR = encoder[indexChanged].rotaryFeedback.color[R_INDEX];
            colorG = encoder[indexChanged].rotaryFeedback.color[G_INDEX];
            colorB = encoder[indexChanged].rotaryFeedback.color[B_INDEX];
          }
          else{
            colorR = pgm_read_byte(&gamma8[encoder[indexChanged].rotaryFeedback.color[R_INDEX]]);
            colorG = pgm_read_byte(&gamma8[encoder[indexChanged].rotaryFeedback.color[G_INDEX]]);
            colorB = pgm_read_byte(&gamma8[encoder[indexChanged].rotaryFeedback.color[B_INDEX]]);
          }
        }
      }
    }else{
      if(onCenterValue){
        colorR = pgm_read_byte(&gamma8[255-encoder[indexChanged].switchFeedback.color[R_INDEX]]);
        colorG = pgm_read_byte(&gamma8[255-encoder[indexChanged].switchFeedback.color[G_INDEX]]);
        colorB = pgm_read_byte(&gamma8[255-encoder[indexChanged].switchFeedback.color[B_INDEX]]);
      }else{
        colorR = pgm_read_byte(&gamma8[encoder[indexChanged].switchFeedback.color[R_INDEX]]);
        colorG = pgm_read_byte(&gamma8[encoder[indexChanged].switchFeedback.color[G_INDEX]]);
        colorB = pgm_read_byte(&gamma8[encoder[indexChanged].switchFeedback.color[B_INDEX]]);
      }
    }
      
  }else if (fbUpdateType == FB_ENC_2CC){  // Feedback for double CC and for vumeter indicator
    uint16_t fbStep = abs(maxValue-minValue);
    fbStep =  fbStep/S_SPOT_SIZE;
    for(int step = 0; step < S_SPOT_SIZE-1; step++){
      if((newValue >= lowerValue + step*fbStep) && (newValue <= lowerValue + (step+1)*fbStep)){
        ringStateIndex = invert ? (FILL_SIZE-1 - step) : step;
      }else if(newValue > lowerValue + (step+1)*fbStep){
        ringStateIndex = invert ? 0 : FILL_SIZE-1;
      }
    }           
    encFbData[currentBank][indexChanged].encRingState &= newOrientation ? ENCODER_SWITCH_V_ON : ENCODER_SWITCH_H_ON;
    if(invert)  encFbData[currentBank][indexChanged].encRingState |= pgm_read_word(&simpleSpotInv[newOrientation][ringStateIndex]);
    else        encFbData[currentBank][indexChanged].encRingState |= pgm_read_word(&simpleSpot[newOrientation][ringStateIndex]);
    
    if(encoder[indexChanged].rotaryFeedback.message == rotaryMessageTypes::rotary_msg_vu_cc){
      colorR = pgm_read_byte(&gamma8[0xF0]);  // whte for vumeter overlay indicator
      colorG = pgm_read_byte(&gamma8[0xF0]);
      colorB = pgm_read_byte(&gamma8[0xF0]);
    }else if(newValue == feedbackUpdateBuffer[(updateIndex != 0) ? (updateIndex-1) : (FEEDBACK_UPDATE_BUFFER_SIZE-1)].newValue){    // Ring buffer, catch the event where the two 2cc messages are first and last
      colorR = pgm_read_byte(&gamma8[255-encoder[indexChanged].rotaryFeedback.color[B_INDEX]]);
      colorG = pgm_read_byte(&gamma8[255-encoder[indexChanged].rotaryFeedback.color[R_INDEX]]);
      colorB = pgm_read_byte(&gamma8[255-encoder[indexChanged].rotaryFeedback.color[G_INDEX]]);
    }else{
      colorR = pgm_read_byte(&gamma8[255-encoder[indexChanged].rotaryFeedback.color[R_INDEX]]);
      colorG = pgm_read_byte(&gamma8[255-encoder[indexChanged].rotaryFeedback.color[G_INDEX]]);
      colorB = pgm_read_byte(&gamma8[255-encoder[indexChanged].rotaryFeedback.color[B_INDEX]]);
    }
    
  }else if (fbUpdateType == FB_ENC_VUMETER){  // Feedback type for vumeter visualization on encoder ring
    ringStateIndex = mapl(newValue, 
                          minValue,           // might need to be hardcoded 0 
                          maxValue,           // might need to be hardcoded 127
                          0, 
                          FILL_SIZE - 1);
                                                    
    encFbData[currentBank][indexChanged].encRingState &= newOrientation ? ENCODER_SWITCH_V_ON : ENCODER_SWITCH_H_ON;
    encFbData[currentBank][indexChanged].encRingState |= pgm_read_word(&fill[newOrientation][ringStateIndex]);
    
    colorR = 0;  // Color for this feedback type is on SAMD11
    colorG = 0;
    colorB = 0;
    
  }else if (fbUpdateType == FB_ENC_SWITCH) {  // Feedback for encoder switch  
    bool switchState    = (newValue == minValue) ? 0 : 1 ;   // if new value is minValue, state of LED is OFF, otherwise is ON
    bool is2cc          = (encoder[indexChanged].switchConfig.mode == switchModes::switch_mode_2cc);
    bool isFineAdj      = (encoder[indexChanged].switchConfig.mode == switchModes::switch_mode_fine);
    bool isQSTB         = (encoder[indexChanged].switchConfig.mode == switchModes::switch_mode_quick_shift) || 
                          (encoder[indexChanged].switchConfig.mode == switchModes::switch_mode_quick_shift_note);
    bool isShiftRotary  = (encoder[indexChanged].switchConfig.mode == switchModes::switch_mode_shift_rot);
    bool encoderSwitchState = encoderHw.GetEncoderSwitchState(indexChanged);
    // encoder[indexChanged].switchFeedback.lowIntensityOff = true;
    bool lowI = encoder[indexChanged].switchFeedback.lowIntensityOff;

    if((is2cc || isQSTB || isFineAdj || isShiftRotary) && !isShifter){    // Any encoder special function has a white ON state
      if(encoderSwitchState)  encFbData[currentBank][indexChanged].encRingState |=  (newOrientation ? ENCODER_SWITCH_V_ON : ENCODER_SWITCH_H_ON);
      else                    encFbData[currentBank][indexChanged].encRingState &= ~(newOrientation ? ENCODER_SWITCH_V_ON : ENCODER_SWITCH_H_ON);

      valueToIntensity = false; // If a special feature is configured, 

      // SERIALPRINT(F("ENCODER SWITCH ")); SERIALPRINT(indexChanged); SERIALPRINT(F(" - STATE: ")); SERIALPRINTLN(encoderSwitchState);
      // White colour for switch special functions
      colorR = pgm_read_byte(&gamma8[encoderSwitchState ? 220 : 0]);
      colorG = pgm_read_byte(&gamma8[encoderSwitchState ? 220 : 0]);
      colorB = pgm_read_byte(&gamma8[encoderSwitchState ? 220 : 0]);
      encoderSwitchChanged = true;
    }else if(encoder[indexChanged].switchFeedback.valueToColor && !isShifter){     // If color range is configured, get color from value
      encFbData[currentBank][indexChanged].encRingState |= (newOrientation ? ENCODER_SWITCH_V_ON : ENCODER_SWITCH_H_ON);
      colorIndex = newValue;

      if(!valueToIntensity && (newValue <= sizeof(colorRangeTable)))       // Safe guard
        colorIndex = newValue;
      else if(valueToIntensity)
        colorIndex = encFbData[currentBank][indexChanged].colorIndexSwitch;

      if(colorIndex != encFbData[currentBank][indexChanged].colorIndexSwitch || bankUpdate || valueToIntensity){
        encoderSwitchChanged = true;
        if (!valueToIntensity) encFbData[currentBank][indexChanged].colorIndexSwitch = colorIndex;
        
        colorR = pgm_read_byte(&gamma8[pgm_read_byte(&colorRangeTable[colorIndex][R_INDEX])]);
        colorG = pgm_read_byte(&gamma8[pgm_read_byte(&colorRangeTable[colorIndex][G_INDEX])]);
        colorB = pgm_read_byte(&gamma8[pgm_read_byte(&colorRangeTable[colorIndex][B_INDEX])]);
      }
    }else{   // No color range, no special feature, might be normal encoder switch or shifter button
      if(encoderSwitchState || (newValue == maxValue) || (newValue && isShifter) || valueToIntensity){      // ON
        encFbData[currentBank][indexChanged].encRingState |= (newOrientation ? ENCODER_SWITCH_V_ON : ENCODER_SWITCH_H_ON);
        // Check if raw color mode (set by SetEncoderSwitchLedColorDirect, skip gamma)
        //
        if(encFbData[currentBank][indexChanged].colorIndexSwitch == 0xFF){
          colorR = encoder[indexChanged].switchFeedback.color[R_INDEX];
          colorG = encoder[indexChanged].switchFeedback.color[G_INDEX];
          colorB = encoder[indexChanged].switchFeedback.color[B_INDEX];
        }
        else{
          colorR = pgm_read_byte(&gamma8[encoder[indexChanged].switchFeedback.color[R_INDEX]]);
          colorG = pgm_read_byte(&gamma8[encoder[indexChanged].switchFeedback.color[G_INDEX]]);
          colorB = pgm_read_byte(&gamma8[encoder[indexChanged].switchFeedback.color[B_INDEX]]);
        }
      }else if((newValue == minValue && !valueToIntensity) || (isShifter && !newValue)){    // SHIFTER, OFF
        encFbData[currentBank][indexChanged].encRingState |= (newOrientation ? ENCODER_SWITCH_V_ON : ENCODER_SWITCH_H_ON);    // If it's a bank shifter, switch LED's are on
        if(lowI || isShifter){
          if(IsPowerConnected()){    // POWER SUPPLY CONNECTED
            colorR = pgm_read_byte(&gamma8[encoder[indexChanged].switchFeedback.color[R_INDEX]*BANK_OFF_BRIGHTNESS_FACTOR_WP]);
            colorG = pgm_read_byte(&gamma8[encoder[indexChanged].switchFeedback.color[G_INDEX]*BANK_OFF_BRIGHTNESS_FACTOR_WP]);
            colorB = pgm_read_byte(&gamma8[encoder[indexChanged].switchFeedback.color[B_INDEX]*BANK_OFF_BRIGHTNESS_FACTOR_WP]);  
          }else{                     // NO POWER SUPPLY 
            colorR = pgm_read_byte(&gamma8[encoder[indexChanged].switchFeedback.color[R_INDEX]*BANK_OFF_BRIGHTNESS_FACTOR_WOP]);
            colorG = pgm_read_byte(&gamma8[encoder[indexChanged].switchFeedback.color[G_INDEX]*BANK_OFF_BRIGHTNESS_FACTOR_WOP]);
            colorB = pgm_read_byte(&gamma8[encoder[indexChanged].switchFeedback.color[B_INDEX]*BANK_OFF_BRIGHTNESS_FACTOR_WOP]);
          }
        
        }else{    
          encFbData[currentBank][indexChanged].encRingState &= ~(newOrientation ? ENCODER_SWITCH_V_ON : ENCODER_SWITCH_H_ON);
          colorR = 0;
          colorG = 0;
          colorB = 0;
        } 
      }else{
        return;  // If value != MIN and != MAX, do nothing, do not send feedback message. Skips feedbackUpdateBuffer entry.
      }
      encoderSwitchChanged = true;
    }
  }

  if (fbUpdateType == FB_ENCODER 
        || fbUpdateType == FB_ENC_2CC
        || fbUpdateType == FB_ENC_VUMETER
        || updatingBankFeedback 
        || encoderSwitchChanged) {
    encFbData[currentBank][indexChanged].encRingStatePrev = encFbData[currentBank][indexChanged].encRingState;    // not being used
    
    feedbackFrameBuffer[FeedbackFrame_Type] = (fbUpdateType == FB_ENCODER      ? (useSpotBlendFrame ? ENCODER_BLEND_FRAME : ENCODER_CHANGE_FRAME) :
                                        fbUpdateType == FB_ENC_2CC      ? ENCODER_DOUBLE_FRAME        : 
                                        fbUpdateType == FB_ENC_VUMETER  ? ENCODER_VUMETER_FRAME       : 
                                        fbUpdateType == FB_ENC_SWITCH   ? ENCODER_SWITCH_CHANGE_FRAME : 255);   
    
    // value to intensity feature
    uint8_t intensityFactor = MAX_INTENSITY;

    if(fbUpdateType == FB_ENCODER && encoder[indexChanged].rotaryFeedback.valueToIntensity){
      intensityFactor = encFbData[currentBank][indexChanged].rotIntensityFactor;
    }else if(fbUpdateType == FB_ENC_SWITCH &&  encoder[indexChanged].switchFeedback.valueToIntensity){
      intensityFactor = encFbData[currentBank][indexChanged].swIntensityFactor;
    }

    if(valueToIntensity){   // if current feedback update is Value To Intensity, store value
      intensityFactor = mapl(newValue, minValue, maxValue, MIN_INTENSITY, MAX_INTENSITY);
      if(fbUpdateType == FB_ENCODER){
        encFbData[currentBank][indexChanged].rotIntensityFactor = intensityFactor;
      }else if(fbUpdateType == FB_ENC_SWITCH && encoderHw.GetEncoderSwitchState(indexChanged)){
        encFbData[currentBank][indexChanged].swIntensityFactor = intensityFactor;
      }else if(fbUpdateType == FB_ENC_SWITCH && !encoderHw.GetEncoderSwitchState(indexChanged)){
        return;
      }
    }
    
    uint8_t brightness = encoderHw.GetEncoderBrightness(indexChanged);

    feedbackFrameBuffer[FeedbackFrame_nRing] = indexChanged;
    feedbackFrameBuffer[FeedbackFrame_Orientation] = (useSpotBlendFrame && fbUpdateType == FB_ENCODER) ? spotBlendMeta : newOrientation;
    feedbackFrameBuffer[FeedbackFrame_RingStateH] = encFbData[currentBank][indexChanged].encRingState >> 8;
    feedbackFrameBuffer[FeedbackFrame_RingStateL] = encFbData[currentBank][indexChanged].encRingState & 0xff;
    feedbackFrameBuffer[FeedbackFrame_R] = colorR*brightness/MAX_INTENSITY*intensityFactor/MAX_INTENSITY;
    feedbackFrameBuffer[FeedbackFrame_G] = colorG*brightness/MAX_INTENSITY*intensityFactor/MAX_INTENSITY;
    feedbackFrameBuffer[FeedbackFrame_B] = colorB*brightness/MAX_INTENSITY*intensityFactor/MAX_INTENSITY;
    feedbackDataToSend = true;
  }
}

void FeedbackClass::FillFrameWithDigitalData(byte updateIndex){
  uint8_t colorR = 0, colorG = 0, colorB = 0;
  bool colorIndexChanged = false;
  uint8_t colorIndex = 0; 
  uint16_t minValue = 0, maxValue = 0;
  uint8_t msgType = 0;
  bool is14bits = false;

  uint8_t indexChanged = feedbackUpdateBuffer[updateIndex].indexChanged;
  uint16_t newValue = feedbackUpdateBuffer[updateIndex].newValue;
  uint8_t fbUpdateType = feedbackUpdateBuffer[updateIndex].type;
  bool isShifter = feedbackUpdateBuffer[updateIndex].isShifter;
  bool newState = feedbackUpdateBuffer[updateIndex].newOrientation;
  bool bankUpdate = feedbackUpdateBuffer[updateIndex].updatingBank;
  bool lowI = digital[indexChanged].feedback.lowIntensityOff;
  bool valueToIntensity = feedbackUpdateBuffer[updateIndex].valueToIntensity;
  bool invert = false;
  
  minValue = digital[indexChanged].actionConfig.parameter[digital_minMSB] << 7 |
                      digital[indexChanged].actionConfig.parameter[digital_minLSB];
  maxValue = digital[indexChanged].actionConfig.parameter[digital_maxMSB] << 7 |
                      digital[indexChanged].actionConfig.parameter[digital_maxLSB];
  msgType = digital[indexChanged].actionConfig.message;

  if(IS_DIGITAL_FB_14_BIT(indexChanged)){
    is14bits = true;      
  }else{
    minValue = minValue & 0x7F;
    maxValue = maxValue & 0x7F;
    newValue = newValue & 0x7F;
  }

  if(minValue > maxValue){
    invert = true;
  }
  
  //digital config is valueToColor
  if(digital[indexChanged].feedback.valueToColor && !isShifter){
    
    //feedback action is valueToIntensity
    if(valueToIntensity){
      colorIndex = digFbData[currentBank][indexChanged].colorIndexPrev;
    }else{
      //feedback action isnt valueToIntensity
      if(newValue <= sizeof(colorRangeTable)){
        colorIndex = newValue;
        
        if(newValue>0)
          digFbData[currentBank][indexChanged].colorIndexPrev = colorIndex;
      }
    }

    colorR = pgm_read_byte(&gamma8[pgm_read_byte(&colorRangeTable[colorIndex][R_INDEX])]);
    colorG = pgm_read_byte(&gamma8[pgm_read_byte(&colorRangeTable[colorIndex][G_INDEX])]);
    colorB = pgm_read_byte(&gamma8[pgm_read_byte(&colorRangeTable[colorIndex][B_INDEX])]);
  }else{     
    // FIXED COLOR
    if(newValue == maxValue || (newValue && isShifter) || valueToIntensity){
      // Check if raw color mode (set by SetDigitalLedColorDirect, skip gamma)
      //
      if(digFbData[currentBank][indexChanged].colorIndexPrev == 0xFF){
        colorR = digital[indexChanged].feedback.color[R_INDEX];
        colorG = digital[indexChanged].feedback.color[G_INDEX];
        colorB = digital[indexChanged].feedback.color[B_INDEX];
      }
      else{
        colorR = pgm_read_byte(&gamma8[digital[indexChanged].feedback.color[R_INDEX]]);
        colorG = pgm_read_byte(&gamma8[digital[indexChanged].feedback.color[G_INDEX]]);
        colorB = pgm_read_byte(&gamma8[digital[indexChanged].feedback.color[B_INDEX]]);
      }
      if (invert) newValue = minValue;
    }else if((newValue == minValue && !valueToIntensity) || (isShifter && !newValue)){
      if(lowI || isShifter){
        if(IsPowerConnected()){
          colorR = pgm_read_byte(&gamma8[digital[indexChanged].feedback.color[R_INDEX]*BANK_OFF_BRIGHTNESS_FACTOR_WP]);
          colorG = pgm_read_byte(&gamma8[digital[indexChanged].feedback.color[G_INDEX]*BANK_OFF_BRIGHTNESS_FACTOR_WP]);
          colorB = pgm_read_byte(&gamma8[digital[indexChanged].feedback.color[B_INDEX]*BANK_OFF_BRIGHTNESS_FACTOR_WP]);  
        }else{
          colorR = pgm_read_byte(&gamma8[digital[indexChanged].feedback.color[R_INDEX]*BANK_OFF_BRIGHTNESS_FACTOR_WOP]);
          colorG = pgm_read_byte(&gamma8[digital[indexChanged].feedback.color[G_INDEX]*BANK_OFF_BRIGHTNESS_FACTOR_WOP]);
          colorB = pgm_read_byte(&gamma8[digital[indexChanged].feedback.color[B_INDEX]*BANK_OFF_BRIGHTNESS_FACTOR_WOP]);
        }    
      }else{
        colorR = 0;
        colorG = 0;
        colorB = 0;  
      } 
      if (invert) newValue = maxValue;
    }else{
      return;  // If value != MIN and != MAX, do nothing, do not send feedback message. Skips feedbackUpdateBuffer entry.
    }
  }
  
   // value to intensity feature
  uint8_t intensityFactor = MAX_INTENSITY;

  if(digital[indexChanged].feedback.valueToIntensity){
    intensityFactor = digFbData[currentBank][indexChanged].digIntensityFactor;
  }

  if(valueToIntensity){   // if current feedback update is Value To Intensity, store value
    intensityFactor = mapl(newValue, minValue, maxValue, MIN_INTENSITY, MAX_INTENSITY);
    if(digitalHw.GetDigitalState(indexChanged)){
      // SERIALPRINTLN("Dig value to intensity updated");
      digFbData[currentBank][indexChanged].digIntensityFactor = intensityFactor;
    }else if(!digitalHw.GetDigitalState(indexChanged)){
      // SERIALPRINTLN("Dig value to intensity not updated because OFF");
      return;
    }
  }

  uint8_t brightness = digitalHw.GetDigitalButtonBrightness(indexChanged);

  //feedbackFrameBuffer[msgLength] = TX_BYTES;   // INIT SERIAL FRAME WITH CONSTANT DATA
  feedbackFrameBuffer[FeedbackFrame_Type] = (indexChanged < amountOfDigitalInConfig[0]) ?  DIGITAL1_CHANGE_FRAME : 
                                                                                    DIGITAL2_CHANGE_FRAME;   
  feedbackFrameBuffer[FeedbackFrame_nDigital] = indexChanged;
  feedbackFrameBuffer[FeedbackFrame_Orientation] = 0;
  feedbackFrameBuffer[FeedbackFrame_DigitalState] = (isShifter || newValue || lowI || valueToIntensity) ? 1 : 0;
  feedbackFrameBuffer[FeedbackFrame_RingStateL] = 0;
  feedbackFrameBuffer[FeedbackFrame_R] = colorR*brightness/MAX_INTENSITY*intensityFactor/MAX_INTENSITY;
  feedbackFrameBuffer[FeedbackFrame_G] = colorG*brightness/MAX_INTENSITY*intensityFactor/MAX_INTENSITY;
  feedbackFrameBuffer[FeedbackFrame_B] = colorB*brightness/MAX_INTENSITY*intensityFactor/MAX_INTENSITY;
  feedbackDataToSend = true;
}

FeedbackClass::encFeedbackData* FeedbackClass::GetCurrentEncoderFeedbackData(uint8_t bank, uint8_t encNo){
  if(begun){
    return &encFbData[bank][encNo];
  }else{
    return NULL;
  }
}

FeedbackClass::digFeedbackData* FeedbackClass::GetCurrentDigitalFeedbackData(uint8_t bank, uint8_t digNo){
  if(begun){
    return &digFbData[bank][digNo];
  }else{
    return NULL;
  }
}

uint8_t FeedbackClass::GetVumeterValue(uint8_t encNo){
  if(encNo < nEncoders)
    return encFbData[currentBank][encNo].vumeterValue;
}

bool FeedbackClass::IsCoalescableType(uint8_t type){
  switch(type){
    case FB_ENCODER:
    case FB_ENC_VUMETER:
    case FB_ENC_VAL_TO_COLOR:
    case FB_ENC_VAL_TO_INT:
    case FB_ENC_SWITCH:
    case FB_ENC_2CC:
    case FB_ENC_SHIFT:
    case FB_ENC_SW_VAL_TO_INT:
    case FB_DIGITAL:
    case FB_DIG_VAL_TO_INT:
    case FB_BANK_CHANGED:
    case FB_BANK_DIGITAL1:
    case FB_BANK_DIGITAL2:
      return true;
    default:
      return false;
  }
}

int16_t FeedbackClass::EncoderCoalesceTypeIndex(uint8_t type){
  switch(type){
    case FB_ENCODER:            return 0;
    case FB_ENC_VUMETER:        return 1;
    case FB_ENC_VAL_TO_COLOR:   return 2;
    case FB_ENC_VAL_TO_INT:     return 3;
    case FB_ENC_SWITCH:         return 4;
    case FB_ENC_2CC:            return 5;
    case FB_ENC_SHIFT:          return 6;
    case FB_ENC_SW_VAL_TO_INT:  return 7;
    default:                    return -1;
  }
}

int16_t FeedbackClass::DigitalCoalesceTypeIndex(uint8_t type){
  switch(type){
    case FB_DIGITAL:            return 0;
    case FB_DIG_VAL_TO_INT:     return 1;
    default:                    return -1;
  }
}

int16_t FeedbackClass::BankCoalesceTypeIndex(uint8_t type){
  switch(type){
    case FB_BANK_CHANGED:       return 0;
    case FB_BANK_DIGITAL1:      return 1;
    case FB_BANK_DIGITAL2:      return 2;
    default:                    return -1;
  }
}

uint16_t* FeedbackClass::CoalesceSlotPtr(uint8_t type, uint8_t indexChanged, bool isShifter){
  int16_t typeIndex = BankCoalesceTypeIndex(type);
  if(typeIndex >= 0){
    return &bankCoalesceSlots[typeIndex];
  }

  typeIndex = DigitalCoalesceTypeIndex(type);
  if(typeIndex >= 0){
    if(!digitalCoalesceSlots || indexChanged >= nDigitals){
      return NULL;
    }
    return &digitalCoalesceSlots[((typeIndex * 2 + (isShifter ? 1 : 0)) * nDigitals) + indexChanged];
  }

  typeIndex = EncoderCoalesceTypeIndex(type);
  if(typeIndex >= 0){
    if(!encoderCoalesceSlots || indexChanged >= nEncoders){
      return NULL;
    }
    return &encoderCoalesceSlots[((typeIndex * 2 + (isShifter ? 1 : 0)) * nEncoders) + indexChanged];
  }

  return NULL;
}

void FeedbackClass::ClearCoalesceIndex(){
  for(uint8_t i = 0; i < 3; i++){
    bankCoalesceSlots[i] = 0;
  }

  if(digitalCoalesceSlots){
    uint16_t total = 2 * 2 * nDigitals;
    for(uint16_t i = 0; i < total; i++){
      digitalCoalesceSlots[i] = 0;
    }
  }

  if(encoderCoalesceSlots){
    uint16_t total = 8 * 2 * nEncoders;
    for(uint16_t i = 0; i < total; i++){
      encoderCoalesceSlots[i] = 0;
    }
  }
}

void FeedbackClass::RegisterCoalesceSlot(uint8_t slot){
  uint16_t *slotPtr = CoalesceSlotPtr(feedbackUpdateBuffer[slot].type,
                                      feedbackUpdateBuffer[slot].indexChanged,
                                      feedbackUpdateBuffer[slot].isShifter);
  if(slotPtr){
    *slotPtr = (uint16_t)slot + 1;
  }
}

void FeedbackClass::UnregisterCoalesceSlot(uint8_t slot){
  uint16_t *slotPtr = CoalesceSlotPtr(feedbackUpdateBuffer[slot].type,
                                      feedbackUpdateBuffer[slot].indexChanged,
                                      feedbackUpdateBuffer[slot].isShifter);
  if(slotPtr && *slotPtr == ((uint16_t)slot + 1)){
    *slotPtr = 0;
  }
}

void FeedbackClass::RebuildCoalesceIndex(){
  ClearCoalesceIndex();

  uint8_t idx = feedbackUpdateReadIdx;
  for(uint16_t pending = 0; pending < fbItemsToSend; pending++){
    RegisterCoalesceSlot(idx);
    idx = NextFeedbackIndex(idx);
  }
}

int16_t FeedbackClass::FindPendingUpdate(uint8_t type, uint8_t indexChanged, bool isShifter){
  uint16_t *slotPtr = CoalesceSlotPtr(type, indexChanged, isShifter);
  if(slotPtr && *slotPtr){
    return (int16_t)(*slotPtr - 1);
  }
  return -1;
}

void FeedbackClass::QueueFeedbackUpdate(uint8_t type, uint8_t indexChanged, uint16_t newValue, uint8_t newOrientation,
                                        bool isShifter, bool bankUpdate, bool rotaryValueToColor,
                                        bool valueToIntensity, bool externalFeedback){
  WaitForMIDI(externalFeedback);

  int16_t coalesceIdx = -1;
  if(IsCoalescableType(type)){
    coalesceIdx = FindPendingUpdate(type, indexChanged, isShifter);
  }

  if(coalesceIdx >= 0){
    feedbackUpdateBuffer[coalesceIdx].type               = type;
    feedbackUpdateBuffer[coalesceIdx].indexChanged       = indexChanged;
    feedbackUpdateBuffer[coalesceIdx].newValue           = newValue;
    feedbackUpdateBuffer[coalesceIdx].newOrientation     = newOrientation;
    feedbackUpdateBuffer[coalesceIdx].isShifter          = isShifter;
    feedbackUpdateBuffer[coalesceIdx].updatingBank       = bankUpdate;
    feedbackUpdateBuffer[coalesceIdx].rotaryValueToColor = rotaryValueToColor;
    feedbackUpdateBuffer[coalesceIdx].valueToIntensity   = valueToIntensity;
    return;
  }

  uint8_t writeIndex = feedbackUpdateWriteIdx;
  bool replacingUnsentEntry = false;

  // Never move the read index beneath entries already transmitted in a burst.
  // Replace the oldest unsent wire entry in place so the newest host state is
  // retained without disturbing the ACK retirement order or dropping a
  // bank-stage control entry.
  bool preservingBankControlSlot = bankControlSlotReserved &&
                                   IsWireFeedbackType(type) &&
                                   fbItemsToSend >= (FEEDBACK_UPDATE_BUFFER_SIZE - 1);
  if(fbItemsToSend >= FEEDBACK_UPDATE_BUFFER_SIZE || preservingBankControlSlot){
    int16_t replaceIndex = IsWireFeedbackType(type)
                         ? FindOldestReplaceableUnsentSlot()
                         : FindNewestReplaceableUnsentSlot();
    if(replaceIndex < 0){
      return;
    }
    writeIndex = (uint8_t)replaceIndex;

    // Cross-target replacement is lossy, but a physical-state bank repaint
    // cannot reconstruct externally supplied LED state.  Never overwrite
    // host colors with that unrelated semantic fallback.
    UnregisterCoalesceSlot(writeIndex);
    replacingUnsentEntry = true;
  }

  feedbackUpdateBuffer[writeIndex].type               = type;
  feedbackUpdateBuffer[writeIndex].indexChanged       = indexChanged;
  feedbackUpdateBuffer[writeIndex].newValue           = newValue;
  feedbackUpdateBuffer[writeIndex].newOrientation     = newOrientation;
  feedbackUpdateBuffer[writeIndex].isShifter          = isShifter;
  feedbackUpdateBuffer[writeIndex].updatingBank       = bankUpdate;
  feedbackUpdateBuffer[writeIndex].rotaryValueToColor = rotaryValueToColor;
  feedbackUpdateBuffer[writeIndex].valueToIntensity   = valueToIntensity;
  RegisterCoalesceSlot(writeIndex);

  if(!replacingUnsentEntry){
    IncreaseBufferIndex(WRITE_INDEX);
  }
}

int16_t FeedbackClass::FindOldestReplaceableUnsentSlot(){
  bool burstActive = burstInProgress || burstAwaitingAck;
  uint16_t entriesToScan = burstActive
                         ? fbItemsToSend - burstEntriesProcessed
                         : fbItemsToSend;
  uint8_t scanIndex = burstActive ? burstSendIdx : feedbackUpdateReadIdx;

  while(entriesToScan--){
    if(IsWireFeedbackType(feedbackUpdateBuffer[scanIndex].type)){
      return scanIndex;
    }
    scanIndex = NextFeedbackIndex(scanIndex);
  }

  return -1;
}

int16_t FeedbackClass::FindNewestReplaceableUnsentSlot(){
  bool burstActive = burstInProgress || burstAwaitingAck;
  uint16_t entriesToScan = burstActive
                         ? fbItemsToSend - burstEntriesProcessed
                         : fbItemsToSend;
  uint8_t scanIndex = burstActive ? burstSendIdx : feedbackUpdateReadIdx;
  int16_t newestReplaceable = -1;

  while(entriesToScan--){
    if(IsWireFeedbackType(feedbackUpdateBuffer[scanIndex].type)){
      newestReplaceable = scanIndex;
    }
    scanIndex = NextFeedbackIndex(scanIndex);
  }

  return newestReplaceable;
}

void FeedbackClass::SetChangeEncoderFeedback(uint8_t type, uint8_t encIndex, uint16_t val, uint8_t encoderOrientation, 
                                              bool isShifter, bool bankUpdate, bool encoderColorChangeMsg, bool valToIntensity, bool externalFeedback) {
  if(type == FB_ENC_VUMETER)  encFbData[currentBank][encIndex].vumeterValue = val;
  QueueFeedbackUpdate(type, encIndex, val, encoderOrientation, isShifter, bankUpdate,
                      encoderColorChangeMsg, valToIntensity, externalFeedback);
}

void FeedbackClass::SetChangeDigitalFeedback(uint16_t digitalIndex, uint16_t updateValue, bool hwState, 
                                              bool isShifter, bool bankUpdate, bool externalFeedback, bool valToIntensity){
  QueueFeedbackUpdate(FB_DIGITAL, digitalIndex, updateValue, hwState, isShifter, bankUpdate,
                      false, valToIntensity, externalFeedback);
}

void FeedbackClass::SetChangeIndependentFeedback(uint8_t type, uint16_t fbIndex, uint16_t val, bool bankUpdate, bool externalFeedback){
  QueueFeedbackUpdate(type, fbIndex, val, 0, false, bankUpdate, false, false, externalFeedback);
  // SERIALPRINTLN(fbItemsToSend);
}

void FeedbackClass::SetBankChangeFeedback(uint8_t type){
  QueueFeedbackUpdate(type, 0, 0, 0, false, false, false, false, false);
  // SERIALPRINTLN(fbItemsToSend);
}

void FeedbackClass::IncreaseBufferIndex(bool indexType){
  if(indexType == READ_INDEX){
    UnregisterCoalesceSlot(feedbackUpdateReadIdx);
    feedbackUpdateReadIdx = NextFeedbackIndex(feedbackUpdateReadIdx);
    fbItemsToSend--;
    // SERIALPRINT("R IDX: ");
    // SERIALPRINTLN(fbItemsToSend);
  }else if(indexType == WRITE_INDEX){
    feedbackUpdateWriteIdx = NextFeedbackIndex(feedbackUpdateWriteIdx);
    fbItemsToSend++;  
    // SERIALPRINT("W IDX: ");
    // SERIALPRINTLN(fbItemsToSend);
  }
  
// SERIALPRINTLN(fbItemsToSend);

}

uint8_t FeedbackClass::NextFeedbackIndex(uint8_t index){
  static_assert(FEEDBACK_UPDATE_BUFFER_SIZE <= 256,
                "feedback queue indices require at most 256 entries");
  return index == (FEEDBACK_UPDATE_BUFFER_SIZE - 1) ? 0 : index + 1;
}

void FeedbackClass::WaitForMIDI(bool externalFeedback){
  if(externalFeedback){
    antMillisWaitMoreData = millis();
    waitingMoreData = true;
  }
}

int8_t FeedbackClass::SendDataIfReady(){
  if(!feedbackDataToSend){
    return 0;
  }

  if(!SendFeedbackData()){
    return -1;
  }

  feedbackDataToSend = false;
  return 1;
}

void FeedbackClass::AddCheckSum(){ 
  // uint16_t sum = 2019 - checkSum(sendSerialBufferEnc, e_B+1);
  
  // sum &= 0x3FFF;    // 14 bit checksum
  
  // sendSerialBufferEnc[e_checkSum_MSB] = (sum >> 7) & 0x7F;
  // sendSerialBufferEnc[e_checkSum_LSB] = sum & 0x7F;
}

// #define DEBUG_FB_FRAME
bool FeedbackClass::SendFeedbackData(){
  // In pipelined mode, just send the frame without waiting for ACK
  // The ACK will be checked after BURST_END is sent
  //

  #ifdef DEBUG_FB_FRAME
    SERIALPRINT(F("FRAME WITHOUT ENCODING:\n"));
    for(int i = 0; i <= FeedbackFrame_B; i++){
      SERIALPRINT(i); SERIALPRINT(F(": "));SERIALPRINT(feedbackFrameBuffer[i]); SERIALPRINT(F("\t"));
    } 
    SERIALPRINTLN();
  #endif
  
  if(fbShowInProgress){
    return false;
  }

  uint16_t sum = 2019 + checkSum(feedbackFrameBuffer, FeedbackFrame_Size);

  Serial.write9bit(NEW_FRAME_BYTE);             // SEND FRAME HEADER

  for (int i = 0; i < FeedbackFrame_Size; i++)
  {
    Serial.write(feedbackFrameBuffer[i]);       // FRAME BODY
  }

  Serial.write(sum&0x00FF);

  Serial.write9bit(END_OF_FRAME_BYTE);          // SEND END OF FRAME BYTE
  return true;
}

void FeedbackClass::SendCommand(uint8_t cmd){
//  SERIALPRINT(F("Command sent: "));
//  SERIALPRINTLNF(cmd, HEX);
  Serial.write9bit(cmd);
}

// Directly set a digital LED to a specific RGB color
// RGB values are expected to be 7-bit (0-127), will be scaled to 8-bit
// Bypasses gamma correction for linear color control
//
void FeedbackClass::SetDigitalLedColorDirect(uint16_t digitalIndex, uint8_t r, uint8_t g, uint8_t b)
{
  if (!begun) return;
  if (digitalIndex >= nDigitals) return;

  // Scale 7-bit to 8-bit and apply inverse gamma so final result is linear
  // gamma8[x] crushes low values, so we find the index that gives us our target
  // For simplicity, just scale up more aggressively: 7-bit * 2 = 8-bit
  // Then store directly without going through gamma (set raw values in color[])
  //
  digital[digitalIndex].feedback.color[R_INDEX] = r << 1;
  digital[digitalIndex].feedback.color[G_INDEX] = g << 1;
  digital[digitalIndex].feedback.color[B_INDEX] = b << 1;

  // Disable valueToColor so our fixed color is used
  //
  digital[digitalIndex].feedback.valueToColor = 0;
  
  // Mark this digital as using raw color (skip gamma in FillFrameWithDigitalData)
  //
  digFbData[currentBank][digitalIndex].colorIndexPrev = 0xFF;

  // Queue the feedback update through the normal path
  //
  uint8_t maxVal = digital[digitalIndex].actionConfig.parameter[digital_maxLSB];
  SetChangeDigitalFeedback(digitalIndex, maxVal, true, NO_SHIFTER, NO_BANK_UPDATE, true);
}

// Directly set an encoder switch (pushbutton) LED to a specific RGB color
// RGB values are expected to be 7-bit (0-127), will be scaled to 8-bit
// Bypasses gamma correction for linear color control
//
void FeedbackClass::SetEncoderSwitchLedColorDirect(uint8_t encIndex, uint8_t r, uint8_t g, uint8_t b)
{
  if (!begun) return;
  if (encIndex >= nEncoders) return;

  // Scale 7-bit to 8-bit
  //
  encoder[encIndex].switchFeedback.color[R_INDEX] = r << 1;
  encoder[encIndex].switchFeedback.color[G_INDEX] = g << 1;
  encoder[encIndex].switchFeedback.color[B_INDEX] = b << 1;

  // Disable valueToColor so our fixed color is used
  //
  encoder[encIndex].switchFeedback.valueToColor = 0;

  // Mark as raw color mode (skip gamma) using colorIndexSwitch = 0xFF
  //
  encFbData[currentBank][encIndex].colorIndexSwitch = 0xFF;

  // Queue the feedback update
  //
  uint8_t maxVal = encoder[encIndex].switchConfig.parameter[switch_maxValue_LSB];
  SetChangeEncoderFeedback(FB_ENC_SWITCH, encIndex, maxVal, 
                           encoderHw.GetModuleOrientation(encIndex/4), 
                           NO_SHIFTER, NO_BANK_UPDATE, false, false, true);
}

// Directly set an encoder ring LED to a specific RGB color
// RGB values are expected to be 7-bit (0-127), will be scaled to 8-bit
// Bypasses gamma correction for linear color control
//
void FeedbackClass::SetEncoderRingLedColorDirect(uint8_t encIndex, uint8_t r, uint8_t g, uint8_t b)
{
  if (!begun) return;
  if (encIndex >= nEncoders) return;

  // Scale 7-bit to 8-bit
  //
  encoder[encIndex].rotaryFeedback.color[R_INDEX] = r << 1;
  encoder[encIndex].rotaryFeedback.color[G_INDEX] = g << 1;
  encoder[encIndex].rotaryFeedback.color[B_INDEX] = b << 1;

  // Disable valueToColor so our fixed color is used
  //
  encoder[encIndex].rotaryFeedback.rotaryValueToColor = 0;

  // Mark as raw color mode (skip gamma) using colorIndexRotary = 0xFF
  //
  encFbData[currentBank][encIndex].colorIndexRotary = 0xFF;

  // Queue the feedback update - use current encoder value
  //
  SetChangeEncoderFeedback(FB_ENCODER, encIndex, encoderHw.GetEncoderValue(encIndex), 
                           encoderHw.GetModuleOrientation(encIndex/4), 
                           NO_SHIFTER, NO_BANK_UPDATE, false, false, true);
}

void FeedbackClass::SendResetToBootloader(){
}

void * FeedbackClass::GetEncoderFBPtr(){
  return (void*) encFbData[currentBank];
}
      

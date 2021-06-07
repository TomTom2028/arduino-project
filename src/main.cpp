#include <Arduino.h>
#include "Wire.h"

const int I2C_ADD = 2;

const int LEFT_TILT_PIN = 7;
const int RIGHT_TILT_PIN = 6;

const int BTN1_PIN = 3;
const int BTN2_PIN = 2;


const int SHIFT_PIN_DATA = 12;
const int SHIFT_PIN_CLK = 10;
const int SHIFT_PIN_LATCH = 11;

const int RANDOM_SEED_PIN = A0;

char FINAL_CODE_NUM = '4';
const int COMO_LEN = 7;

const unsigned long DEBOUNCE_TILT_MS = 100;
const unsigned long DEBOUNCE_BTN_MS = 2;

enum Move {
    TILT_L,
    TILT_R,
    BTN_1,
    BTN_2,
    NONE

};

void sendByteToShift(byte data)
{
    // we convert the nice input data to the random wireing of the real world (bitmapping)
    byte dataCopy = 0xff;
    bitWrite(dataCopy, 0, bitRead(data, 6));
    bitWrite(dataCopy, 1, bitRead(data, 5));
    bitWrite(dataCopy, 2, bitRead(data, 0));
    bitWrite(dataCopy, 3, bitRead(data, 4));
    bitWrite(dataCopy, 4, bitRead(data, 3));
    bitWrite(dataCopy, 5, bitRead(data, 2));
    bitWrite(dataCopy, 6, bitRead(data, 7));
    bitWrite(dataCopy, 7, bitRead(data, 1));


    digitalWrite(SHIFT_PIN_LATCH, LOW);
    shiftOut(SHIFT_PIN_DATA, SHIFT_PIN_CLK, LSBFIRST, dataCopy);
    digitalWrite(SHIFT_PIN_LATCH, HIGH);
}


byte sevenSegLastByte;
byte sevenSegByte;




Move combo[COMO_LEN];

int numComboCorrect;

byte charToByte(char in)
{
    // we expect that the display is wirered with first bit A, last bit DP
    // this is not the case, so we fix this in our sendByteToShift function.

    switch (in) {
        case '0': return 0b11000000;
        case '1': return 0b11111001;
        case '2': return 0b10100100;
        case '3': return 0b10110000;
        case '4': return 0b10011001;
        case '5': return 0b10010010;
        case '6': return 0b10000010;
        case '7': return 0b11111000;
        case '8': return 0b10000000;
        case '9': return 0b10010000;
        default: return 0b11111111;
    }
}


void generateRandomCombo()
{
    for (int i = 0; i < COMO_LEN; i++)
    {
        switch (random(0, 4)) {
            case 0:
                combo[i] = TILT_L;
                break;
            case 1:
                combo[i] = TILT_R;
                break;
            case 2:
                combo[i] = BTN_1;
                break;
            case 3:
                combo[i] = BTN_2;
        }
    }
}

Move tiltStatus;
Move lastTiltStatus;


Move debouncedTiltStatus;
Move lastDebouncedTiltStatus;


// game can only start if code is received
bool gameDone = true;

bool startTiltTimer;


unsigned long lastUpdateTiltMs;

Move btnStatus;
Move lastBtnStatus;

Move debouncedBtnStatus;
Move lastDebouncedBtnStatus;



unsigned long lastUpdateBtnMs;

bool startBtnTimer;




Move lastMove;
Move currentMove;


void initGame()
{
    generateRandomCombo();
    numComboCorrect = 0;
    sevenSegLastByte = 0x7f;
    sevenSegByte = sevenSegLastByte;


    tiltStatus = NONE;
    lastTiltStatus = tiltStatus;
    startTiltTimer = false;
    debouncedTiltStatus = lastTiltStatus;
    lastDebouncedTiltStatus = debouncedTiltStatus;

    lastUpdateTiltMs = millis();
    lastUpdateBtnMs = millis();


    btnStatus = NONE;
    lastBtnStatus = btnStatus;
    debouncedBtnStatus = lastBtnStatus;
    lastDebouncedBtnStatus = debouncedBtnStatus;
    startBtnTimer = false;


    lastMove = NONE;
    currentMove = lastMove;


    gameDone = false;
}

void recieveCode(int howMany)
{
    //on i2c signal resets game
    while(Wire.available())
    {
        gameDone = Wire.read();
    }

    initGame();
}

void setup() {
    pinMode(LEFT_TILT_PIN, INPUT);
    pinMode(RIGHT_TILT_PIN, INPUT);

    pinMode(BTN1_PIN, INPUT);
    pinMode(BTN2_PIN, INPUT);


    pinMode(RANDOM_SEED_PIN, INPUT);
    randomSeed(analogRead(RANDOM_SEED_PIN));


    pinMode(SHIFT_PIN_DATA, OUTPUT);
    pinMode(SHIFT_PIN_CLK, OUTPUT);
    pinMode(SHIFT_PIN_LATCH, OUTPUT);



    sevenSegLastByte = 0xff;
    sendByteToShift(sevenSegLastByte);
    Serial.begin(9600);


    Wire.begin(I2C_ADD);
    Wire.onReceive(recieveCode);
}


void doMove(Move toDoMove)
{
    if (toDoMove == NONE)
    {
        return;
    }

    // stops game
    if (numComboCorrect >= COMO_LEN)
    {
        return;
    }

    if (toDoMove == combo[numComboCorrect])
    {
        bitClear(sevenSegByte, numComboCorrect);
        numComboCorrect++;
        Serial.println("Correct");

        // win condition
        if (numComboCorrect == COMO_LEN - 1)
        {
            sevenSegByte = charToByte(FINAL_CODE_NUM);
            sendByteToShift(sevenSegByte);
            gameDone = true;
        }
    }
    else
    {
        // We reset the first seven bits
        byte sevenSegCopy = sevenSegByte;

        sevenSegCopy = 0xff;
        bitWrite(sevenSegCopy, 7, bitRead(sevenSegByte, 7));
        sevenSegByte = sevenSegCopy;
        numComboCorrect = 0;
    }
}



void getDebouncedTiltStatus()
{
    // everytime the tilt status changes, we start a timer
    // when this timer stops, we set the tilt status
    if (!digitalRead(LEFT_TILT_PIN))
    {
        tiltStatus = TILT_L;
    }


    if (!digitalRead(RIGHT_TILT_PIN))
    {
        tiltStatus = TILT_R;
    }

    if (digitalRead(LEFT_TILT_PIN) && digitalRead(RIGHT_TILT_PIN))
    {
        tiltStatus = NONE;
    }


    if (lastTiltStatus != tiltStatus)
    {
        lastTiltStatus = tiltStatus;
        startTiltTimer = true;
        lastUpdateTiltMs = millis();
    }


    if (millis() - lastUpdateTiltMs > DEBOUNCE_TILT_MS && startTiltTimer)
    {
        startTiltTimer = false;
        debouncedTiltStatus = lastTiltStatus;
    }

}


void getDebouncedBtnStatus()
{
    // We don't want 2 buttons pressed at the same time, so we just skip
    if (digitalRead(BTN1_PIN) && digitalRead(BTN2_PIN))
    {
        return;
    }


    if (digitalRead(BTN1_PIN))
    {
        btnStatus = BTN_1;
    }
    else if (digitalRead(BTN2_PIN))
    {
        btnStatus = BTN_2;
    }
    else
    {
        btnStatus = NONE;
    }


    if (btnStatus != lastBtnStatus)
    {
        lastBtnStatus = btnStatus;
        lastUpdateBtnMs = millis();
        startBtnTimer = true;
    }

    if (millis() - lastUpdateBtnMs > DEBOUNCE_BTN_MS && startBtnTimer)
    {
        startBtnTimer = false;
        debouncedBtnStatus = lastBtnStatus;
    }
}



void loop() {

    if (gameDone)
    {
        return;
    }

    getDebouncedTiltStatus();

    getDebouncedBtnStatus();

    // In this if statement we can access the update of the debounced status
    if (lastDebouncedTiltStatus != debouncedTiltStatus)
    {
        lastDebouncedTiltStatus = debouncedTiltStatus;

        // if the dot is shown, the tilt sensor is in neutral
        if (debouncedTiltStatus == NONE)
        {
            bitClear(sevenSegByte, 7);
        }
        else
        {
            bitSet(sevenSegByte, 7);
        }

    }

    if (lastDebouncedBtnStatus != debouncedBtnStatus)
    {
        lastDebouncedBtnStatus = debouncedBtnStatus;

    }

    // we "combine" the 2 statuses to get the final input
    if (debouncedTiltStatus == NONE && debouncedBtnStatus == NONE)
    {
        currentMove = NONE;
    }
    else if (debouncedTiltStatus != NONE)
    {
        currentMove = debouncedTiltStatus;
    }
    else if (debouncedBtnStatus != NONE)
    {
        currentMove = debouncedBtnStatus;
    }


    if (lastMove != currentMove)
    {
        lastMove = currentMove;
        doMove(lastMove);
    }


    if (sevenSegLastByte != sevenSegByte)
    {
        sevenSegLastByte = sevenSegByte;
        sendByteToShift(sevenSegLastByte);
    }


}
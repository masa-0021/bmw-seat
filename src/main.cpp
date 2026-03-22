#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// If you comment this line, the DPRINT & DPRINTLN lines are
#define DEBUG

#ifdef DEBUG // Macros are usually in all capital letters.
#define DPRINT(...) Serial.print(__VA_ARGS__) // DPRINT is a macro, debug print
#define DPRINTLN(...)                                                          \
    Serial.println(__VA_ARGS__) // DPRINTLN is a macro, debug print with new line
#else
#define DPRINT(...)   // now defines a blank line
#define DPRINTLN(...) // now defines a blank line
#endif

//#define SIMULATION
#ifdef SIMULATION
#define OVERWRITE(a,b) a = b;
#else
#define OVERWRITE(...)
#endif

/* Display settings */
const uint8_t Disp_I2C_Addr = 0x27;
const uint8_t Disp_Cols = 20;
const uint8_t Disp_Rows = 4;

/* Initialize the display */
LiquidCrystal_I2C lcd = LiquidCrystal_I2C(Disp_I2C_Addr, Disp_Cols, Disp_Rows);

/* ADC connected to connector 2
 * Reference voltage divider:
 * Two 10kOhm voltage divider
 * ADC reference: 5V
 * ADC = 1023 corresponds to 5V
 * Due to symmetric voltage divider, double the value for reference voltage */
const uint8_t AnalogPinRef = A1;

float readRefVoltage() {
    int val = analogRead(AnalogPinRef);
    return( (float)(2 * val * 5)/1023.0f);
}

const unsigned int arrSize = 13;
const int TempArr[arrSize] = {60, 55, 50, 45, 40, 35, 30, 25, 20, 15, 10, 5, 0};
const int ResArr[arrSize] = {2996, 3513, 4137, 4891, 5809, 6929, 8304, 10000, 12100, 14720, 18000, 22130, 27350};
/* Getter for 10kOhm NTC
 * 27.35 kOhm -> 0degC
 * 22.13 kOhm -> 5degC
 * 18.00 kOhm -> 10degC
 * 14.72 kOhm -> 15degC
 * 12.10 kOhm -> 20degC
 * 10.00 kOhm -> 25degC
 * 8.304 kOhm -> 30degC
 * 6.929 kOhm -> 35degC
 * 5.809 kOhm -> 40degC
 * 4.891 kOhm -> 45degC
 * 4.137 kOhm -> 50degC
 * 3.513 kOhm -> 55degC
 * 2.996 kOhm -> 60degC */


int convertTemp(int resNTC){
    DPRINT("convertTemp: resNTC is: ");
    DPRINT(resNTC);
    DPRINT("\n");

    if( resNTC <= ResArr[0])
        return TempArr[0];

    if( resNTC >= ResArr[12])
        return TempArr[12];
    
    for( int i = 0; i < 12; ++i ) {
        if (resNTC == ResArr[i])
            return TempArr[i];

        if( resNTC > ResArr[i] && resNTC < ResArr[i+1]) {
            float d = ( (float) (resNTC - ResArr[i])) / ((float) (ResArr[i+1]-ResArr[i]));
            return ( TempArr[i] + (int) (d*(TempArr[i+1]-TempArr[i])) );
        }
    }

    return(TempArr[12]);
}


/* Temperature seat 1
 * 10 kOhm NTC + Voltage Divider with 10kOhm, supplied by reference voltage
 * ADC reference: 5V
 * ADC = 1023 corresponds to 5V ADC
 * RNTC = 10 kOhm x (Uref/Uadc - 1)
 * Uadc = ADC/1023 x 5V
 * RNTC = 10.000 Ohm * (Uref*1023/ADC x 5V - 1)
 */
const int analogPinSeat1 = A0;

int readNTCSeat1(float uref) {
    int val = analogRead(analogPinSeat1);
    long tmp_res = (long) (10000.0f*(uref/val * 1023.0f / 5.0f - 1.0f));
    int res;
    if (tmp_res < -32768) {
        res = -32768;
    } else if (tmp_res > 32767) {
        res = 32767;
    } else {
        res = (int) tmp_res;
    }

    DPRINT("readNTCSeat1: val & uref & resNTC: ");
    DPRINT(val);
    DPRINT(" ");
    DPRINT(uref);
    DPRINT(" ");
    DPRINT(res);
    DPRINT("\n");

    return( res );
}

/* Temperature seat 2
 * 10 kOhm NTC + Voltage Divider with 10kOhm, supplied by reference voltage
 * ADC reference: 5V
 * ADC = 1023 corresponds to 5V ADC
 * RNTC = 10 kOhm x (Uref/Uadc - 1)
 * Uadc = ADC/1023 x 5V
 * RNTC = 10.000 Ohm * (Uref/(ADC/1023 x 5V) - 1)
 */
const int analogPinSeat2 = A3;

int readNTCSeat2(float uref) {
    int val = analogRead(analogPinSeat2);
    long tmp_res = (long) (10000.0f*(uref/val * 1023.0f / 5.0f - 1.0f));
    int res;
    if (tmp_res < -32768) {
        res = -32768;
    } else if (tmp_res > 32767) {
        res = 32767;
    } else {
        res = (int) tmp_res;
    }

    DPRINT("readNTCSeat2: val & uref & resNTC: ");
    DPRINT(val);
    DPRINT(" ");
    DPRINT(uref);
    DPRINT(" ");
    DPRINT(res);
    DPRINT("\n");


    return( res );
}

/* Variables for Heating
 */ 
bool stHeat = true;
bool stCoolDown = false;
uint32_t timer = 0UL;
/* Stop heating after 1200000ms = 1200s = 20min
 * Start heating again after 10000ms = 10s */ 
const uint32_t timer_heat = 1200000UL;
const uint32_t timer_cool = 10000UL;
const int relaisPin = 7;
const int maxTemp = 55;

ISR(TIMER0_COMPA_vect){    //This is the interrupt request
  timer++;
}

const int SerialSpeed = 9600;

void setup() {
    Serial.begin(SerialSpeed);

    /* LCD start backlight */
    lcd.init();
    lcd.backlight();
    delay(250);
    lcd.noBacklight();
    delay(1000);
    lcd.backlight();
    delay(1000);

    /* Start timer, interval 1ms */
    /* CTC - Clear Timer on CompareCTC - Clear Timer on Compare*/
    TCCR0A = 1<<WGM01;
    OCR0A = 0xF9;
    TIMSK0|=(1<<OCIE0A);   //Set the interrupt request
    sei(); //Enable interrupt
 
    TCCR0B|=(1<<CS01);    //Set the prescale 1/64 clock
    TCCR0B|=(1<<CS00);


    pinMode(analogPinSeat1, INPUT);
    pinMode(analogPinSeat2, INPUT);
    pinMode(AnalogPinRef, INPUT);
    pinMode(relaisPin, OUTPUT);
    digitalWrite(relaisPin, HIGH);
}

uint8_t skipClear = 0;
void updateDisplay(int tempRear, int tempSeat, bool stRear, bool stSeat) {
    OVERWRITE(tempRear, 10)
    OVERWRITE(tempSeat, 20)
    OVERWRITE(stRear, true)
    OVERWRITE(stSeat, false)
    
    if(skipClear == 0U) {
        lcd.clear();
    }
    skipClear = (skipClear + 1U) % 5;

    lcd.setCursor(0,0);
    lcd.print("Ruckenlehne");
    lcd.setCursor(0,1);
    lcd.print("Temp: ");
    lcd.print(tempRear);
    lcd.print("C -");
    lcd.setCursor(14,1);
    lcd.print(stRear ? "Heizen" : "Warten");

    lcd.setCursor(0,2);
    lcd.print("Sitzflache");
    lcd.setCursor(0,3);
    lcd.print("Temp: ");
    lcd.print(tempSeat);
    lcd.print("C -");
    lcd.setCursor(14,3);
    lcd.print(stSeat ? "Heizen" : "Warten");

    return;
}

long memCoolDown = 0UL;

void loop() {
    float uref = readRefVoltage();

    int ntc_seat1 = readNTCSeat1(uref);
    int ntc_seat2 = readNTCSeat2(uref);
    int temp_seat1 = convertTemp(ntc_seat1);
    int temp_seat2 = convertTemp(ntc_seat2);
    DPRINT("Main loop: ntc_seat1 / temp_seat1: ");
    DPRINT(ntc_seat1);
    DPRINT(" / ");
    DPRINT(temp_seat1);
    DPRINT("\n");
    updateDisplay(temp_seat1, temp_seat2, stHeat, stHeat);


    if( stHeat && timer > timer_heat ) {
        /* Stop heating until reset */ 
        stHeat = false;
        timer = 0UL;
        digitalWrite(relaisPin, stHeat);
    }

    if( stHeat && (temp_seat1 > maxTemp || temp_seat2 > maxTemp) ) {
        memCoolDown = timer;
        stHeat = false;
        stCoolDown = true;
        digitalWrite(relaisPin, stHeat);
    }
    
    if ( !stHeat && stCoolDown && timer > (memCoolDown + timer_cool) ) {
        /* Start heating again */ 
        stHeat = true;
        memCoolDown = 0UL;
        digitalWrite(relaisPin, stHeat);
    }
    delay(1000);
}

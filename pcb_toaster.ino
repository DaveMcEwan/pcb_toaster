/** Toaster Oven SMT soldering control
 *  Dave McEwan 2015-01-18
 *  Based on Adrian Bowyer's code from 2 November 2011
 *  Licence: GPL and CC
 *
 *  TypeK thermocouple with MAX6675 SPI interface is used as the temperature
 *    sensor.
 *  ArduinoUno uses GPIO on pin13 to control solid state relay which switches the
 *    power socket that the oven is plugged into.

 *  Important parts are:
 *  - TypeK thermocouple with premade SPI interface board.
 *    Bought on 2015-01-06 for 4.50 GBP.
 *    http://www.amazon.co.uk/gp/product/B00MO7T05I
 *  - Solid state relay.
 *    Bought on 2015-01-06 for 4.76 GBP
 *    http://www.amazon.co.uk/gp/product/B00F998JSM
 *  - Toaster oven 9 Litre, 800W.
 *    Bought on 2015-01-06 for 32 GBP
 *    http://www.amazon.co.uk/gp/product/B00K22YJEY
 *  - Pattress box with socket plate.
 */

#include "MAX6675.h"


const int SPI_SO = 8;
const int SPI_CS = 9;
const int SPI_SCK = 10;
const int tempUnits = 1; // Celcius (0=raw(0 to 1023), 1=celcius, 2=fahrenheit)
const int heatPin =  13; // Heater control pin, also connected to onboard LED.


// The temperature/time profile as {secs, temp}
// PLEN is the number of slots.
#define PLEN 6
long profile[PLEN][2] = {
                         {0, 15},
                         {120, 150},
                         {220, 183},
                         {280, 215},
                         {320, 183},
                         {350, 15} // Last time is when to freefall to resting temperature.
                        };

bool check_profile()
{
    // First time must be 0.
    if (profile[0][0] != 0) return false;

    // Times must be increasing.
    for (unsigned short i = 1; i < PLEN; i++)
    {
        if (profile[i][0] <= profile[i-1][0]) return false;
    }

    // Resting temperature must be sane.
    if (profile[PLEN-1][1] < 70) return false;

    return true;
}

bool done = false; // Flag indicate to loop that the process has finished.
// Linearly interpolate the profile for the current time in secs, t.
float target(unsigned long t)
{
    float ret = 0;
    // Find profile time slot we are in.
    unsigned short current;
    for (unsigned short i = 0; i < PLEN; i++)
    {
        if (t >= profile[i][0]) current = i;
        else break;
    }

    // Exit early if in the last slot.
    if (current >= PLEN-1)
    {
        done = true;
        return profile[PLEN-1][1];
    }

    // Calculate how far through the current slot we are.
    // Since we exit early if in the last slot we can count on current+1.
    unsigned int slot_begin = profile[current][0];
    unsigned int slot_end = profile[current+1][0];
    unsigned int slot_size = slot_end - slot_begin;
    float slot_progress = (t - slot_begin) / slot_size; // 0.0=begin, 1.0=end

    // Calculate difference in temperature over this slot.
    unsigned int t_begin = profile[current][1];
    unsigned int t_end = profile[current+1][1];
    signed int t_diff = t_end - t_begin;

    // Interpolate linearly.
    return t_begin + (slot_progress*t_diff);
}

// Linearly interpolate the profile for the current time in secs, t.
int target_orig(unsigned long t)
{
    if (t <= profile[0][0])
    {
        return profile[0][1];
    }
    else if (t >= profile[PLEN-1][0])
    {
        done = true; // We are off the end of the time curve
        return profile[PLEN-1][1];
    }

    for (int i = 1; i < PLEN-1; i++)
    {
        if (t <= profile[i][0])
          return (int)(profile[i-1][1] + ((t - profile[i-1][0])*(profile[i][1] - profile[i-1][1]))/
            (profile[i][0] - profile[i-1][0]));
    }
    return 0;
}

// Initialise thermocouple interface.
MAX6675 temp(SPI_CS, SPI_SO, SPI_SCK, tempUnits);

// Initialise everything else.
bool profile_okay = false;
void setup()
{
    pinMode(heatPin, OUTPUT);
    Serial.begin(9600);
    Serial.println("\n\n\nTime, target, temp");
    profile_okay = check_profile();
}

// Main loop.
unsigned long time = 0; // Time since start in seconds
void loop()
{
    if (profile_okay)
    {
        // Get the actual temperature
        float real_t = temp.read_temp();

        // Find the target temperature
        float tgt_t = target(time);

        // Simple bang-bang temperature control
        int heatState = (real_t < tgt_t) ? HIGH : LOW;

        // Turn the heater on or off (and the LED)
        digitalWrite(heatPin, heatState);

        // Send status over serial port.
        if (done)
        {
            // Bell to wake the user up...
            Serial.print((char)0x07);
            Serial.print((char)0x07);
            Serial.print("FINISHED ");
        }
        Serial.print(time);
        Serial.print(", ");
        Serial.print(tgt_t);
        Serial.print(", ");
        Serial.println(real_t);
    }
    else
    {
        Serial.println("ERROR with profile. Refusing to turn on.");
        Serial.print((char)0x07);
    }

    // Delay for a second.
    delay(1000);
    time++;   
}


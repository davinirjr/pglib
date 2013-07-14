
#ifndef JULIANDATE_H
#define JULIANDATE_H

enum
{
    JULIAN_START = 2451545, // 2000-01-01
};


void julianToDate(int julian, int& year, int& month, int& day);
uint32_t dateToJulian(int year, int month, int day);

#endif // JULIANDATE_H

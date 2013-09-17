
#include "pglib.h"

// PostgreSQL stores data as 32-bit Julian dates, offset from 2001-01-01.

enum
{
    JULIAN_START     = 2451545, // 2000-01-01
    GREGORIAN_OFFSET = 15 + 31*(10+12*1582)
};

void julianToDate(int julian, int& year, int& month, int& day)
{
    // This is the standard conversion from some book that I forgot to record the name of.  However, this
    // implementation is from one of my Java projects, so we need to double-check the C.

    int jalpha, ja, jb, jc, jd, je;

    ja = julian;
    if (ja >= GREGORIAN_OFFSET)
    {
        jalpha = (int) (((ja - 1867216) - 0.25) / 36524.25);
        ja = ja + 1 + jalpha - jalpha / 4;
    }

    jb = ja + 1524;
    jc = (int) (6680.0 + ((jb - 2439870) - 122.1) / 365.25);
    jd = 365 * jc + jc / 4;
    je = (int) ((jb - jd) / 30.6001);
    day = jb - jd - (int) (30.6001 * je);
    month = je - 1;
    if (month > 12)
        month = month - 12;
    year = jc - 4715;
    if (month > 2)
        year--;
    if (year <= 0)
        year--;
}

uint32_t dateToJulian(int year, int month, int day)
{
    int julianYear = year;
    if (year < 0)
        julianYear++;

    int julianMonth = month;
    if (month > 2)
    {
        julianMonth++;
    }
    else
    {
        julianYear--;
        julianMonth += 13;
    }

    double julian = (long)(365.25 * julianYear) + (long)(30.6001*julianMonth) + day + 1720995.0;
    if (day + 31 * (month + 12 * year) >= GREGORIAN_OFFSET)
    {
        // change over to Gregorian calendar
        int ja = (int)(0.01 * julianYear);
        julian += 2 - ja + (0.25 * ja);
    }

    return (int)julian;
}

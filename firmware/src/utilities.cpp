#include <stdio.h>
#include <main.h>
#include <utilities.h>

void IncrementDate(Date &date)
{
    const int daysPerMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (++date.day > daysPerMonth[date.month - 1])
    {
        date.day = 1;
        if (++date.month > 12)
        {
            date.month = 1;
            date.year++;
        }
    }
}

void FormatNumber(int number, char *buffer)
{
    if (number < 1000)
    {
        sprintf(buffer, "%i", number);
    }
    else if (number < 9999)
    {
        int iPart = number / 100;
        int fPart = number - (iPart * 100);
        fPart = fPart / 10;
        sprintf(buffer, "%i.%iK", iPart, fPart);
    }
    else if (number < 99999)
    {
        int iPart = number / 1000;
        int fPart = number - (iPart * 1000);
        fPart = fPart / 100;
        sprintf(buffer, "%i.%iK", iPart, fPart);
    }
    else if (number < 999999)
    {
        int iPart = number / 1000;
        sprintf(buffer, "%iK", iPart);
    }
    else if (number < 9999999)
    {
        int iPart = number / 100000;
        int fPart = number - (iPart * 100000);
        fPart = fPart / 10000;
        sprintf(buffer, "%i.%iM", iPart, fPart);
    }
    else if (number < 99999999)
    {
        int iPart = number / 1000000;
        int fPart = number - (iPart * 1000000);
        fPart = fPart / 100000;
        sprintf(buffer, "%i.%iM", iPart, fPart);
    }
    else if (number < 999999999)
    {
        int iPart = number / 1000000;
        sprintf(buffer, "%iM", iPart);
    }
}
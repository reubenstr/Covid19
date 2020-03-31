struct DisplayData
{

    int confirmedGlobal;
    int deathsGlobal;
    int recoveredGlobal;
    int defaultCountryId;
    int confirmedCountry;
    int deathsCountry;
    int recoveredCountry;

    int selectedCountryId;
    bool selectMode;
};

struct StatsData
{
    int confirmed;
    int deaths;
    int recovered;
};

struct Date
{
    int year;
    int month;
    int day;
};

enum HttpStatus
{
    Success = 200,
    ServerError = 500,
    HttpError = 0,
    JsonError = 1,
    NotReady = 2
};

enum IndicatorMode
{
    IndicatorOff,
    IndicatorOn,
    IndicatorFlash
};



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
    unsigned int confirmed;
    unsigned int deaths;
    unsigned int recovered;
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


enum ConnectionStatus
{
    NoConnection, 
    Connected,
    DownloadingRecords,
    DownloadedPaused
};


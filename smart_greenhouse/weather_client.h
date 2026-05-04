#ifndef WEATHER_CLIENT_H
#define WEATHER_CLIENT_H

struct WeatherForecast {
  bool willRain;
  int rainProbability;
  float forecastTemp;
  float humidity;
  int weatherCode;
  int hour;
  bool fetchSuccess;
};

WeatherForecast weatherFetch();
bool weatherUpload(WeatherForecast f);

#endif

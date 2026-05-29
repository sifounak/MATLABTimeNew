var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var customClay = require('./custom-clay');
var clay = new Clay(clayConfig, customClay);

function xhrRequest(url, type, callback) {
  var xhr = new XMLHttpRequest();
  xhr.onload = function() {
    callback(null, this.responseText);
  };
  xhr.onerror = function() {
    callback(new Error('Weather request failed'));
  };
  xhr.open(type, url);
  xhr.send();
}

function locationSuccess(pos) {
  var url = 'https://api.open-meteo.com/v1/forecast?' +
      'latitude=' + pos.coords.latitude +
      '&longitude=' + pos.coords.longitude +
      '&current=temperature_2m%2Cuv_index';

  xhrRequest(url, 'GET', function(error, responseText) {
    if (error) {
      console.log(error.message);
      return;
    }

    try {
      var json = JSON.parse(responseText);
      var dictionary = {
        'WeatherTemperatureC': Math.round(json.current.temperature_2m),
        'WeatherUV': Math.round(json.current.uv_index)
      };

      Pebble.sendAppMessage(dictionary,
        function() {
          console.log('Weather sent to watch');
        },
        function(e) {
          console.log('Weather send failed: ' + JSON.stringify(e));
        }
      );
    } catch (e) {
      console.log('Weather parse failed: ' + e.message);
    }
  });
}

function locationError(err) {
  console.log('Location request failed: ' + err.code);
}

function getWeather() {
  navigator.geolocation.getCurrentPosition(
    locationSuccess,
    locationError,
    { timeout: 15000, maximumAge: 60 * 60 * 1000 }
  );
}

Pebble.addEventListener('ready', function() {
  console.log('PebbleKit JS ready');
  getWeather();
});

Pebble.addEventListener('appmessage', function(e) {
  if (e.payload && e.payload.RequestWeather) {
    getWeather();
  }
});

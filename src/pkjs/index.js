var Clay = require('@rebble/clay');
var messageKeys = require('message_keys');
var clayConfig = require('./config');
var customClay = require('./custom-clay');
var clay = new Clay(clayConfig, customClay, { autoHandleEvents: false });

var WEATHER_MIN_UPDATE_MS = 30 * 60 * 1000;
var WEATHER_UPDATED_AT_KEY = 'weather-updated-at';

var INTEGER_SETTING_KEYS = [
  'HourFormat',
  'RotateSideText',
  'VibeOnDisconnect',
  'VibeOnConnect'
];

var SELECT_SETTING_DEFINITIONS = {
  'TemperatureUnit': {
    values: {'C': 0, 'F': 1, 'K': 2, '0': 0, '1': 1, '2': 2},
    configValues: ['C', 'F', 'K'],
    fallback: 'F'
  },
  'DateFormat': {
    values: {'words': 0, 'mdy': 1, 'dmy': 2, '0': 0, '1': 1, '2': 2},
    configValues: ['words', 'mdy', 'dmy'],
    fallback: 'words'
  },
  'LeadingZeros': {
    values: {'none': 0, 'date': 1, 'time': 2, 'dateTime': 3, '0': 0, '1': 1, '2': 2, '3': 3},
    configValues: ['none', 'date', 'time', 'dateTime'],
    fallback: 'none'
  },
  'ComplicationLeft': {
    values: {'empty': 0, 'temperature': 1, 'battery': 2, 'uv': 3, '0': 0, '1': 1, '2': 2, '3': 3},
    configValues: ['empty', 'temperature', 'battery', 'uv'],
    fallback: 'uv'
  },
  'ComplicationMiddle': {
    values: {'empty': 0, 'temperature': 1, 'battery': 2, 'uv': 3, '0': 0, '1': 1, '2': 2, '3': 3},
    configValues: ['empty', 'temperature', 'battery', 'uv'],
    fallback: 'battery'
  },
  'ComplicationRight': {
    values: {'empty': 0, 'temperature': 1, 'battery': 2, 'uv': 3, '0': 0, '1': 1, '2': 2, '3': 3},
    configValues: ['empty', 'temperature', 'battery', 'uv'],
    fallback: 'temperature'
  },
  'LogoRotationTrigger': {
    values: {'off': 0, 'doubleTap': 1, 'shake': 2, 'minute': 3, 'hour': 4, '0': 0, '1': 1, '2': 2, '3': 3, '4': 4},
    configValues: ['off', 'doubleTap', 'shake', 'minute', 'hour'],
    fallback: 'off'
  }
};

function normalizeSelectForConfig(key, value) {
  var definition = SELECT_SETTING_DEFINITIONS[key];
  var normalized = definition.configValues[definition.values[value]];
  return normalized || definition.fallback;
}

function migrateStoredConfigSettings() {
  var settings;
  var changed = false;

  try {
    settings = JSON.parse(localStorage.getItem('clay-settings')) || {};
  } catch (e) {
    console.log('Settings migration skipped: ' + e.message);
    return;
  }

  Object.keys(SELECT_SETTING_DEFINITIONS).forEach(function(key) {
    if (settings[key] === undefined) {
      return;
    }

    var normalized = normalizeSelectForConfig(key, settings[key]);
    if (settings[key] !== normalized) {
      settings[key] = normalized;
      changed = true;
    }
  });

  if (changed) {
    localStorage.setItem('clay-settings', JSON.stringify(settings));
  }
}

function normalizeSettingsForWatch(settings) {
  Object.keys(SELECT_SETTING_DEFINITIONS).forEach(function(key) {
    var messageKey = messageKeys[key];
    if (messageKey === undefined || settings[messageKey] === undefined) {
      return;
    }

    var definition = SELECT_SETTING_DEFINITIONS[key];
    var value = definition.values[settings[messageKey]];
    if (value !== undefined) {
      settings[messageKey] = value;
    }
  });

  INTEGER_SETTING_KEYS.forEach(function(key) {
    var messageKey = messageKeys[key];
    if (messageKey === undefined) {
      return;
    }

    var value = settings[messageKey];
    var numericValue = parseInt(value, 10);

    if (value !== undefined && !isNaN(numericValue)) {
      settings[messageKey] = numericValue;
    }
  });

  return settings;
}

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
          localStorage.setItem(WEATHER_UPDATED_AT_KEY, Date.now().toString());
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

function weatherShouldUpdate() {
  var updatedAt = parseInt(localStorage.getItem(WEATHER_UPDATED_AT_KEY), 10);

  if (isNaN(updatedAt)) {
    return true;
  }

  return Date.now() - updatedAt >= WEATHER_MIN_UPDATE_MS;
}

function getWeatherIfStale() {
  if (weatherShouldUpdate()) {
    getWeather();
  }
}

Pebble.addEventListener('ready', function() {
  console.log('PebbleKit JS ready');
  getWeatherIfStale();
});

Pebble.addEventListener('showConfiguration', function() {
  migrateStoredConfigSettings();
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response) {
    return;
  }

  var settings = normalizeSettingsForWatch(clay.getSettings(e.response));
  Pebble.sendAppMessage(settings,
    function() {
      console.log('Settings sent to watch');
    },
    function(error) {
      console.log('Settings send failed: ' + JSON.stringify(error));
    }
  );
});

Pebble.addEventListener('appmessage', function(e) {
  if (e.payload && e.payload.RequestWeather) {
    getWeather();
  }
});

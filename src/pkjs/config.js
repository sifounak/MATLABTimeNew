module.exports = [
  {
    "type": "heading",
    "defaultValue": "MATLAB Time Settings"
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Logo Rotation"
      },
      {
        "type": "select",
        "messageKey": "LogoRotationTrigger",
        "label": "Trigger:",
        "defaultValue": "off",
        "options": [
          { "label": "Off", "value": "off" },
          { "label": "Double tap", "value": "doubleTap" },
          { "label": "Shake", "value": "shake" },
          { "label": "Minute", "value": "minute" },
          { "label": "Hour", "value": "hour" }
        ]
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Colors"
      },
      {
        "type": "color",
        "messageKey": "BackgroundColor",
        "defaultValue": "0x000000",
        "label": "Background Color"
      },
      {
        "type": "color",
        "messageKey": "TextColor",
        "defaultValue": "0xFFFFFF",
        "label": "Text Color"
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Units / Format"
      },
      {
        "type": "toggle",
        "messageKey": "HourFormat",
        "label": "24-Hour Clock",
        "defaultValue": false
      },
      {
        "type": "select",
        "messageKey": "LeadingZeros",
        "label": "Show Leading Zeros",
        "defaultValue": "none",
        "options": [
          { "label": "None", "value": "none" },
          { "label": "Date", "value": "date" },
          { "label": "Time", "value": "time" },
          { "label": "Date and Time", "value": "dateTime" }
        ]
      },
      {
        "type": "select",
        "messageKey": "DateFormat",
        "label": "Date Format",
        "defaultValue": "words",
        "options": [
          { "label": "eee MMM d", "value": "words" },
          { "label": "MM/dd/yyyy", "value": "mdy" },
          { "label": "dd/MM/yyyy", "value": "dmy" }
        ]
      },
      {
        "type": "select",
        "messageKey": "TemperatureUnit",
        "label": "Temperature",
        "defaultValue": "F",
        "options": [
          { "label": "Celsius", "value": "C" },
          { "label": "Fahrenheit", "value": "F" },
          { "label": "Kelvin", "value": "K" }
        ]
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Complications"
      },
      {
        "type": "toggle",
        "messageKey": "RotateSideText",
        "label": "Curved Complication Layout",
        "defaultValue": true,
        "description": "(Gabbro only) Angle complication text to match curvature of round displays. Turn this off for better battery efficiency."
      },
      {
        "type": "select",
        "messageKey": "ComplicationLeft",
        "label": "Left Slot",
        "defaultValue": "uv",
        "options": [
          { "label": "--Empty--", "value": "empty" },
          { "label": "Battery", "value": "battery" },
          { "label": "Temperature", "value": "temperature" },
          { "label": "UV Index", "value": "uv" }
        ]
      },
      {
        "type": "select",
        "messageKey": "ComplicationMiddle",
        "label": "Middle Slot",
        "defaultValue": "battery",
        "options": [
          { "label": "--Empty--", "value": "empty" },
          { "label": "Battery", "value": "battery" },
          { "label": "Temperature", "value": "temperature" },
          { "label": "UV Index", "value": "uv" }
        ]
      },
      {
        "type": "select",
        "messageKey": "ComplicationRight",
        "label": "Right Slot",
        "defaultValue": "temperature",
        "options": [
          { "label": "--Empty--", "value": "empty" },
          { "label": "Battery", "value": "battery" },
          { "label": "Temperature", "value": "temperature" },
          { "label": "UV Index", "value": "uv" }
        ]
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Bluetooth Alerts"
      },
      {
        "type": "toggle",
        "messageKey": "VibeOnDisconnect",
        "label": "Vibrate on Disconnect",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "VibeOnConnect",
        "label": "Vibrate on Reconnect",
        "defaultValue": true
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];

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
        "defaultValue": "Units & Display"
      },
      {
        "type": "toggle",
        "messageKey": "HourFormat",
        "label": "Use 24-Hour Format",
        "defaultValue": false
      },
      {
        "type": "select",
        "messageKey": "DateFormat",
        "label": "Date Format",
        "defaultValue": "0",
        "options": [
          { "label": "eee MMM d", "value": "0" },
          { "label": "MM/dd/yyyy", "value": "1" },
          { "label": "dd/MM/yyyy", "value": "2" }
        ]
      },
      {
        "type": "select",
        "messageKey": "TemperatureUnit",
        "label": "Temperature",
        "defaultValue": "F",
        "options": [
          { "label": "Celsius", "value": "C" },
          { "label": "Fahrenheit", "value": "F" }
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
        "type": "select",
        "messageKey": "ComplicationLeft",
        "label": "Left Slot",
        "defaultValue": "3",
        "options": [
          { "label": "--Empty--", "value": "0" },
          { "label": "Battery", "value": "2" },
          { "label": "Temperature", "value": "1" },
          { "label": "UV Index", "value": "3" }
        ]
      },
      {
        "type": "select",
        "messageKey": "ComplicationMiddle",
        "label": "Middle Slot",
        "defaultValue": "1",
        "options": [
          { "label": "--Empty--", "value": "0" },
          { "label": "Battery", "value": "2" },
          { "label": "Temperature", "value": "1" },
          { "label": "UV Index", "value": "3" }
        ]
      },
      {
        "type": "select",
        "messageKey": "ComplicationRight",
        "label": "Right Slot",
        "defaultValue": "2",
        "options": [
          { "label": "--Empty--", "value": "0" },
          { "label": "Battery", "value": "2" },
          { "label": "Temperature", "value": "1" },
          { "label": "UV Index", "value": "3" }
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

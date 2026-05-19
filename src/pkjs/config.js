module.exports = [
  {
    "type": "heading",
    "defaultValue": "MATLAB Time Settings"
  },
  {
    "type": "text",
    "defaultValue": "Customize your watchface appearance and preferences."
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
        "defaultValue": "Time & Date"
      },
      {
        "type": "toggle",
        "messageKey": "HourFormat",
        "label": "Use 24-Hour Format",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "ShowDate",
        "label": "Show Date",
        "defaultValue": true
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Weather"
      },
      {
        "type": "toggle",
        "messageKey": "TemperatureUnit",
        "label": "Use Fahrenheit",
        "defaultValue": false
      },
      {
        "type": "toggle",
        "messageKey": "ShowConditions",
        "label": "Show Weather Conditions",
        "defaultValue": true
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Display"
      },
      {
        "type": "toggle",
        "messageKey": "ShowBatteryPercent",
        "label": "Show Battery Percentage",
        "defaultValue": true
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
        "defaultValue": false
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];

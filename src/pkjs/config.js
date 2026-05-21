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
        "defaultValue": "Time & Date"
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
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Data"
      },
      {
        "type": "toggle",
        "messageKey": "UseCelsius",
        "label": "Use Celsius",
        "defaultValue": false
      },
      {
        "type": "toggle",
        "messageKey": "ShowUV",
        "label": "Show UV Index",
        "defaultValue": true
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
        "defaultValue": true
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];

import Poco from "commodetto/Poco";
import parseBMF from "commodetto/parseBMF";
import parseRLE from "commodetto/parseRLE";
import Battery from "embedded:sensor/Battery";
import Location from "embedded:sensor/Location";
import Message from "pebble/message";

const render = new Poco(screen);

// --- Fonts ---

function getFont(name, size) {
    const font = parseBMF(new Resource(`${name}-${size}.fnt`));
    font.bitmap = parseRLE(new Resource(`${name}-${size}-alpha.bm4`));
    return font;
}

const timeFont = getFont("Olyford-Semi-Bold", 48);
const dateFont = getFont("Olyford-Semi-Bold", 26);
const smallFont = getFont("Olyford-Semi-Bold", 24);

// --- Colors ---

const green = render.makeColor(0, 170, 0);
const yellow = render.makeColor(255, 170, 0);
const red = render.makeColor(255, 0, 0);

let bgColor, textColor;

function updateColors() {
    bgColor = render.makeColor(settings.backgroundColor.r,
        settings.backgroundColor.g, settings.backgroundColor.b);
    textColor = render.makeColor(settings.textColor.r,
        settings.textColor.g, settings.textColor.b);
}

// --- Settings ---

const DEFAULT_SETTINGS = {
    backgroundColor: { r: 0, g: 0, b: 0 },
    textColor: { r: 255, g: 255, b: 255 },
    useFahrenheit: false,
    showDate: true,
    use24Hour: true
};

function loadSettings() {
    const stored = localStorage.getItem("settings");
    if (stored) {
        try {
            return { ...DEFAULT_SETTINGS, ...JSON.parse(stored) };
        } catch (e) {
            console.log("Failed to parse settings");
        }
    }
    return { ...DEFAULT_SETTINGS };
}

function saveSettings() {
    localStorage.setItem("settings", JSON.stringify(settings));
}

let settings = loadSettings();
updateColors();

// --- Constants ---

const DAYS = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];
const MONTHS = ["Jan", "Feb", "Mar", "Apr", "May", "Jun",
                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];

// --- State ---

let lastDate = new Date();
let weather = null;
let batteryPercent = 100;
let isConnected = true;

// --- Battery ---

const battery = new Battery({
    onSample() {
        batteryPercent = this.sample().percent;
        drawScreen();
    }
});
batteryPercent = battery.sample().percent;

// --- Connection ---

function checkConnection() {
    isConnected = watch.connected.app;
    drawScreen();
}
watch.addEventListener("connected", checkConnection);
checkConnection();

// --- Weather ---

function getWeatherDescription(code) {
    if (code === 0) return "Clear";
    if (code <= 3) return "Cloudy";
    if (code <= 48) return "Fog";
    if (code <= 55) return "Drizzle";
    if (code <= 57) return "Fz. Drizzle";
    if (code <= 65) return "Rain";
    if (code <= 67) return "Fz. Rain";
    if (code <= 75) return "Snow";
    if (code <= 77) return "Snow Grains";
    if (code <= 82) return "Showers";
    if (code <= 86) return "Snow Shwrs";
    if (code === 95) return "T-Storm";
    if (code <= 99) return "T-Storm";
    return "Unknown";
}

function loadCachedWeather() {
    const cached = localStorage.getItem("weather");
    const cachedTime = localStorage.getItem("weatherTime");
    if (cached && cachedTime) {
        const age = Date.now() - Number(cachedTime);
        if (age < 60 * 60 * 1000) {
            try {
                weather = JSON.parse(cached);
                return true;
            } catch (e) {
                console.log("Failed to parse cached weather");
            }
        }
    }
    return false;
}

function saveWeather() {
    if (weather) {
        localStorage.setItem("weather", JSON.stringify(weather));
        localStorage.setItem("weatherTime", String(Date.now()));
    }
}

function requestLocation() {
    new Location({
        onSample() {
            const sample = this.sample();
            this.close();
            fetchWeather(sample.latitude, sample.longitude);
        }
    });
}

async function fetchWeather(latitude, longitude) {
    try {
        const params = {
            latitude,
            longitude,
            current: "temperature_2m,weather_code"
        };
        if (settings.useFahrenheit) {
            params.temperature_unit = "fahrenheit";
        }

        const url = new URL("https://api.open-meteo.com/v1/forecast");
        url.search = new URLSearchParams(params);

        const response = await fetch(url);
        const data = await response.json();

        weather = {
            temp: Math.round(data.current.temperature_2m),
            conditions: getWeatherDescription(data.current.weather_code)
        };

        saveWeather();
        drawScreen();
    } catch (e) {
        console.log("Weather fetch error: " + e);
    }
}

loadCachedWeather();

// --- Drawing ---

function drawBluetoothIcon(midX, midY) {
    if (isConnected) return;

    const length = 8;
    const thickness = 3;
    const leftX = midX - length;
    const rightX = midX + length;
    const topY = midY - 2 * length;
    const botY = midY + 2 * length;

    // Vertical spine
    render.drawLine(midX, topY, midX, botY, red, thickness);
    // Long diagonal lines
    render.drawLine(leftX, midY - length, rightX, midY + length, red, thickness);
    render.drawLine(leftX, midY + length, rightX, midY - length, red, thickness);
    // Short diagonal lines (top and bottom arrows)
    render.drawLine(midX, topY, rightX, midY - length, red, thickness);
    render.drawLine(midX, botY, rightX, midY + length, red, thickness);
}

function drawScreen(event) {
    const now = event?.date ?? lastDate;
    if (event?.date) lastDate = event.date;

    const w = render.unobstructed.width;
    const h = render.unobstructed.height;

    // Compute layout positions
    const timeY = h * 0.30;
    const dateY = timeY + timeFont.height * 0.86;
    const weatherY = h - smallFont.height * 2 - 10;
    const batteryY = h - smallFont.height - 6;

    render.begin();
    render.fillRectangle(bgColor, 0, 0, render.width, render.height);

    // Bluetooth icon (top-right area)
    drawBluetoothIcon(w * 0.82, timeY - 10);

    // Time
    let hours = now.getHours();
    let ampm = "";
    if (!settings.use24Hour) {
        ampm = hours >= 12 ? " PM" : " AM";
        hours = hours % 12 || 12;
    }
    const timeStr = `${String(hours)}:${String(now.getMinutes()).padStart(2, "0")}${ampm}`;
    let tw = render.getTextWidth(timeStr, timeFont);
    render.drawText(timeStr, timeFont, textColor, (w - tw) / 2, timeY);

    // Date
    if (settings.showDate) {
        const dayName = DAYS[now.getDay()];
        const monthName = MONTHS[now.getMonth()];
        const dateStr = `${dayName} ${monthName} ${String(now.getDate()).padStart(2, "0")}`;
        let dw = render.getTextWidth(dateStr, dateFont);
        render.drawText(dateStr, dateFont, textColor, (w - dw) / 2, dateY);
    }

    // Weather
    if (weather) {
        const unit = settings.useFahrenheit ? "F" : "C";
        const weatherStr = `${weather.temp}\xB0${unit}  ${weather.conditions}`;
        let ww = render.getTextWidth(weatherStr, smallFont);
        render.drawText(weatherStr, smallFont, textColor, (w - ww) / 2, weatherY);
    } else {
        const msg = "Loading...";
        let ww = render.getTextWidth(msg, smallFont);
        render.drawText(msg, smallFont, textColor, (w - ww) / 2, weatherY);
    }

    // Battery percentage
    let battColor = green;
    if (batteryPercent <= 20) battColor = red;
    else if (batteryPercent <= 40) battColor = yellow;

    const battStr = `${batteryPercent}%`;
    let bw = render.getTextWidth(battStr, smallFont);
    render.drawText(battStr, smallFont, battColor, (w - bw) / 2, batteryY);

    render.end();
}

// --- Event Listeners ---

watch.addEventListener("minutechange", drawScreen);
watch.addEventListener("hourchange", requestLocation);
watch.addEventListener("resize", drawScreen);

// --- Settings Receiver ---

const message = new Message({
    keys: ["BackgroundColor", "TextColor", "TemperatureUnit", "ShowDate", "HourFormat"],
    onReadable() {
        const msg = this.read();

        const bg = msg.get("BackgroundColor");
        if (bg !== undefined) {
            settings.backgroundColor = { r: (bg >> 16) & 0xFF, g: (bg >> 8) & 0xFF, b: bg & 0xFF };
        }
        const tc = msg.get("TextColor");
        if (tc !== undefined) {
            settings.textColor = { r: (tc >> 16) & 0xFF, g: (tc >> 8) & 0xFF, b: tc & 0xFF };
        }
        const tu = msg.get("TemperatureUnit");
        if (tu !== undefined) {
            settings.useFahrenheit = tu === 1;
        }
        const sd = msg.get("ShowDate");
        if (sd !== undefined) {
            settings.showDate = sd === 1;
        }
        const hf = msg.get("HourFormat");
        if (hf !== undefined) {
            settings.use24Hour = hf === 1;
        }

        saveSettings();
        updateColors();
        drawScreen();

        if (tu !== undefined) {
            requestLocation();
        }
    }
});

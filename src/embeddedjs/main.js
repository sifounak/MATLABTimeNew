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

// --- Logo ---
// Frame indices match position in package.json media array (0=Logo00, 1=Logo01, etc.)

const LOGO_TOTAL_FRAMES = 20;
const LOGO_STATIC_FRAME = 0;
let logoImage = new Poco.PebbleBitmap(LOGO_STATIC_FRAME);
let logoAnimating = false;

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
    use24Hour: true,
    showBatteryPercent: true,
    showConditions: true,
    vibeOnDisconnect: true,
    vibeOnConnect: false,
    rotateLogo: 4  // 0=off, 1=minute, 2=hour, 3=shake, 4=100% battery (debug)
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
let batteryCharging = false;
let isConnected = true;

// --- Battery ---

let prevBatteryPercent = 0;

const battery = new Battery({
    onSample() {
        const sample = this.sample();
        prevBatteryPercent = batteryPercent;
        batteryPercent = sample.percent;
        batteryCharging = sample.charging;
        drawScreen();

        // Trigger logo animation when battery reaches 100% (debug trigger)
        if (settings.rotateLogo === 4 && batteryPercent === 100 && prevBatteryPercent < 100) {
            rotateLogo();
        }
    }
});
const initialBattery = battery.sample();
batteryPercent = initialBattery.percent;
prevBatteryPercent = batteryPercent;
batteryCharging = initialBattery.charging;

// --- Connection ---

let connectionInitialized = false;

function checkConnection() {
    const wasConnected = isConnected;
    isConnected = watch.connected.app;

    // Vibrate on connection state changes (skip initial check)
    if (connectionInitialized) {
        if (!isConnected && wasConnected && settings.vibeOnDisconnect) {
            watch.vibrate("long");
        } else if (isConnected && !wasConnected && settings.vibeOnConnect) {
            watch.vibrate("double");
        }
    }
    connectionInitialized = true;

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

// --- Logo Animation ---

function rotateLogo() {
    if (logoAnimating) return;
    logoAnimating = true;
    let frameIndex = 2;

    const intervalId = setInterval(() => {
        // Load new frame (indices 1-20 in media array)
        logoImage = new Poco.PebbleBitmap(frameIndex);

        const w = render.unobstructed.width;
        const logoX = ((w - logoImage.width) / 2) | 0;
        const logoY = 4;

        render.begin(logoX, logoY, logoImage.width, logoImage.height);
        render.fillRectangle(bgColor, logoX, logoY, logoImage.width, logoImage.height);
        render.drawBitmap(logoImage, logoX, logoY);
        render.end();

        frameIndex += 2;
        if (frameIndex > LOGO_TOTAL_FRAMES) {
            // Animation complete — reset to static frame
            clearInterval(intervalId);
            logoImage = new Poco.PebbleBitmap(LOGO_STATIC_FRAME);
            logoAnimating = false;

            // Redraw logo area with static frame
            render.begin(logoX, logoY, logoImage.width, logoImage.height);
            render.fillRectangle(bgColor, logoX, logoY, logoImage.width, logoImage.height);
            render.drawBitmap(logoImage, logoX, logoY);
            render.end();
        }
    }, 100);
}

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

    // Logo (centered at top)
    if (!logoAnimating) {
        const logoX = ((w - logoImage.width) / 2) | 0;
        const logoY = 4;
        render.drawBitmap(logoImage, logoX, logoY);
    }

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
        let weatherStr = `${weather.temp}\xB0${unit}`;
        if (settings.showConditions) {
            weatherStr += `  ${weather.conditions}`;
        }
        let ww = render.getTextWidth(weatherStr, smallFont);
        render.drawText(weatherStr, smallFont, textColor, (w - ww) / 2, weatherY);
    } else {
        const msg = "Loading...";
        let ww = render.getTextWidth(msg, smallFont);
        render.drawText(msg, smallFont, textColor, (w - ww) / 2, weatherY);
    }

    // Battery percentage with charging indicator
    if (settings.showBatteryPercent || batteryCharging) {
        let battColor = green;
        if (batteryPercent <= 20) battColor = red;
        else if (batteryPercent <= 40) battColor = yellow;

        const battStr = batteryCharging
            ? `Charging ${batteryPercent}%`
            : `${batteryPercent}%`;
        let bw = render.getTextWidth(battStr, smallFont);
        render.drawText(battStr, smallFont, battColor, (w - bw) / 2, batteryY);
    }

    render.end();
}

// --- Event Listeners ---

watch.addEventListener("minutechange", (event) => {
    drawScreen(event);
    if (settings.rotateLogo === 1) rotateLogo();
});

watch.addEventListener("hourchange", (event) => {
    requestLocation();
    if (settings.rotateLogo === 2) rotateLogo();
});

watch.addEventListener("resize", drawScreen);

// Shake/tap trigger for logo animation
watch.addEventListener("accel-tap", () => {
    if (settings.rotateLogo === 3) rotateLogo();
});

// --- Settings Receiver ---

const message = new Message({
    keys: ["BackgroundColor", "TextColor", "TemperatureUnit", "ShowDate",
           "HourFormat", "ShowBatteryPercent", "ShowConditions",
           "VibeOnDisconnect", "VibeOnConnect", "RotateLogo"],
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
        const sbp = msg.get("ShowBatteryPercent");
        if (sbp !== undefined) {
            settings.showBatteryPercent = sbp === 1;
        }
        const sc = msg.get("ShowConditions");
        if (sc !== undefined) {
            settings.showConditions = sc === 1;
        }
        const vd = msg.get("VibeOnDisconnect");
        if (vd !== undefined) {
            settings.vibeOnDisconnect = vd === 1;
        }
        const vc = msg.get("VibeOnConnect");
        if (vc !== undefined) {
            settings.vibeOnConnect = vc === 1;
        }
        const rl = msg.get("RotateLogo");
        if (rl !== undefined) {
            settings.rotateLogo = rl;
        }

        saveSettings();
        updateColors();
        drawScreen();

        if (tu !== undefined) {
            requestLocation();
        }
    }
});

import Poco from "commodetto/Poco";
import parseBMF from "commodetto/parseBMF";
import parseRLE from "commodetto/parseRLE";
import Battery from "embedded:sensor/Battery";
import Location from "embedded:sensor/Location";
import Message from "pebble/message";

console.log("=== Starting ===");

const render = new Poco(screen);

// --- Platform detection ---
// Emery is 200x228 (rectangular), gabbro is 260x260 (round)
const isEmery = render.height > render.width;

// --- Fonts ---
function getFont(name, size) {
    const font = parseBMF(new Resource(`${name}-${size}.fnt`));
    font.bitmap = parseRLE(new Resource(`${name}-${size}-alpha.bm4`));
    return font;
}

const fonts = {
    time: getFont("Olyford-Semi-Bold", isEmery ? 40 : 48),
    date: getFont("Olyford-Semi-Bold", isEmery ? 22 : 26),
    small: getFont("Olyford-Semi-Bold", 16)
};

function fillBackground() {
    // Fill entire screen with background color; C logo layer redraws on top
    render.begin();
    render.fillRectangle(colors.bg, 0, 0, render.width, render.height);
    render.end();
}

// --- Colors ---
const colors = {
    green: render.makeColor(0, 170, 0),
    yellow: render.makeColor(255, 170, 0),
    red: render.makeColor(255, 0, 0),
    bg: null,
    text: null
};

function updateColors() {
    colors.bg = render.makeColor(settings.backgroundColor.r,
        settings.backgroundColor.g, settings.backgroundColor.b);
    colors.text = render.makeColor(settings.textColor.r,
        settings.textColor.g, settings.textColor.b);
}

// --- Settings ---
// Complication types: 0=empty, 1=temperature, 2=battery, 3=uv
const DEFAULT_SETTINGS = {
    backgroundColor: { r: 0, g: 0, b: 0 },
    textColor: { r: 255, g: 255, b: 255 },
    useCelsius: false,
    dateFormat: 0,
    use24Hour: false,
    complicationLeft: 1,
    complicationMiddle: 2,
    complicationRight: 3,
    vibeOnDisconnect: true,
    vibeOnConnect: true
};

function loadSettings() {
    const stored = localStorage.getItem("settings");
    if (stored) {
        try {
            const parsed = { ...DEFAULT_SETTINGS, ...JSON.parse(stored) };
            parsed.complicationLeft = Number(parsed.complicationLeft);
            parsed.complicationMiddle = Number(parsed.complicationMiddle);
            parsed.complicationRight = Number(parsed.complicationRight);
            return parsed;
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

// --- Layout constants ---
const screenW = render.unobstructed.width;
const screenH = render.unobstructed.height;
const _timeY = screenH / 2 - fonts.time.height * 0.25;
const _logoY = (((_timeY - 90) / 2) + 5) | 0;
const LOGO_BOTTOM = _logoY + 90;

// --- State ---
const state = {
    lastDate: new Date(),
    weather: null,
    batteryPercent: 100,
    batteryCharging: false,
    isConnected: true,
    connectionInitialized: false
};

// --- Battery ---
const battery = new Battery({
    onSample() {
        const sample = this.sample();
        state.batteryPercent = sample.percent;
        state.batteryCharging = sample.charging;
        drawScreen();
    }
});
const initialBattery = battery.sample();
state.batteryPercent = initialBattery.percent;
state.batteryCharging = initialBattery.charging;

// --- Connection ---
function checkConnection() {
    const wasConnected = state.isConnected;
    state.isConnected = watch.connected.app;

    if (state.connectionInitialized) {
        if (!state.isConnected && wasConnected && settings.vibeOnDisconnect) {
            watch.vibrate("long");
        } else if (state.isConnected && !wasConnected && settings.vibeOnConnect) {
            watch.vibrate("double");
        }
    }
    state.connectionInitialized = true;

    drawScreen();
}
watch.addEventListener("connected", checkConnection);
checkConnection();

// --- Drawing ---


function drawBluetoothIcon(midX, midY) {
    if (state.isConnected) return;

    const length = 8;
    const thickness = 3;
    const leftX = midX - length;
    const rightX = midX + length;
    const topY = midY - 2 * length;
    const botY = midY + 2 * length;

    // Own render pass for bluetooth (above LOGO_BOTTOM)
    const iconLeft = leftX - thickness;
    const iconTop = topY - thickness;
    const iconW = (rightX - leftX) + thickness * 2;
    const iconH = (botY - topY) + thickness * 2;
    render.begin(iconLeft, iconTop, iconW, iconH);
    render.fillRectangle(colors.bg, iconLeft, iconTop, iconW, iconH);
    render.drawLine(midX, topY, midX, botY, colors.red, thickness);
    render.drawLine(leftX, midY - length, rightX, midY + length, colors.red, thickness);
    render.drawLine(leftX, midY + length, rightX, midY - length, colors.red, thickness);
    render.drawLine(midX, topY, rightX, midY - length, colors.red, thickness);
    render.drawLine(midX, botY, rightX, midY + length, colors.red, thickness);
    render.end();
}

function drawScreen(event) {
    const now = event?.date ?? state.lastDate;
    if (event?.date) state.lastDate = event.date;

    const w = screenW;
    const h = screenH;

    const timeY = h / 2 - fonts.time.height * 0.25 + (isEmery ? 12 : 0);
    const dateY = timeY + fonts.time.height * 0.86;
    const complicationY = isEmery ? h - fonts.small.height - 5 : h - fonts.small.height - 10;

    // Main render pass: everything below the logo area
    render.begin(0, LOGO_BOTTOM, w, h - LOGO_BOTTOM);
    render.fillRectangle(colors.bg, 0, LOGO_BOTTOM, w, h - LOGO_BOTTOM);

    // Time
    let hours = now.getHours();
    let ampm = "";
    if (!settings.use24Hour) {
        ampm = hours >= 12 ? " PM" : " AM";
        hours = hours % 12 || 12;
    }
    const timeStr = `${String(hours)}:${String(now.getMinutes()).padStart(2, "0")}${ampm}`;
    let tw = render.getTextWidth(timeStr, fonts.time);
    render.drawText(timeStr, fonts.time, colors.text, (w - tw) / 2, timeY);

    // Date
    {
        let dateStr;
        const day = now.getDate();
        const month = now.getMonth();
        const year = now.getFullYear();
        if (settings.dateFormat === 1) {
            dateStr = `${String(month + 1).padStart(2, "0")}/${String(day).padStart(2, "0")}/${year}`;
        } else if (settings.dateFormat === 2) {
            dateStr = `${String(day).padStart(2, "0")}/${String(month + 1).padStart(2, "0")}/${year}`;
        } else {
            const dayName = DAYS[now.getDay()];
            const monthName = MONTHS[now.getMonth()];
            dateStr = `${dayName} ${monthName} ${day}`;
        }
        let dw = render.getTextWidth(dateStr, fonts.date);
        render.drawText(dateStr, fonts.date, colors.text, (w - dw) / 2, dateY);
    }

    // Complications
    function getComplicationStr(type) {
        if (type === 1 && state.weather) {
            const unit = settings.useCelsius ? "C" : "F";
            return `${state.weather.temp}\xB0${unit}`;
        }
        if (type === 2) {
            if (state.batteryCharging) return `Charging ${state.batteryPercent}%`;
            return `${state.batteryPercent}%`;
        }
        if (type === 3 && state.weather) {
            return `UV ${state.weather.uv}`;
        }
        return "";
    }

    const leftStr = getComplicationStr(settings.complicationLeft);
    const middleStr = getComplicationStr(settings.complicationMiddle);
    const rightStr = getComplicationStr(settings.complicationRight);

    if (isEmery) {
        if (leftStr) {
            render.drawText(leftStr, fonts.small, colors.text, 15, complicationY);
        }
        if (middleStr) {
            const mw = render.getTextWidth(middleStr, fonts.small);
            render.drawText(middleStr, fonts.small, colors.text, (w - mw) / 2, complicationY);
        }
        if (rightStr) {
            const rw = render.getTextWidth(rightStr, fonts.small);
            render.drawText(rightStr, fonts.small, colors.text, w - rw - 15, complicationY);
        }
    } else {
        // Gabbro: center the middle, flank left/right beside it (offset up)
        const midW = middleStr ? render.getTextWidth(middleStr, fonts.small) : 0;
        const midX = (w - midW) / 2;
        const flankY = complicationY - fonts.small.height / 2;
        if (middleStr) {
            render.drawText(middleStr, fonts.small, colors.text, midX, complicationY);
        }
        if (leftStr) {
            const lw = render.getTextWidth(leftStr, fonts.small);
            render.drawText(leftStr, fonts.small, colors.text, midX - 7 - lw, flankY);
        }
        if (rightStr) {
            render.drawText(rightStr, fonts.small, colors.text, midX + midW + 7, flankY);
        }
    }

    render.end();

    // Bluetooth icon gets its own render pass (it's above LOGO_BOTTOM)
    drawBluetoothIcon(w * 0.82, timeY - 10);
}

// --- Event Listeners ---
watch.addEventListener("minutechange", (event) => {
    drawScreen(event);
});
watch.addEventListener("resize", drawScreen);

// --- Settings Receiver ---
const message = new Message({
    keys: ["BackgroundColor", "TextColor", "UseCelsius", "DateFormat",
           "HourFormat", "ComplicationLeft", "ComplicationMiddle",
           "ComplicationRight", "VibeOnDisconnect", "VibeOnConnect"],
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
        const uc = msg.get("UseCelsius");
        if (uc !== undefined) {
            settings.useCelsius = uc === 1;
        }
        const df = msg.get("DateFormat");
        if (df !== undefined) {
            settings.dateFormat = df;
        }
        const hf = msg.get("HourFormat");
        if (hf !== undefined) {
            settings.use24Hour = hf === 1;
        }
        const cl = msg.get("ComplicationLeft");
        if (cl !== undefined) {
            settings.complicationLeft = Number(cl);
        }
        const cm = msg.get("ComplicationMiddle");
        if (cm !== undefined) {
            settings.complicationMiddle = Number(cm);
        }
        const cr = msg.get("ComplicationRight");
        if (cr !== undefined) {
            settings.complicationRight = Number(cr);
        }
        const vd = msg.get("VibeOnDisconnect");
        if (vd !== undefined) {
            settings.vibeOnDisconnect = vd === 1;
        }
        const vc = msg.get("VibeOnConnect");
        if (vc !== undefined) {
            settings.vibeOnConnect = vc === 1;
        }
        saveSettings();
        updateColors();
        fillBackground();
        drawScreen();
    }
});

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
                state.weather = JSON.parse(cached);
                return true;
            } catch (e) {
                console.log("Failed to parse cached weather");
            }
        }
    }
    return false;
}

function saveWeather() {
    if (state.weather) {
        localStorage.setItem("weather", JSON.stringify(state.weather));
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
        let url = `https://api.open-meteo.com/v1/forecast?latitude=${latitude}&longitude=${longitude}&current=temperature_2m%2Cweather_code%2Cuv_index`;
        if (!settings.useCelsius) {
            url += "&temperature_unit=fahrenheit";
        }

        const response = await fetch(url);
        const data = await response.json();

        state.weather = {
            temp: Math.round(data.current.temperature_2m),
            conditions: getWeatherDescription(data.current.weather_code),
            uv: Math.round(data.current.uv_index)
        };

        saveWeather();
        drawScreen();
    } catch (e) {
        console.log("Weather fetch error: " + e);
    }
}

loadCachedWeather();

// --- Initial Draw ---
fillBackground();
drawScreen();
watch.addEventListener("hourchange", requestLocation);


console.log("=== Watchface running ===");

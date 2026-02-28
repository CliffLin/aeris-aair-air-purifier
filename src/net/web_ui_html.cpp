#include "web_ui_html.h"

namespace {
const char kIndexHtml[] = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Aeris Home</title>
<style>
:root {
    --bg-a: #edf4ff;
    --bg-b: #f7f3eb;
    --card-bg: rgba(255, 255, 255, 0.68);
    --card-border: rgba(255, 255, 255, 0.9);
    --pm-bg: rgba(255, 255, 255, 0.62);
    --pm-border: rgba(255, 255, 255, 0.84);
    --chip-bg: rgba(255, 255, 255, 0.55);
    --chip-border: rgba(255, 255, 255, 0.9);
    --input-bg: rgba(255, 255, 255, 0.78);
    --input-border: rgba(255, 255, 255, 0.95);
    --button-bg: rgba(255, 255, 255, 0.88);
    --button-fg: #173258;
    --text: #1d2636;
    --muted: #62738d;
    --accent: #2e7bf4;
    --accent-soft: #ddedff;
    --good: #1f9f67;
    --mid: #d89a31;
    --bad: #c14a4a;
    --card-shadow: 0 12px 30px rgba(22, 29, 45, 0.08);
    --toast-bg: rgba(17, 31, 52, 0.92);
    --toast-fg: #ffffff;
}
body[data-theme="dark"] {
    --bg-a: #0f1626;
    --bg-b: #1a263b;
    --card-bg: rgba(13, 21, 35, 0.72);
    --card-border: rgba(127, 150, 182, 0.38);
    --pm-bg: rgba(16, 27, 44, 0.66);
    --pm-border: rgba(127, 150, 182, 0.32);
    --chip-bg: rgba(21, 34, 53, 0.72);
    --chip-border: rgba(127, 150, 182, 0.38);
    --input-bg: rgba(15, 25, 41, 0.8);
    --input-border: rgba(127, 150, 182, 0.44);
    --button-bg: rgba(16, 28, 45, 0.9);
    --button-fg: #dce9ff;
    --text: #edf4ff;
    --muted: #a6b8d7;
    --accent: #82afff;
    --accent-soft: rgba(130, 175, 255, 0.2);
    --good: #5ed7a1;
    --mid: #f0bc62;
    --bad: #ff8b8b;
    --card-shadow: 0 18px 34px rgba(0, 0, 0, 0.35);
    --toast-bg: rgba(8, 15, 28, 0.92);
    --toast-fg: #dce9ff;
}
* {
    box-sizing: border-box;
}
html, body {
    margin: 0;
    padding: 0;
}
body {
    font-family: "SF Pro Display", "Avenir Next", "Helvetica Neue", sans-serif;
    color: var(--text);
    background: linear-gradient(135deg, var(--bg-a) 0%, var(--bg-b) 100%);
    min-height: 100vh;
    overflow-x: hidden;
}
.orb {
    position: fixed;
    border-radius: 999px;
    filter: blur(32px);
    pointer-events: none;
    z-index: 0;
    opacity: 0.55;
}
.orb-a {
    width: 320px;
    height: 320px;
    top: -80px;
    right: -60px;
    background: #7db8ff;
}
.orb-b {
    width: 280px;
    height: 280px;
    bottom: -90px;
    left: -60px;
    background: #f8c4a4;
}
.shell {
    position: relative;
    z-index: 1;
    max-width: 1080px;
    margin: 0 auto;
    padding: 24px 18px 28px;
}
.top {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-bottom: 18px;
}
.title-wrap h1 {
    margin: 0;
    font-size: 28px;
    letter-spacing: 0.01em;
}
.title-wrap p {
    margin: 4px 0 0;
    color: var(--muted);
    font-size: 14px;
}
.status {
    display: flex;
    gap: 8px;
    align-items: center;
}
.chip {
    border: 1px solid var(--chip-border);
    background: var(--chip-bg);
    backdrop-filter: blur(10px);
    border-radius: 999px;
    padding: 6px 11px;
    font-size: 12px;
    color: var(--muted);
    display: inline-flex;
    align-items: center;
    justify-content: center;
    line-height: 1;
}
.chip.ok {
    color: var(--good);
}
.chip.warn {
    color: var(--bad);
}
.grid {
    display: grid;
    grid-template-columns: repeat(12, minmax(0, 1fr));
    gap: 14px;
}
.card {
    border: 1px solid var(--card-border);
    background: var(--card-bg);
    backdrop-filter: blur(16px);
    border-radius: 22px;
    padding: 16px;
    box-shadow: var(--card-shadow);
    animation: rise 360ms ease both;
}
.card h2 {
    margin: 0 0 14px;
    font-size: 16px;
    font-weight: 600;
}
.air-card {
    grid-column: span 6;
}
.fan-card {
    grid-column: span 6;
}
.mqtt-card {
    grid-column: span 8;
}
.sys-card {
    grid-column: span 4;
}
.pm-grid {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 10px;
}
.pm-box {
    border-radius: 16px;
    background: var(--pm-bg);
    border: 1px solid var(--pm-border);
    padding: 12px;
}
.pm-label {
    color: var(--muted);
    font-size: 12px;
}
.pm-value {
    margin-top: 6px;
    font-size: 28px;
    font-weight: 650;
    letter-spacing: 0.01em;
}
.quality {
    margin-top: 12px;
    display: inline-block;
    padding: 6px 10px;
    border-radius: 999px;
    font-size: 12px;
    background: var(--accent-soft);
    color: var(--accent);
}
.quality.good {
    background: rgba(31, 159, 103, 0.15);
    color: var(--good);
}
.quality.mid {
    background: rgba(216, 154, 49, 0.18);
    color: var(--mid);
}
.quality.bad {
    background: rgba(193, 74, 74, 0.16);
    color: var(--bad);
}
.row {
    display: flex;
    gap: 10px;
    align-items: center;
}
.row + .row {
    margin-top: 12px;
}
.row label {
    color: var(--muted);
    font-size: 13px;
}
.mqtt-card .row label {
    width: 118px;
    min-width: 118px;
    white-space: nowrap;
}
input[type="text"],
input[type="password"],
input[type="number"] {
    width: 100%;
    border: 1px solid var(--input-border);
    border-radius: 12px;
    background: var(--input-bg);
    padding: 10px 12px;
    color: var(--text);
    outline: none;
}
input[type="range"] {
    width: 100%;
}
.fan-value {
    min-width: 44px;
    text-align: right;
    color: var(--accent);
    font-weight: 600;
}
.btn-row {
    display: flex;
    gap: 8px;
    flex-wrap: wrap;
    margin-top: 12px;
}
button {
    border: 0;
    border-radius: 12px;
    padding: 9px 12px;
    font-weight: 560;
    color: var(--button-fg);
    background: var(--button-bg);
    border: 1px solid transparent;
    transition: transform 120ms ease, filter 120ms ease;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    line-height: 1;
}
button.primary {
    color: #fff;
    background: linear-gradient(135deg, #2e7bf4, #2d9ff4);
}
button.ghost {
    color: #2e7bf4;
    background: rgba(46, 123, 244, 0.12);
}
button.action {
    color: #fff;
    font-weight: 620;
    box-shadow: 0 8px 16px rgba(10, 18, 33, 0.2);
}
button.action:hover {
    filter: brightness(1.06);
}
button.action:active {
    transform: translateY(1px);
}
button.action-screen {
    background: linear-gradient(135deg, #2578f0, #399cf6);
}
button.action-reboot {
    background: linear-gradient(135deg, #d5851d, #e7a53c);
}
button.action-dfu {
    background: linear-gradient(135deg, #c84b4b, #de6666);
}
body[data-theme="dark"] button.action {
    box-shadow: 0 10px 18px rgba(0, 0, 0, 0.34);
}
.kv {
    display: grid;
    grid-template-columns: auto 1fr;
    gap: 6px 10px;
    font-size: 13px;
    color: var(--muted);
}
.kv b {
    color: var(--text);
    font-weight: 600;
}
.toast {
    position: fixed;
    left: 50%;
    bottom: 18px;
    transform: translateX(-50%);
    background: var(--toast-bg);
    color: var(--toast-fg);
    border-radius: 999px;
    padding: 10px 14px;
    font-size: 12px;
    opacity: 0;
    pointer-events: none;
    transition: opacity 180ms ease;
}
.toast.show {
    opacity: 1;
}
@keyframes rise {
    from {
        transform: translateY(8px);
        opacity: 0;
    }
    to {
        transform: translateY(0);
        opacity: 1;
    }
}
@media (max-width: 900px) {
    .air-card, .fan-card, .mqtt-card, .sys-card {
        grid-column: span 12;
    }
    .top {
        flex-direction: column;
        align-items: flex-start;
        gap: 10px;
    }
}
</style>
</head>
<body>
<div class="orb orb-a"></div>
<div class="orb orb-b"></div>
<main class="shell">
    <header class="top">
        <div class="title-wrap">
            <h1>Aeris Aair Purifier</h1>
            <p>Live air quality and controls</p>
        </div>
        <div class="status">
            <span class="chip" id="wifi-chip">Wi-Fi --</span>
            <span class="chip" id="mqtt-chip">MQTT --</span>
            <button class="ghost" id="theme-toggle" type="button">Dark Mode</button>
        </div>
    </header>

    <section class="grid">
        <article class="card air-card">
            <h2>Air Quality</h2>
            <div class="pm-grid">
                <div class="pm-box">
                    <div class="pm-label">PM2.5</div>
                    <div class="pm-value" id="pm25-value">--</div>
                </div>
                <div class="pm-box">
                    <div class="pm-label">PM10</div>
                    <div class="pm-value" id="pm10-value">--</div>
                </div>
            </div>
            <div class="quality" id="quality-chip">Waiting for sensor data</div>
        </article>

        <article class="card fan-card">
            <h2>Fan Control</h2>
            <div class="row">
                <label for="fan-slider">Fan Speed</label>
                <div style="flex:1">
                    <input type="range" id="fan-slider" min="0" max="100" value="0">
                </div>
                <span class="fan-value" id="fan-value">0%</span>
            </div>
            <div class="btn-row">
                <button class="ghost fan-preset" type="button" data-fan="0">0%</button>
                <button class="ghost fan-preset" type="button" data-fan="25">25%</button>
                <button class="ghost fan-preset" type="button" data-fan="50">50%</button>
                <button class="ghost fan-preset" type="button" data-fan="75">75%</button>
                <button class="ghost fan-preset" type="button" data-fan="100">100%</button>
            </div>
            <div class="btn-row">
                <button class="action action-screen" type="button" id="screen-toggle">Toggle Screen Light</button>
            </div>
        </article>

        <article class="card mqtt-card">
            <h2>Network and MQTT</h2>
            <form id="settings-form">
                <div class="row"><label>Wi-Fi SSID</label><input id="wifi-ssid" type="text" autocomplete="off"></div>
                <div class="row"><label>Wi-Fi Pass</label><input id="wifi-pass" type="password" autocomplete="off" placeholder="leave blank to keep"></div>
                <div class="row"><label>MQTT Enabled</label><input id="mqtt-enabled" type="checkbox"></div>
                <div class="row"><label>MQTT Host</label><input id="mqtt-host" type="text" autocomplete="off"></div>
                <div class="row"><label>MQTT Port</label><input id="mqtt-port" type="number" min="1" max="65535"></div>
                <div class="row"><label>MQTT User</label><input id="mqtt-user" type="text" autocomplete="off"></div>
                <div class="row"><label>MQTT Pass</label><input id="mqtt-pass" type="password" autocomplete="off" placeholder="leave blank to keep"></div>
                <div class="row"><label>MQTT Topic Root</label><input id="mqtt-topic-root" type="text" autocomplete="off"></div>
                <div class="row"><label>Device ID</label><input id="device-id" type="text" autocomplete="off"></div>
                <div class="btn-row">
                    <button class="primary" type="submit">Save Settings</button>
                </div>
            </form>
        </article>

        <article class="card sys-card">
            <h2>System</h2>
            <div class="kv">
                <span>Uptime</span><b id="uptime-value">--</b>
                <span>Sensor Errors</span><b id="sensor-errors">--</b>
                <span>Fan</span><b id="fan-state">--</b>
            </div>
            <div class="btn-row">
                <button class="action action-reboot" type="button" id="reboot-btn">Reboot</button>
                <button class="action action-dfu" type="button" id="dfu-btn">DFU</button>
            </div>
        </article>
    </section>
</main>
<div class="toast" id="toast"></div>
<script>
(function () {
    function byId(id) {
        return document.getElementById(id);
    }

    var stateCache = {
        fan_percent: 0,
        lights_on: false,
        screen_light_on: true,
        wifi_ready: false,
        mqtt_enabled: false,
        mqtt_connected: false
    };
    var toastTimer = null;
    var isFanDragging = false;
    var fanDebounceTimer = null;
    var themeKey = 'aeris_ui_theme';

    function showToast(message) {
        var el = byId('toast');
        if (!el) {
            return;
        }
        el.textContent = message;
        el.classList.add('show');
        if (toastTimer) {
            clearTimeout(toastTimer);
        }
        toastTimer = setTimeout(function () {
            el.classList.remove('show');
        }, 1800);
    }

    function getSavedTheme() {
        try {
            return localStorage.getItem(themeKey);
        } catch (err) {
            return null;
        }
    }

    function saveTheme(theme) {
        try {
            localStorage.setItem(themeKey, theme);
        } catch (err) {
        }
    }

    function resolveInitialTheme() {
        var saved = getSavedTheme();
        if (saved === 'light' || saved === 'dark') {
            return saved;
        }
        var prefersDark = window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches;
        return prefersDark ? 'dark' : 'light';
    }

    function applyTheme(theme) {
        var normalized = (theme === 'dark') ? 'dark' : 'light';
        document.body.setAttribute('data-theme', normalized);
        var themeBtn = byId('theme-toggle');
        if (themeBtn) {
            themeBtn.textContent = normalized === 'dark' ? 'Light Mode' : 'Dark Mode';
        }
    }

    function encodeForm(data) {
        var list = [];
        Object.keys(data).forEach(function (key) {
            if (data[key] === undefined || data[key] === null) {
                return;
            }
            list.push(encodeURIComponent(key) + '=' + encodeURIComponent(String(data[key])));
        });
        return list.join('&');
    }

    async function apiGet(path) {
        var res = await fetch(path, { cache: 'no-store' });
        if (!res.ok) {
            throw new Error('HTTP ' + res.status);
        }
        return res.json();
    }

    async function apiPost(path, data) {
        var res = await fetch(path, {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: encodeForm(data || {})
        });
        var payload = {};
        try {
            payload = await res.json();
        } catch (e) {
            payload = {};
        }
        if (!res.ok) {
            var msg = payload.error ? payload.error : ('HTTP ' + res.status);
            throw new Error(msg);
        }
        return payload;
    }

    function qualityForPm25(pm25) {
        if (pm25 <= 12) {
            return { text: 'Air quality: Good', cls: 'good' };
        }
        if (pm25 <= 35) {
            return { text: 'Air quality: Moderate', cls: 'mid' };
        }
        return { text: 'Air quality: High PM', cls: 'bad' };
    }

    function renderState(data) {
        stateCache = data;

        byId('pm25-value').textContent = data.pm25;
        byId('pm10-value').textContent = data.pm10;
        byId('uptime-value').textContent = data.uptime_s + 's';
        byId('sensor-errors').textContent = data.sensor_parse_errors;
        byId('fan-state').textContent = data.fan_percent + '%';

        if (!isFanDragging) {
            byId('fan-slider').value = data.fan_percent;
            byId('fan-value').textContent = data.fan_percent + '%';
        }

        var q = qualityForPm25(Number(data.pm25) || 0);
        var qChip = byId('quality-chip');
        qChip.textContent = q.text;
        qChip.className = 'quality ' + q.cls;

        var wifiChip = byId('wifi-chip');
        wifiChip.textContent = data.wifi_ready ? 'Wi-Fi online' : 'Wi-Fi offline';
        wifiChip.className = data.wifi_ready ? 'chip ok' : 'chip warn';

        var mqttChip = byId('mqtt-chip');
        if (!data.mqtt_enabled) {
            mqttChip.textContent = 'MQTT disabled';
            mqttChip.className = 'chip';
        } else {
            mqttChip.textContent = data.mqtt_connected ? 'MQTT online' : 'MQTT offline';
            mqttChip.className = data.mqtt_connected ? 'chip ok' : 'chip warn';
        }

        var screenBtn = byId('screen-toggle');
        if (screenBtn) {
            screenBtn.textContent = data.screen_light_on ? 'Toggle Screen Light (On)' : 'Toggle Screen Light (Off)';
        }
    }

    async function loadState() {
        try {
            var data = await apiGet('/api/v2/state');
            renderState(data);
        } catch (err) {
            showToast('State refresh failed');
        }
    }

    async function loadSettings() {
        try {
            var s = await apiGet('/api/v2/settings');
            byId('wifi-ssid').value = s.wifi_ssid || '';
            byId('mqtt-enabled').checked = !!Number(s.mqtt_enabled || 0);
            byId('mqtt-host').value = s.mqtt_host || '';
            byId('mqtt-port').value = s.mqtt_port || 1883;
            byId('mqtt-user').value = s.mqtt_user || '';
            byId('device-id').value = s.device_id || '';
            byId('mqtt-topic-root').value = s.mqtt_topic_root || '';
        } catch (err) {
            showToast('Settings fetch failed');
        }
    }

    async function sendControl(payload) {
        await apiPost('/api/v2/control', payload);
        loadState();
    }

    byId('settings-form').addEventListener('submit', async function (evt) {
        evt.preventDefault();
        try {
            var payload = {
                wifi_ssid: byId('wifi-ssid').value.trim(),
                mqtt_enabled: byId('mqtt-enabled').checked ? 1 : 0,
                mqtt_host: byId('mqtt-host').value.trim(),
                mqtt_port: byId('mqtt-port').value.trim(),
                mqtt_user: byId('mqtt-user').value.trim(),
                device_id: byId('device-id').value.trim(),
                mqtt_topic_root: byId('mqtt-topic-root').value.trim()
            };
            var wifiPass = byId('wifi-pass').value;
            var mqttPass = byId('mqtt-pass').value;
            if (wifiPass.length > 0) {
                payload.wifi_pass = wifiPass;
            }
            if (mqttPass.length > 0) {
                payload.mqtt_pass = mqttPass;
            }
            await apiPost('/api/v2/settings', payload);
            byId('wifi-pass').value = '';
            byId('mqtt-pass').value = '';
            showToast('Settings saved');
        } catch (err) {
            showToast('Save failed: ' + err.message);
        }
    });

    function queueFanUpdate(value) {
        if (fanDebounceTimer) {
            clearTimeout(fanDebounceTimer);
        }
        fanDebounceTimer = setTimeout(async function () {
            try {
                await sendControl({ fan_percent: value });
            } catch (err) {
                showToast('Fan update failed');
            }
        }, 160);
    }

    var fanSlider = byId('fan-slider');
    fanSlider.addEventListener('pointerdown', function () {
        isFanDragging = true;
    });
    fanSlider.addEventListener('pointerup', function () {
        isFanDragging = false;
    });
    fanSlider.addEventListener('touchstart', function () {
        isFanDragging = true;
    }, { passive: true });
    fanSlider.addEventListener('touchend', function () {
        isFanDragging = false;
    }, { passive: true });
    fanSlider.addEventListener('change', function () {
        isFanDragging = false;
    });
    fanSlider.addEventListener('input', function (evt) {
        var value = evt.target.value;
        byId('fan-value').textContent = value + '%';
        queueFanUpdate(value);
    });

    Array.prototype.forEach.call(document.querySelectorAll('.fan-preset'), function (el) {
        el.addEventListener('click', async function () {
            var value = el.getAttribute('data-fan');
            byId('fan-slider').value = value;
            byId('fan-value').textContent = value + '%';
            try {
                await sendControl({ fan_percent: value });
                showToast('Fan ' + value + '%');
            } catch (err) {
                showToast('Preset failed');
            }
        });
    });

    byId('screen-toggle').addEventListener('click', async function () {
        var next = stateCache.screen_light_on ? 0 : 1;
        try {
            await sendControl({ screen_light: next });
            showToast(next ? 'Screen light on' : 'Screen light off');
        } catch (err) {
            showToast('Screen update failed');
        }
    });

    byId('theme-toggle').addEventListener('click', function () {
        var current = document.body.getAttribute('data-theme') || 'light';
        var next = current === 'dark' ? 'light' : 'dark';
        applyTheme(next);
        saveTheme(next);
    });

    byId('reboot-btn').addEventListener('click', async function () {
        try {
            await apiPost('/api/v2/system/reboot', {});
            showToast('Reboot requested');
        } catch (err) {
            showToast('Reboot failed');
        }
    });

    byId('dfu-btn').addEventListener('click', async function () {
        try {
            await apiPost('/api/v2/system/dfu', {});
            showToast('DFU requested');
        } catch (err) {
            showToast('DFU failed');
        }
    });

    applyTheme(resolveInitialTheme());
    loadSettings();
    loadState();
    setInterval(loadState, 2000);
})();
</script>
</body>
</html>)HTML";
}  // namespace

const char* webUiIndexHtml() {
    return kIndexHtml;
}

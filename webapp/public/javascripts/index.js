const MAX_ROOM_NUMBER = 9;
const MIN_TEMPERATURE_THRESHOLD = -15.0;
const MAX_TEMPERATURE_THRESHOLD = 50.0;
const MIN_SIGNAL_STRENGTH_THRESHOLD = 0;
const MAX_SIGNAL_STRENGTH_THRESHOLD = -100;

const loginContainer = document.getElementById("login_container");
const mainContainer = document.getElementById("main_container");
const lastTemperature = document.getElementById("last_temperature");
const lastHumidity = document.getElementById("last_humidity");
const lastSignalStrength = document.getElementById("last_signal_strength");
const roomNumber = document.getElementById("room_number");
const newRoomNumber = document.getElementById("new_room_number");
const deviceStatus = document.getElementById("status");
const resumeButton = document.getElementById("resume_button");
const pauseButton = document.getElementById("pause_button");
const lowTemperature = document.getElementById("low_temperature");
const highTemperature = document.getElementById("high_temperature");
const signalStrength = document.getElementById("signal_strength");
const temperatureWarning = document.getElementById("temperature_warning");
const signalStrengthWarning = document.getElementById("signal_strength_warning");
const powerSavingMode = document.getElementById("power_saving_mode");
const awayButton = document.getElementById("away_button");
const nightModeStartHour = document.getElementById("night_mode_start_hour");
const nightModeStartMinute = document.getElementById("night_mode_start_minute");
const nightModeEndHour = document.getElementById("night_mode_end_hour");
const nightModeEndMinute = document.getElementById("night_mode_end_minute");
const nightModeNotSet = document.getElementById("night_mode_not_set");
document.getElementById("login_button").addEventListener("click", login);
document.getElementById("update_room").addEventListener("click", publishRoomNumber);
document.getElementById("update_thresholds").addEventListener("click", publishThresholds);
document.getElementById("update_night_mode").addEventListener("click", publishNightModeConfig);
resumeButton.addEventListener("click", resumeDevice);
pauseButton.addEventListener("click", pauseDevice);
awayButton.addEventListener("click", setAwayMode);
newRoomNumber.max = MAX_ROOM_NUMBER;
lowTemperature.min = MIN_TEMPERATURE_THRESHOLD;
lowTemperature.max = MAX_TEMPERATURE_THRESHOLD;
highTemperature.min = MIN_TEMPERATURE_THRESHOLD;
highTemperature.max = MAX_TEMPERATURE_THRESHOLD;
signalStrength.min = MAX_SIGNAL_STRENGTH_THRESHOLD;
signalStrength.max = MIN_SIGNAL_STRENGTH_THRESHOLD;

let currentRoom = -1;
let devices = ["", "", "", "", "", "", "", "", ""];
let minTemperatureThreshold = [0, 0, 0, 0, 0, 0, 0, 0, 0];
let maxTemperatureThreshold = [0, 0, 0, 0, 0, 0, 0, 0, 0];
let signalStrengthThreshold = [0, 0, 0, 0, 0, 0, 0, 0, 0];
let lastTemperatureValues = [null, null, null, null, null, null, null, null, null];
let lastHumidityValues = [null, null, null, null, null, null, null, null, null];
let lastSignalStrengthValues = [null, null, null, null, null, null, null, null, null];
let powerSavingModes = ["none", "none", "none", "none", "none", "none", "none", "none", "none"];
let nightModeConfigs = Array.from({ length: 10 }, () => ({
    startHour: -1,
    startMinute: -1,
    endHour: -1,
    endMinute: -1,
}));

const roomButtons = document.querySelectorAll(".room");
for (let room of roomButtons) {
    room.addEventListener("click", () => {
        changeRoom(room);
    });
}
changeRoom(roomButtons[0]);

function login() {

    let clientId = document.getElementById("client_id").value;
    let username = document.getElementById("username").value;
    let password = document.getElementById("password").value;

    mqttConnect(clientId, username, password, () => {
        loginContainer.style.display = "none";
        mainContainer.style.display = "block";
        checkConnectedDevices();
    });
}

function checkConnectedDevices() {

    fetch("/devices").then(res => {
        res.json().then(devices => {
            devices.forEach(device => {
                mqttSubscribeDevice(device);
            })
        })
    }).catch(err => {
        console.error(err)
    });
}

function onDeviceActiveReceived(isActive, deviceId) {

    if (devices[currentRoom] !== deviceId)
        return;

    if (isActive === "true") {
        deviceStatus.innerText = "active";
        pauseButton.style.display = "block";
        resumeButton.style.display = "none";
    } else {
        deviceStatus.innerText = "paused";
        resumeButton.style.display = "block";
        pauseButton.style.display = "none";
    }
}

function onDataReceived(data, type, deviceId) {

    let room = devices.indexOf(deviceId);
    let newValue = Number(data);
    switch (type) {
        case "temperature":
            lastTemperatureValues[room] = newValue;
            if (room === currentRoom) {
                lastTemperature.innerText = data;
                if (newValue < minTemperatureThreshold[room] || newValue > maxTemperatureThreshold[room])
                    temperatureWarning.style.display = "block";
                else
                    temperatureWarning.style.display = "none";
            }
            break;
        case "humidity":
            lastHumidityValues[room] = newValue;
            if (room === currentRoom)
                lastHumidity.innerText = data;
            break;
        case "signal_strength":
            lastSignalStrengthValues[room] = newValue;
            if (room === currentRoom) {
                lastSignalStrength.innerText = data;
                if (newValue < signalStrengthThreshold[room])
                    signalStrengthWarning.style.display = "block";
                else
                    signalStrengthWarning.style.display = "none";
            }
            break;
        default:
            console.error("Unknown type: " + type);
    }
}

function onThresholdReceived(data, type, deviceId) {

    let room = devices.indexOf(deviceId);
    switch (type) {
        case "min_temperature":
            minTemperatureThreshold[room] = data;
            if (room === currentRoom)
                lowTemperature.value = data;
            break;
        case "max_temperature":
            maxTemperatureThreshold[room] = data;
            if (room === currentRoom)
                highTemperature.value = data;
            break;
        case "signal_strength":
            signalStrengthThreshold[room] = data;
            if (room === currentRoom)
                signalStrength.value = data;
            break;
        default:
            console.error("Unknown type: " + type);
    }
}

function onRoomUpdated(newRoom, deviceId) {

    let previousRoom = devices.indexOf(deviceId);
    if (previousRoom !== -1) {
        devices[previousRoom] = "";
        minTemperatureThreshold[newRoom - 1] = minTemperatureThreshold[previousRoom];
        maxTemperatureThreshold[newRoom - 1] = maxTemperatureThreshold[previousRoom];
        signalStrengthThreshold[newRoom - 1] = signalStrengthThreshold[previousRoom];
        nightModeConfigs[newRoom - 1].startHour = nightModeConfigs[currentRoom].startHour;
        nightModeConfigs[newRoom - 1].startMinute = nightModeConfigs[currentRoom].startMinute;
        nightModeConfigs[newRoom - 1].endHour = nightModeConfigs[currentRoom].endHour;
        nightModeConfigs[newRoom - 1].endMinute = nightModeConfigs[currentRoom].endMinute;
        if (previousRoom === currentRoom && Number(newRoom) - 1 !== previousRoom)
            deviceStatus.innerText = "not connected";
    }
    devices[newRoom - 1] = deviceId;
}

function changeRoom(room) {

    room.classList.remove("room");
    room.classList.remove("room-alarm");
    if (currentRoom !== -1) {
        let button = roomButtons[currentRoom];
        button.classList.add("room");
    }
    let newRoom = room.innerText;
    currentRoom = Number(newRoom - 1);
    roomNumber.innerText = newRoom;

    let status;
    if (devices[currentRoom])
        status = "active";
    else
        status = "not connected";
    deviceStatus.innerText = status;

    if (lastTemperatureValues[currentRoom] === null) {
        lastTemperature.innerText = "--";
        temperatureWarning.style.display = "none";
    } else {
        let value = lastTemperatureValues[currentRoom];
        lastTemperature.innerText = value;
        if (value < minTemperatureThreshold[room] || value > maxTemperatureThreshold[room])
            temperatureWarning.style.display = "block";
        else
            temperatureWarning.style.display = "none";
    }

    if (lastHumidityValues[currentRoom] === null)
        lastHumidity.innerText = "--";
    else
        lastHumidity.innerText = lastHumidityValues[currentRoom];

    if (lastSignalStrengthValues[currentRoom] === null) {
        lastSignalStrength.innerText = "--";
        signalStrengthWarning.style.display = "none";
    } else {
        let value = lastSignalStrengthValues[currentRoom];
        lastSignalStrength.innerText = value
        if (value < signalStrengthThreshold[room])
            signalStrengthWarning.style.display = "block";
        else
            signalStrengthWarning.style.display = "none";
    }

    newRoomNumber.value = newRoom;
    lowTemperature.value = minTemperatureThreshold[currentRoom];
    highTemperature.value = maxTemperatureThreshold[currentRoom];
    signalStrength.value = signalStrengthThreshold[currentRoom];
    powerSavingMode.innerText = powerSavingModes[currentRoom];

    awayButton.disabled = powerSavingModes[currentRoom] !== "none";

    nightModeStartHour.value = nightModeConfigs[currentRoom].startHour;
    nightModeStartMinute.value = nightModeConfigs[currentRoom].startMinute;
    nightModeEndHour.value = nightModeConfigs[currentRoom].endHour;
    nightModeEndMinute.value = nightModeConfigs[currentRoom].endMinute;

    if (nightModeConfigs[currentRoom].startHour === -1 || nightModeConfigs[currentRoom].startMinute === -1 ||
        nightModeConfigs[currentRoom].endHour === -1 || nightModeConfigs[currentRoom].endMinute === -1)
        nightModeNotSet.style.display = "block";
    else
        nightModeNotSet.style.display = "none";
}

function onDeviceDisconnected(deviceId) {

    let room = devices.indexOf(deviceId);
    devices[room] = "";

    if (room === currentRoom)
        deviceStatus.innerText = "not connected";
}

function onAlarmReceived(deviceId) {

    let room = devices.indexOf(deviceId);
    if (room === currentRoom || room === -1)
        return;

    let roomButton = roomButtons[room];
    if (roomButton.classList.contains("room-alarm"))
        return;

    roomButton.classList.add("room-alarm");
    roomButton.classList.remove("room");
}

function onPowerSavingModeUpdated(mode, deviceId) {

    let room = devices.indexOf(deviceId);
    powerSavingModes[room] = mode;

    if (room !== currentRoom)
        return;

    powerSavingMode.innerText = mode;
    awayButton.disabled = mode !== "none";
}

function onNightModeConfigUpdate(config, deviceId) {

    let room = devices.indexOf(deviceId);
    let newConfig = JSON.parse(config);

    let localTimeConfig;
    if (newConfig.startHour === -1 || newConfig.startMinute === -1 || newConfig.endHour === -1 || newConfig.endMinute === -1) {
        nightModeNotSet.style.display = "block";
        localTimeConfig = newConfig;
    }
    else {
        nightModeNotSet.style.display = "none";
        let startTime = new Date();
        startTime.setUTCHours(newConfig.startHour, newConfig.startMinute, 0, 0);
        let endTime = new Date();
        endTime.setUTCHours(newConfig.endHour, newConfig.endMinute, 0, 0);
        localTimeConfig = {
            "startHour": startTime.getHours(),
            "startMinute": startTime.getMinutes(),
            "endHour": endTime.getHours(),
            "endMinute": endTime.getMinutes()
        }
    }

    nightModeConfigs[room] = localTimeConfig;

    if (room !== currentRoom)
        return;

    nightModeStartHour.value = localTimeConfig.startHour;
    nightModeStartMinute.value = localTimeConfig.startMinute;
    nightModeEndHour.value = localTimeConfig.endHour;
    nightModeEndMinute.value = localTimeConfig.endMinute;
}

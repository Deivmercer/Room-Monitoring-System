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
document.getElementById("login_button").addEventListener("click", login);
document.getElementById("update_room").addEventListener("click", publishRoomNumber);
document.getElementById("update_thresholds").addEventListener("click", publishThresholds);
resumeButton.addEventListener("click", resumeDevice);
pauseButton.addEventListener("click", pauseDevice);
newRoomNumber.max = MAX_ROOM_NUMBER;
lowTemperature.min = MIN_TEMPERATURE_THRESHOLD;
lowTemperature.max = MAX_TEMPERATURE_THRESHOLD;
highTemperature.min = MIN_TEMPERATURE_THRESHOLD;
highTemperature.max = MAX_TEMPERATURE_THRESHOLD;
signalStrength.min = MAX_SIGNAL_STRENGTH_THRESHOLD;
signalStrength.max = MIN_SIGNAL_STRENGTH_THRESHOLD;

let deviceId = "";
let minTemperatureThreshold = 0;
let maxTemperatureThreshold = 0;
let signalStrengthThreshold = 0;

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
                deviceId = device;
                deviceStatus.innerText = "active";
                mqttSubscribeDevice(device);
            })
        })
    }).catch(err => {
        console.log(err)
    });
}

function onDeviceActiveReceived(data) {

    if (data.toString() === "true") {
        deviceStatus.innerText = "active";
        pauseButton.style.display = "block";
        resumeButton.style.display = "none";
    } else {
        deviceStatus.innerText = "paused";
        resumeButton.style.display = "block";
        pauseButton.style.display = "none";
    }
}

function onDataReceived(data, type) {

    switch (type) {
        case "temperature":
            lastTemperature.innerText = data;
            if (data < minTemperatureThreshold || data > maxTemperatureThreshold)
                temperatureWarning.style.display = "block";
            else
                temperatureWarning.style.display = "none";
            break;
        case "humidity":
            lastHumidity.innerText = data;
            break;
        case "signal":
            lastSignalStrength.innerText = data;
            if (data > signalStrengthThreshold)
                signalStrengthWarning.style.display = "block";
            else
                signalStrengthWarning.style.display = "none";
            break;
        default:
            console.error("Unknown type: " + type);
    }
}

function onThresholdReceived(data, type) {

    switch (type) {
        case "min_temperature":
            minTemperatureThreshold = data;
            lowTemperature.value = data;
            break;
        case "max_temperature":
            maxTemperatureThreshold = data;
            highTemperature.value = data;
            break;
        case "signal_strength":
            signalStrengthThreshold = data;
            signalStrength.value = data;
            break;
        default:
            console.error("Unknown type: " + type);
    }
}

function onRoomUpdated(newRoom) {

    roomNumber.innerText = newRoom;
    newRoomNumber.value = newRoom;
}

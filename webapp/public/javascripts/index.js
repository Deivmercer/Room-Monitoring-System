const MAX_ROOM_NUMBER = 9;

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
document.getElementById("login_button").addEventListener("click", login);
document.getElementById("update_room").addEventListener("click", publishRoomNumber);
resumeButton.addEventListener("click", resumeDevice);
pauseButton.addEventListener("click", pauseDevice);
newRoomNumber.max = MAX_ROOM_NUMBER;

let deviceId = "";

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
            break;
        case "humidity":
            lastHumidity.innerText = data;
            break;
        case "signal":
            lastSignalStrength.innerText = data;
            break;
        default:
            console.error("Unknown type: " + type);
    }
}

function onRoomUpdated(newRoom) {

    roomNumber.innerText = newRoom;
    newRoomNumber.value = newRoom;
}

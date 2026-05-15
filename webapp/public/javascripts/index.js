const MAX_ROOM_NUMBER = 9;

const loginContainer = document.getElementById("login_container");
const mainContainer = document.getElementById("main_container");
const lastTemperature = document.getElementById("last_temperature");
const lastHumidity = document.getElementById("last_humidity");
const lastSignalStrength = document.getElementById("last_signal_strength");
const roomNumber = document.getElementById("room_number");
const newRoomNumber = document.getElementById("new_room_number");
document.getElementById("login_button").addEventListener("click", login);
document.getElementById("update_room").addEventListener("click", publishRoomNumber);
newRoomNumber.max = MAX_ROOM_NUMBER;

let deviceId;

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
                mqttSubscribeDevice(device);
            })
        })
    }).catch(err => {
        console.log(err)
    });
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
}

function publishRoomNumber() {

    client.publish(`rms/${deviceId}/room/update`, newRoomNumber.value)
}

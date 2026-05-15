const loginContainer = document.getElementById("login_container");
const mainContainer = document.getElementById("main_container");
const lastTemperature = document.getElementById("last_temperature");
const lastHumidity = document.getElementById("last_humidity");
const lastSignalStrength = document.getElementById("last_signal_strength");
document.getElementById("login_button").addEventListener("click", login);

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
        console.log(err)
    });
}

function onDataReceived(data) {

function onDataReceived(data, type) {

    console.log("type: " + type);
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

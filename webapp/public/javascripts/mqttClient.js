const MQTT_BROKER_URL = "ws://davemercer.moe:9001"; // Browsers only support websockets
const MQTT_TOPIC_DEVICE = "rms/devices"
const MQTT_TOPIC_ALARMS = "rms/+/alarms/#"
const MQTT_SUBTOPIC_SENSORS = "/sensors/#"
const MQTT_SUBTOPIC_ROOM = "/room"
const MQTT_SUBTOPIC_ACTIVE = "/active"
const MQTT_SUBTOPIC_THRESHOLDS = "/thresholds/+"
const MQTT_SUBTOPIC_THRESHOLDS_ACKS = "/thresholds/+/update/ack"
const MQTT_SUBTOPIC_ROOM_ACKS = "/room/update/ack"
const MQTT_SUBTOPIC_POWER_SAVING_MODES = "/power_saving_mode"
const MQTT_SUBTOPIC_NIGHT_MODE_CONFIG = "/night_mode_config"
let client;

function mqttConnect(clientId, username, password, callback) {

    const options = {
        clean: true,
        connectTimeout: 4000,
        clientId: clientId,
        username: username,
        password: password,
    }
    client = mqtt.connect(MQTT_BROKER_URL, options);

    client.on("connect", () => {
        callback();
        subscribeTopic(MQTT_TOPIC_DEVICE);
        subscribeTopic(MQTT_TOPIC_ALARMS);
    });

    client.on("message", (topic, message) => {
        if (topic === MQTT_TOPIC_DEVICE) {
            let json = JSON.parse(message);
            if (json.status === "connected")
                mqttSubscribeDevice(json.mac_address);
            else if (json.status === "disconnected") {
                let topic = `rms/${json.mac_address}/sensors`;
                client.unsubscribe(topic, (err) => {
                    if (err)
                        console.error(err);
                    else {
                        console.log(`Unsubscribed from ${topic}`);
                        onDeviceDisconnected(json.mac_address);
                    }
                });
            }

            return;
        }

        let deviceId = topic.split("/")[1];
        if (topic.includes("/sensors/")) {
            let type = topic.split("/sensors/")[1];
            onDataReceived(message, type, deviceId);
        } else if (topic.endsWith("/ack")) {
            let json = JSON.parse(message);
            if (json.result === "error")
                alert("Error: " + json.error);
        } else if (topic.includes("/thresholds/")) {
            let type = topic.split("/thresholds/")[1];
            onThresholdReceived(message, type, deviceId);
        } else if (topic.endsWith(MQTT_SUBTOPIC_ROOM))
            onRoomUpdated(message, deviceId)
        else if (topic.endsWith(MQTT_SUBTOPIC_ACTIVE))
            onDeviceActiveReceived(message.toString(), deviceId);
        else if (topic.includes("/alarms/"))
            onAlarmReceived(deviceId);
        else if (topic.endsWith(MQTT_SUBTOPIC_POWER_SAVING_MODES))
            onPowerSavingModeUpdated(message.toString(), deviceId);
        else if (topic.endsWith(MQTT_SUBTOPIC_NIGHT_MODE_CONFIG))
            onNightModeConfigUpdate(message, deviceId);
        else
            console.log("Unknown topic: " + topic);
    });
}

function mqttSubscribeDevice(device) {

    let mainTopic = `rms/${device}`;
    subscribeTopic(mainTopic);
    subscribeTopic(mainTopic + MQTT_SUBTOPIC_SENSORS);
    subscribeTopic(mainTopic + MQTT_SUBTOPIC_ROOM);
    subscribeTopic(mainTopic + MQTT_SUBTOPIC_ACTIVE);
    subscribeTopic(mainTopic + MQTT_SUBTOPIC_THRESHOLDS);
    subscribeTopic(mainTopic + MQTT_SUBTOPIC_THRESHOLDS_ACKS);
    subscribeTopic(mainTopic + MQTT_SUBTOPIC_ROOM_ACKS);
    subscribeTopic(mainTopic + MQTT_SUBTOPIC_POWER_SAVING_MODES);
    subscribeTopic(mainTopic + MQTT_SUBTOPIC_NIGHT_MODE_CONFIG);
}

function subscribeTopic(topic) {

    client.subscribe(topic, (err) => {
        if (err)
            console.error(err);
        else
            console.log(`Subscribed to ${topic}`);
    });
}

function publishRoomNumber() {

    let deviceId = devices[currentRoom];
    client.publish(`rms/${deviceId}/room/update`, newRoomNumber.value);
}

function resumeDevice() {

    let deviceId = devices[currentRoom];
    client.publish(`rms/${deviceId}/active/update`, "true");
}

function pauseDevice() {

    let deviceId = devices[currentRoom];
    client.publish(`rms/${deviceId}/active/update`, "false");
}

function publishThresholds() {

    let deviceId = devices[currentRoom];
    client.publish(`rms/${deviceId}/thresholds/max_temperature/update`, highTemperature.value);
    client.publish(`rms/${deviceId}/thresholds/min_temperature/update`, lowTemperature.value);
    client.publish(`rms/${deviceId}/thresholds/signal_strength/update`, signalStrength.value);
}

function setAwayMode() {

    let deviceId = devices[currentRoom];
    client.publish(`rms/${deviceId}${MQTT_SUBTOPIC_POWER_SAVING_MODES}/update`, "away");
    awayButton.disabled = true;
}

function publishNightModeConfig() {

    if (nightModeStartHour.value < 0 || nightModeStartHour.value > 23 ||
        nightModeStartMinute.value < 0 || nightModeStartMinute.value > 59 ||
        nightModeEndHour.value < 0 || nightModeEndHour.value > 23 ||
        nightModeEndMinute.value < 0 || nightModeEndMinute.value > 59) {
        alert("Invalid night mode configuration")
        return;
    }

    let deviceId = devices[currentRoom];
    let date = new Date();

    date.setHours(nightModeStartHour.value, nightModeStartMinute.value, 0, 0);
    let startHour = date.getUTCHours();
    let startMinute = date.getUTCMinutes();

    date.setHours(nightModeEndHour.value, nightModeEndMinute.value, 0, 0);
    let endHour = date.getUTCHours();
    let endMinute = date.getUTCMinutes();

    client.publish(`rms/${deviceId}${MQTT_SUBTOPIC_NIGHT_MODE_CONFIG}/update`, JSON.stringify({
        "startHour": startHour,
        "startMinute": startMinute,
        "endHour": endHour,
        "endMinute": endMinute
    }));
}

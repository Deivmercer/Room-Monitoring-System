const MQTT_BROKER_URL = "ws://davemercer.moe:9001"; // Browsers only support websockets
const MQTT_TOPIC_DEVICE = "rms/devices"
const MQTT_SUBTOPIC_SENSORS = "/sensors/#"
const MQTT_SUBTOPIC_ROOM = "/room"
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
                        console.log(err);
                    else
                        console.log(`Unsubscribed from ${topic}`);
                });
            }
        } else if (topic.includes("/sensors/")) {
            let type = topic.split("/sensors/")[1];
            onDataReceived(message, type);
        } else if (topic.endsWith("/room"))
            onRoomUpdated(message)
    });
}

function mqttSubscribeDevice(device) {

    let mainTopic = `rms/${device}`;
    subscribeTopic(mainTopic);
    subscribeTopic(mainTopic + MQTT_SUBTOPIC_SENSORS);
    subscribeTopic(mainTopic + MQTT_SUBTOPIC_ROOM);
}

function subscribeTopic(topic) {

    client.subscribe(topic, (err) => {
        if (err)
            console.log(err);
        else
            console.log(`Subscribed to ${topic}`);
    });
}

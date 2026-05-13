const MQTT_BROKER_URL = "ws://davemercer.moe:9001"; // Browsers only support websockets
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
        client.subscribe("rms/devices", (err) => {
            if (err)
                console.log(err);
            else
                console.log("Subscribed to rms/devices");
        })
    });

    client.on("message", (topic, message) => {
        let json = JSON.parse(message);
        if (topic === "rms/devices") {
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
        } else if (topic.endsWith("/sensors"))
            onDataReceived(json);
        else
            console.log(`Received message ${message.toString()} from topic ${topic}`);
    });
}

function mqttSubscribeDevice(device) {

    let topic = `rms/${device}/sensors`;
    client.subscribe(topic, (err) => {
        if (err)
            console.log(err);
        else
            console.log(`Subscribed to ${topic}`);
    });
}

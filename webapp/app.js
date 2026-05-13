const express = require("express");
const mqtt = require("mqtt");
const app = express();
const devices = new Set();

const port = process.env.PORT || "3000";
app.listen(port, () => {
    console.log(`Listening on port ${port}`)
})

app.get("/devices", (req, res) => {
    res.send([...devices]);
});

const options = {
    clean: true,
    connectTimeout: 4000,
    clientId: process.env.MQTT_CLIENT_ID,
    username: process.env.MQTT_USERNAME,
    password: process.env.MQTT_PASSWORD,
}
const client = mqtt.connect(process.env.MQTT_BROKER_URL, options);

client.on("connect", () => {
    client.subscribe("rms/devices", (err) => {
        if (err)
            console.log(err);
        else
            console.log("Subscribed to rms/devices");
    })
});

client.on("message", (topic, message) => {

    if (topic !== "rms/devices")
        return;

    let json = JSON.parse(message);
    let deviceTopic = `rms/${json.mac_address}/sensors`;
    if (json.status === "connected") {
        client.subscribe(deviceTopic, (err) => {
            if (err)
                console.log(err);
            else {
                console.log(`Subscribed to ${deviceTopic}`);
                devices.add(json.mac_address);
            }
        });
    } else if (json.status === "disconnected") {
        client.unsubscribe(deviceTopic, (err) => {
            if (err)
                console.log(err);
            else {
                console.log(`Unsubscribed from ${deviceTopic}`);
                devices.delete(json.mac_address);
            }
        });
    }
});

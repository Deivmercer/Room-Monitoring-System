const express = require("express");
const path = require("path");
const indexRouter = require("./routes/index");
const mqtt = require("mqtt");
const app = express();
const devices = new Set();

app.use(express.static(path.join(__dirname, "public")));

const port = process.env.PORT || "3000";
app.listen(port, () => {
    console.log(`Listening on port ${port}`)
})

app.use("/", indexRouter);

app.get("/devices", (req, res) => {
    res.send([...devices]);
});

app.use((req, res, next) => {
    res.redirect("/");
})

const options = {
    clean: true,
    connectTimeout: 4000,
    clientId: process.env.MQTT_CLIENT_ID,
    username: process.env.MQTT_USERNAME,
    password: process.env.MQTT_PASSWORD,
}
const client = mqtt.connect(process.env.MQTT_BROKER_URL, options);

client.on("connect", () => {
    subscribeTopic("rms/devices");
});

client.on("message", (topic, message) => {

    console.log(`Received message ${message} from topic ${topic}`);
    if (topic === "rms/devices")
        handleMqttDevice(message);
    else if (topic.endsWith("/room"))
        restoreDeviceRoom(topic, message);
    else if (topic.includes("/thresholds/"))
        restoreThresholds(topic, message);
});

function subscribeTopic(topic) {

    client.subscribe(topic, (err) => {
        if (err)
            console.error(err);
        else
            console.log(`Subscribed to ${topic}`);
    });
}

function handleMqttDevice(message) {

    let json = JSON.parse(message);
    let deviceTopic = `rms/${json.mac_address}/sensors`;
    if (json.status === "connected") {
        client.subscribe(deviceTopic, (err) => {
            if (err)
                console.error(err);
            else {
                console.log(`Subscribed to ${deviceTopic}`);
                devices.add(json.mac_address);
            }
        });
        subscribeTopic(`rms/${json.mac_address}/room`);
        subscribeTopic(`rms/${json.mac_address}/thresholds/+`);
    } else if (json.status === "disconnected") {
        client.unsubscribe(deviceTopic, (err) => {
            if (err)
                console.error(err);
            else {
                console.log(`Unsubscribed from ${deviceTopic}`);
                devices.delete(json.mac_address);
            }
        });
    }
}

function restoreDeviceRoom(topic, message) {

    let newTopic = topic + "/update";
    client.unsubscribe(topic, (err) => {
        if (err)
            console.error(err);
        else {
            console.log(`Unsubscribed from ${topic}`);
            client.publish(newTopic, message)
        }
    });
}

function restoreThresholds(topic, message) {

    let newTopic = topic + "/update";
    let previousTopic = topic.substring(0, topic.lastIndexOf("/"));
    client.unsubscribe(previousTopic + "/+", (err) => {
        if (err)
            console.error(err);
        else {
            console.log(`Unsubscribed from ${topic}`);
            client.publish(newTopic, message)
        }
    });
}


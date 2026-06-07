const express = require("express");
const path = require("path");
const mqtt = require("mqtt");
const TelegramBot = require("node-telegram-bot-api");
const indexRouter = require("./routes/index");

const TEMPERATURE_THRESHOLD_MIN = -15.0
const TEMPERATURE_THRESHOLD_MAX = 50.0
const SIGNAL_STRENGTH_THRESHOLD_MIN = 0
const SIGNAL_STRENGTH_THRESHOLD_MAX = -100
const MAX_ROOMS = 9

const app = express();
const telegramBot = new TelegramBot(process.env.TELEGRAM_BOT_TOKEN, { polling: true, onlyFirstMatch: true });
const devices = {};

app.use(express.static(path.join(__dirname, "public")));

const port = process.env.PORT || "3000";
app.listen(port, () => {
    console.log(`Listening on port ${port}`)
})

app.use("/", indexRouter);

app.get("/devices", (req, res) => {
    let ids = [];
    for (let device of Object.keys(devices))
        ids.push(device);
    res.send(ids);
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
    subscribeTopic(`rms/+/alarms/#`);
});

client.on("message", (topic, message) => {

    let restoreValue = false;
    console.log(`Received message ${message} from topic ${topic}`);
    let id = topic.split("rms/")[1].split("/")[0];
    if (topic === "rms/devices")
        handleMqttDevice(message);
    else if (topic.includes("/sensors/")) {
        let sensor = topic.split("/sensors/")[1];
        switch (sensor) {
            case "temperature":
                devices[id].sensors.temperature = message;
                break;
            case "humidity":
                devices[id].sensors.humidity = message;
                break;
            case "signal_strength":
                devices[id].sensors.signal_strength = message;
                break;
            default:
                console.error(`Unknown sensor ${sensor} received in topic ${topic}`);
        }
    } else if (topic.endsWith("/active")) {
        devices[id].is_active = message.toString() === "true";
    } else if (topic.endsWith("/room")) {
        if (!devices[id].room)
            restoreValue = true;
        devices[id].room = message;
    } else if (topic.includes("/thresholds/")) {
        let threshold = topic.split("/thresholds/")[1].split("/")[0];
        switch (threshold) {
            case "min_temperature":
                if (!devices[id].thresholds.min_temperature)
                    restoreValue = true;
                devices[id].thresholds.min_temperature = message;
                break;
            case "max_temperature":
                if (!devices[id].thresholds.max_temperature)
                    restoreValue = true;
                devices[id].thresholds.max_temperature = message;
                break;
            case "signal_strength":
                if (!devices[id].thresholds.signal_strength)
                    restoreValue = true;
                devices[id].thresholds.signal_strength = message;
                break;
            default:
                console.error(`Unknown threshold ${threshold} received in topic ${topic}`);
        }
    } else if (topic.includes("/alarms/")) {
        if (devices[id] && devices[id].registered_users.size > 0) {
            for (let user of devices[id].registered_users) {
                telegramBot.sendMessage(user, message)
                    .catch(err => console.error(err));
            }
        }
        else
            console.log(`Device ${id} has no registered users; ignoring alarm.`);
    }

    if (restoreValue)
        client.publish(`${topic}/update`, message);
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

    let json;
    try {
        json = JSON.parse(message);
    } catch (e) {
        console.error("Could not parse JSON:" + e);
        return;
    }
    let deviceTopic = `rms/${json.mac_address}/sensors`;
    if (json.status === "connected") {
        client.subscribe(deviceTopic, (err) => {
            if (err)
                console.error(err);
            else {
                console.log(`Subscribed to ${deviceTopic}`);
                devices[json.mac_address] = {
                    "id": json.mac_address,
                    "is_active": true,
                    "restored": false,
                    "room": undefined,
                    "thresholds": {
                        "min_temperature": undefined,
                        "max_temperature": undefined,
                        "signal_strength": undefined,
                    },
                    "sensors": {
                        "temperature": undefined,
                        "humidity": undefined,
                        "signal_strength": undefined
                    },
                    "registered_users": new Set()
                };
            }
        });
        subscribeTopic(`rms/${json.mac_address}/sensors/#`);
        subscribeTopic(`rms/${json.mac_address}/room`);
        subscribeTopic(`rms/${json.mac_address}/thresholds/+`);
        subscribeTopic(`rms/${json.mac_address}/active`);
    } else if (json.status === "disconnected") {
        client.unsubscribe(deviceTopic, (err) => {
            if (err)
                console.error(err);
            else {
                console.log(`Unsubscribed from ${deviceTopic}`);
                delete devices[json.mac_address];
            }
        });
    }
}

telegramBot.onText(/\/sensors (.+)/, (msg, regex_matches) => {

    let id = regex_matches[1];
    let message;
    if (devices[id]) {
        if (!devices[id].sensors.temperature && !devices[id].sensors.humidity && !devices[id].sensors.signal_strength) {
            message = `No data has been received by bevice ${id} yet`;
        } else {
            if (devices[id].sensors.temperature)
                message = `Temperature: ${devices[id].sensors.temperature} °C\n`;
            if (devices[id].sensors.humidity)
                message += `Humidity: ${devices[id].sensors.humidity}%\n`;
            if (devices[id].sensors.signal_strength)
                message += `WiFi signal strength: ${devices[id].sensors.signal_strength} dBm`;
        }
    }
    else
        message = "Device not found";
    telegramBot.sendMessage(msg.chat.id, message)
        .catch(err => {
            console.error(err);
        });
});

telegramBot.onText(/\/help/, (msg) => {

    let chatId = msg.chat.id;

    telegramBot.sendMessage(chatId, "Comandi disponibili:\n\n" +
        "/start - \n" +
        "/devices - \n" +
        "/get_room <device id> - \n" +
        "/set_room <device id> <room number> - \n" +
        "/get_thresholds <device id> - \n" +
        "/set_threshold <device_id> <threshold> <value> - \n" +
        "/status <device_id> - \n" +
        "/pause <device id> - \n" +
        "/resume <device id> - "
    ).catch(err => {
        console.error(err);
    })
})

telegramBot.onText(/\/devices/, (msg) => {

    let message;
    if (Object.keys(devices).length > 0) {
        message = "Available devices:\n";
        for (let device of Object.keys(devices))
            message += "- " + device + "\n";
    } else
        message = "No device found";
    telegramBot.sendMessage(msg.chat.id, message)
               .catch(err => {
                   console.error(err);
               });
});

telegramBot.onText(/\/get_room (.+)/, (msg, regex_matches) => {

    let id = regex_matches[1];
    let message;
    if (devices[id])
        message = `Device ${id} is in room #${devices[id].room}`;
    else
        message = "Device not found";
    telegramBot.sendMessage(msg.chat.id, message)
        .catch(err => {
            console.error(err);
        });
});

telegramBot.onText(/\/set_room (.+) (\d)/, (msg, regex_matches) => {

    let id = regex_matches[1];
    let message;
    if (devices[id]) {
        let new_value = regex_matches[2];
        if (new_value < 1 || new_value > MAX_ROOMS)
            message = "Room out of range";
        else {
            client.publish(`rms/${id}/room/update`, new_value);
            devices[id].room = new_value;
            message = `Updated room for ${id}`;
        }
    }
    else
        message = "Device not found";
    telegramBot.sendMessage(msg.chat.id, message)
        .catch(err => {
            console.error(err);
        });
});

telegramBot.onText(/\/get_thresholds (.+)/, (msg, regex_matches) => {

    let id = regex_matches[1];
    let message;
    if (devices[id]) {
        message = `Device ${id} thresholds:\n\n` +
                  `- Min temperature: ${devices[id].thresholds.min_temperature} °C\n` +
                  `- Max temperature: ${devices[id].thresholds.max_temperature} °C\n` +
                  `- Signal strength: ${devices[id].thresholds.signal_strength} dBm`;
    }
    else
        message = "Device not found";
    telegramBot.sendMessage(msg.chat.id, message)
        .catch(err => {
            console.error(err);
        });
});

telegramBot.onText(/\/set_threshold (.+) (min_temperature|max_temperature|signal_strength) (-?\d+.?\d+)/, (msg, regex_matches) => {

    let id = regex_matches[1];
    let new_value = regex_matches[3];
    let message;
    if (devices[id]) {
        let threshold = regex_matches[2];
        switch (threshold) {
            case "min_temperature":
                if (new_value >= devices[id].max_temperature)
                    message = "The low temperature threshold must be lower than the high temperature threshold";
                else if (new_value < TEMPERATURE_THRESHOLD_MIN || new_value > TEMPERATURE_THRESHOLD_MAX)
                    message = "Low temperature threshold out of range";
                else {
                    client.publish(`rms/${id}/thresholds/${threshold}/update`, new_value);
                    devices[id].thresholds.min_temperature = regex_matches[3];
                    message = `Updated min_temperature threshold for device ${id}`;
                }
                break;
            case "max_temperature":
                if (new_value < TEMPERATURE_THRESHOLD_MIN || new_value > TEMPERATURE_THRESHOLD_MAX)
                    message = "High temperature threshold out of range";
                else {
                    client.publish(`rms/${id}/thresholds/${threshold}/update`, new_value);
                    devices[id].thresholds.max_temperature = regex_matches[3];
                    message = `Updated max_temperature threshold for device ${id}`;
                }
                break;
            case "signal_strength":
                if (new_value < SIGNAL_STRENGTH_THRESHOLD_MAX || new_value > SIGNAL_STRENGTH_THRESHOLD_MIN)
                    message = "Signal strength threshold out of range";
                else {
                    client.publish(`rms/${id}/thresholds/${threshold}/update`, new_value);
                    devices[id].thresholds.signal_strength = regex_matches[3];
                    message = `Updated signal_strength threshold for device ${id}`;
                }
                break;
            default:
                message = "Invalid threshold";
        }
    }
    else
        message = "Device not found";
    telegramBot.sendMessage(msg.chat.id, message)
        .catch(err => {
            console.error(err);
        });
});

telegramBot.onText(/\/status (.+)/, (msg, regex_matches) => {

    let id = regex_matches[1];
    let message;
    if (devices[id]) {
        if (devices[id].is_active)
            message = `Device ${id} is active`;
        else
            message = `Device ${id} is paused`;
    }
    else
        message = "Device not found";
    telegramBot.sendMessage(msg.chat.id, message)
        .catch(err => {
            console.error(err);
        });
});

telegramBot.onText(/\/pause (.+)/, (msg, regex_matches) => {

    let id = regex_matches[1];
    let message;
    if (devices[id]) {
        if (devices[id].is_active) {
            client.publish(`rms/${id}/active/update`, "false");
            devices[id].is_active = false;
            message = `Device ${id} paused`;
        } else
            message = `Device ${id} is already paused`;
    }
    else
        message = "Device not found";
    telegramBot.sendMessage(msg.chat.id, message)
        .catch(err => {
            console.error(err);
        });
});

telegramBot.onText(/\/resume (.+)/, (msg, regex_matches) => {

    let id = regex_matches[1];
    let message;
    if (devices[id]) {
        if (devices[id].is_active)
            message = `Device ${id} is already active`;
        else {
            client.publish(`rms/${id}/active/update`, "true");
            devices[id].is_active = true;
            message = `Device ${id} resumed`;
        }
    }
    else
        message = "Device not found";
    telegramBot.sendMessage(msg.chat.id, message)
        .catch(err => {
            console.error(err);
        });
});

telegramBot.onText(/\/register (.+)/, (msg, regex_matches) => {

    let id = regex_matches[1];
    let message;
    if (devices[id]) {
        if (devices[id].registered_users.has(msg.chat.id))
            message = `Device ${id} already registered`;
        else {
            devices[id].registered_users.add(msg.chat.id);
            message = `Registered device ${id}`;
        }
    }
    else
        message = "Device not found";
    telegramBot.sendMessage(msg.chat.id, message)
        .catch(err => {
            console.error(err);
        });
});

telegramBot.onText(/\/unregister (.+)/, (msg, regex_matches) => {

    let id = regex_matches[1];
    let message;
    if (devices[id]) {
        if (devices[id].registered_users.has(msg.chat.id)) {
            message = `Unregistered device ${id}`;
            devices[id].registered_users.delete(msg.chat.id);
        } else
            message = `Device ${id} is not registered`;
    }
    else
        message = "Device not found";
    telegramBot.sendMessage(msg.chat.id, message)
        .catch(err => {
            console.error(err);
        });
});

telegramBot.onText(/.+/, (msg) => {

    let message;
    let text = msg.text;
    if (text.startsWith("/sensors"))
        message = "Usage: /sensors <device id>";
    else if (text.startsWith("/get_room"))
        message = "Usage: /get_room <device id>";
    else if (text.startsWith("/set_room"))
        message = "Usage: /set_room <device id> <room number>";
    else if (text.startsWith("/get_thresholds"))
        message = "Usage: /get_thresholds <device id>";
    else if (text.startsWith("/set_threshold"))
        message = "Usage: /set_threshold <device_id> <threshold> <value>";
    else if (text.startsWith("/status"))
        message = "Usage: /status <device_id>";
    else if (text.startsWith("/pause"))
        message = "Usage: /pause <device id>";
    else if (text.startsWith("/resume"))
        message = "Usage: /resume <device id>";
    else if (text.startsWith("/register"))
        message = "Usage: /resume <device id>";
    else if (text.startsWith("/unregister"))
        message = "Usage: /resume <device id>";
    else
        message = "Unknown command. Use /help to...";
    telegramBot.sendMessage(msg.chat.id, message)
        .catch(err => {
            console.error(err);
        });
})

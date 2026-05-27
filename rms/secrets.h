// Use this file to store all of the private credentials and connection details

// InfluxDB cfg
#define INFLUXDB_URL "<influxdb-url>"               // IP and port of the InfluxDB server
#define INFLUXDB_TOKEN "<api-auth-token>"           // API authentication token. Use an existing token or create a new one:
                                                    //  InfluxDB UI -> Load Data -> API Tokens -> Generate API Token -> Custom API Token
                                                    //  set a name for the token and select read and write permissions for your buket only!
#define INFLUXDB_ORG "<influxdb-org>"               // organization id (Use: InfluxDB UI -> Profile -> About -> <name under organization profile> )
#define INFLUXDB_BUCKET "<influxdb-bucket>"         // bucket name (Use: InfluxDB UI -> Load Data -> Buckets)

// MQTT access
#define MQTT_BROKER_IP "149.132.178.179"            // IP address of the machine running the MQTT broker
#define MQTT_CLIENT_ID "esp8266"                    // client identifier
#define MQTT_USERNAME "mqttBroker_user"             // mqtt user's name
#define MQTT_PASSWORD "mqttBroker_pass"             // mqtt user's password

#define OWM_API_KEY "API_KEY"                       // api key from https://home.openweathermap.org/api_keys"
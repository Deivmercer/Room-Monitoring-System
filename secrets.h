// Use this file to store all of the private credentials and connection details

// WiFi configuration
#define SECRET_SSID "WiFi_SSID"                  // SSID
#define SECRET_PASS "WiFi_Password"              // WiFi password

// ONLY if static configuration is needed
// #define IP {149, 132, 182, 140}                  // IP address
// #define SUBNET {255, 255, 255, 0}                // Subnet mask
// #define DNS {149, 132, 2, 3}                     // DNS
// #define GATEWAY {149, 132, 182, 1}               // Gateway

// InfluxDB cfg
#define INFLUXDB_URL "<influxdb-url>"               // IP and port of the InfluxDB server
#define INFLUXDB_TOKEN "<api-auth-token>"           // API authentication token. Use an existing token or create a new one:
                                                    //  InfluxDB UI -> Load Data -> API Tokens -> Generate API Token -> Custom API Token
                                                    //  set a name for the token and select read and write permissions for your buket only!
#define INFLUXDB_ORG "<influxdb-org>"               // organization id (Use: InfluxDB UI -> Profile -> About -> <name under organization profile> )
#define INFLUXDB_BUCKET "<influxdb-bucket>"         // bucket name (Use: InfluxDB UI -> Load Data -> Buckets)

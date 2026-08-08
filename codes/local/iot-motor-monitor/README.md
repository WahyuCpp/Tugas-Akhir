# ESP32 Induction Motor Monitor — Full Stack

```
firmware/          ESP32 Arduino sketch (raw sample batching, MQTT/TLS publish)
infrastructure/    AWS SAM template — IoT Rule, Lambda, DynamoDB, Cognito, API Gateway
lambda/             Python handlers: ingest, FFT, historical query
react-app/          Live + historical dashboard (Cognito auth, MQTT-WSS, REST)
```

## Prerequisites

- AWS CLI configured (`aws configure`) with an IAM user (not root)
- AWS SAM CLI installed (`pip install aws-sam-cli` or see AWS docs)
- Node.js 18+ for the React app
- Arduino IDE or PlatformIO with the ESP32 board package

## Deploy order

### 1. Get your account's IoT endpoint (needed for step 2 and firmware)
```bash
aws iot describe-endpoint --endpoint-type iot:Data-ATS
```

### 2. Deploy the backend (IoT Rule, Lambda, DynamoDB, Cognito, API Gateway)
```bash
cd infrastructure
sam build
sam deploy --guided
# When prompted, paste the IotEndpoint value from step 1
```
Note the stack outputs (`ApiEndpoint`, `UserPoolId`, `UserPoolClientId`, `IdentityPoolId`) — you'll need these in step 4.

### 3. Provision the ESP32 device identity
```bash
aws iot create-thing --thing-name esp32-motor-01

aws iot create-keys-and-certificate \
  --set-as-active \
  --certificate-pem-outfile firmware/certs/cert.pem \
  --public-key-outfile firmware/certs/public.key \
  --private-key-outfile firmware/certs/private.key \
  --query certificateArn --output text
# Save the returned certificateArn for the next command

aws iot create-policy \
  --policy-name esp32-motor-01-policy \
  --policy-document file://infrastructure/iot_policy.json
  # Edit iot_policy.json first: replace REGION and ACCOUNT_ID

aws iot attach-policy --policy-name esp32-motor-01-policy --target <certificateArn>
aws iot attach-thing-principal --thing-name esp32-motor-01 --principal <certificateArn>
```
Download `AmazonRootCA1.pem` from https://www.amazontrust.com/repository/AmazonRootCA1.pem

### 4. Configure and flash the ESP32
```bash
cd firmware
cp secrets.h.example secrets.h
# Edit secrets.h: WiFi credentials, THING_NAME, AWS_IOT_ENDPOINT,
# and paste in AmazonRootCA1.pem / cert.pem / private.key contents
```
Open `esp32_motor_monitor.ino` in Arduino IDE, install PubSubClient + ArduinoJson via Library Manager, select your ESP32 board, upload.

Verify: AWS IoT Core Console → MQTT test client → subscribe to `sensors/raw/esp32-motor-01` → confirm batches arrive.

### 5. Create a Cognito user for dashboard login
```bash
aws cognito-idp admin-create-user \
  --user-pool-id <UserPoolId from step 2> \
  --username you@example.com \
  --temporary-password 'TempPass123!' \
  --user-attributes Name=email,Value=you@example.com Name=email_verified,Value=true
```
You'll be prompted to set a permanent password on first login.

### 6. Configure and run the React app
```bash
cd react-app
# Edit src/aws-config.js with all values from step 2's stack outputs
npm install
npm start
```

## Verifying each layer independently (per the incremental build order)

1. **ESP32 → IoT Core**: MQTT test client shows incoming batches (step 4 above)
2. **IoT Rule → Lambda ingest → raw_buffer table**: check DynamoDB console, `<project>-raw-buffer` table populates
3. **FFT Lambda**: CloudWatch Logs for the scheduled function; `<project>-fft-results` table populates every ~60s
4. **Live path**: MQTT test client subscribed to `results/esp32-motor-01` shows FFT output
5. **React live view**: dashboard chart updates without refresh
6. **React historical view**: table below the chart populates from API Gateway

## Known scope limits in this scaffold

- `DEVICE_IDS` in `fft_handler.py` is a hardcoded env var, not a device registry — fine for one device, needs a `Devices` table + scan for fleet scale
- API Gateway CORS is `*` — restrict to your deployed React app's origin before any production use
- No MCCB/trip-threshold logic included — add as either a branch inside `fft_handler.py` or a separate Lambda subscribed to `results/*`, per the earlier design discussion
- Single device topic hardcoded in React (`DEVICE_ID` constant) — add a device selector once you have more than one ESP32

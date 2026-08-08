import React, { useEffect, useState, useCallback } from "react";
import { fromCognitoIdentityPool } from "@aws-sdk/credential-providers";
import { mqtt5, iot } from "aws-iot-device-sdk-v2";
import Login from "./components/Login";
import FFTChart from "./components/FFTChart";
import { AWS_CONFIG } from "./aws-config";

const DEVICE_ID = "esp32-motor-01"; // TODO: make this selectable once you have >1 device

export default function App() {
  const [auth, setAuth] = useState(null); // { idToken }
  const [liveResult, setLiveResult] = useState(null);
  const [history, setHistory] = useState([]);
  const [connectionStatus, setConnectionStatus] = useState("disconnected");

  // ---- Live MQTT-WSS subscribe (Cognito Identity Pool -> temporary IAM creds) ----
  useEffect(() => {
    if (!auth) return;

    let client;
    let cancelled = false;

    async function connect() {
      const credentialsProvider = fromCognitoIdentityPool({
        clientConfig: { region: AWS_CONFIG.region },
        identityPoolId: AWS_CONFIG.identityPoolId,
        logins: {
          [`cognito-idp.${AWS_CONFIG.region}.amazonaws.com/${AWS_CONFIG.userPoolId}`]:
            auth.idToken,
        },
      });

      const credentials = await credentialsProvider();

      const builder = iot.AwsIotMqtt5ClientConfigBuilder.newWebsocketMqttBuilderWithSigv4Auth(
        AWS_CONFIG.iotEndpoint,
        {
          region: AWS_CONFIG.region,
          credentialsProvider: {
            getCredentials: async () => ({
              aws_access_id: credentials.accessKeyId,
              aws_secret_key: credentials.secretAccessKey,
              aws_sts_token: credentials.sessionToken,
              aws_region: AWS_CONFIG.region,
            }),
          },
        }
      );

      client = new mqtt5.Mqtt5Client(builder.build());

      client.on("connectionSuccess", () => !cancelled && setConnectionStatus("connected"));
      client.on("disconnection", () => !cancelled && setConnectionStatus("disconnected"));
      client.on("messageReceived", (eventData) => {
        try {
          const payload = new TextDecoder().decode(eventData.message.payload);
          const parsed = JSON.parse(payload);
          if (!cancelled) setLiveResult(parsed);
        } catch (err) {
          console.error("Failed to parse live message", err);
        }
      });

      client.start();
      await client.subscribe({
        subscriptions: [
          { qos: mqtt5.QoS.AtLeastOnce, topicFilter: `results/${DEVICE_ID}` },
        ],
      });
    }

    connect().catch((err) => console.error("MQTT connect failed", err));

    return () => {
      cancelled = true;
      client?.stop();
    };
  }, [auth]);

  // ---- Historical fetch via API Gateway (Cognito-authorized REST) ----
  const fetchHistory = useCallback(async () => {
    if (!auth) return;
    try {
      const res = await fetch(`${AWS_CONFIG.apiEndpoint}/results/${DEVICE_ID}?limit=20`, {
        headers: { Authorization: auth.idToken },
      });
      if (!res.ok) throw new Error(`API returned ${res.status}`);
      setHistory(await res.json());
    } catch (err) {
      console.error("History fetch failed", err);
    }
  }, [auth]);

  useEffect(() => {
    fetchHistory();
  }, [fetchHistory]);

  if (!auth) {
    return <Login onAuthenticated={setAuth} />;
  }

  return (
    <div style={{ background: "#0f172a", minHeight: "100vh", padding: "24px", fontFamily: "system-ui, sans-serif" }}>
      <header style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: "24px" }}>
        <h1 style={{ color: "#e2e8f0", fontSize: "20px" }}>Motor Monitor — {DEVICE_ID}</h1>
        <span style={{ color: connectionStatus === "connected" ? "#4ade80" : "#f87171" }}>
          ● {connectionStatus}
        </span>
      </header>

      <section>
        <h2 style={{ color: "#94a3b8", fontSize: "14px", textTransform: "uppercase" }}>
          Live FFT (updates every 60s)
        </h2>
        {liveResult ? (
          <>
            <FFTChart axisData={liveResult.axes.x} axisLabel="x" />
            <FFTChart axisData={liveResult.axes.y} axisLabel="y" />
            <FFTChart axisData={liveResult.axes.z} axisLabel="z" />
          </>
        ) : (
          <p style={{ color: "#64748b" }}>Waiting for first result...</p>
        )}
      </section>

      <section style={{ marginTop: "32px" }}>
        <h2 style={{ color: "#94a3b8", fontSize: "14px", textTransform: "uppercase" }}>
          Recent history ({history.length})
        </h2>
        <table style={{ width: "100%", color: "#cbd5e1", borderCollapse: "collapse" }}>
          <thead>
            <tr style={{ textAlign: "left", borderBottom: "1px solid #334155" }}>
              <th>Timestamp</th>
              <th>Dominant Freq (X)</th>
              <th>Samples</th>
            </tr>
          </thead>
          <tbody>
            {history.map((row) => (
              <tr key={row.timestamp} style={{ borderBottom: "1px solid #1e293b" }}>
                <td>{new Date(row.timestamp).toLocaleTimeString()}</td>
                <td>{row.axes?.x?.dominant_freq?.toFixed(1) ?? "—"} Hz</td>
                <td>{row.sample_count}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </section>
    </div>
  );
}

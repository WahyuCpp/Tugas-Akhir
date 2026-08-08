"""
Triggered on a schedule (EventBridge, rate: 1 minute).
For each known device: pull the last 60s of buffered samples, run FFT on
each axis, persist to fft_results, republish to results/{deviceId} for
any React client subscribed live via MQTT-WSS.

NOTE: iterating devices from a hardcoded list below. For fleet scale,
replace DEVICE_IDS with a query against a Devices registry table instead.
"""
import os
import time
import json
import boto3
import numpy as np
from boto3.dynamodb.conditions import Key

dynamodb = boto3.resource("dynamodb")
buffer_table = dynamodb.Table(os.environ["RAW_BUFFER_TABLE"])
result_table = dynamodb.Table(os.environ["FFT_RESULTS_TABLE"])
IOT_ENDPOINT = os.environ["IOT_ENDPOINT"]  # e.g. xxxxxxxxxxxxx-ats.iot.REGION.amazonaws.com
iot_client = boto3.client("iot-data", endpoint_url=f"https://{IOT_ENDPOINT}")

SAMPLE_RATE_HZ = float(os.environ.get("SAMPLE_RATE_HZ", 1000))
RESULTS_TOPIC_PREFIX = os.environ.get("RESULTS_TOPIC_PREFIX", "results")
WINDOW_SECONDS = 60

# TODO: replace with a DynamoDB scan/query against a Devices table for fleet scale
DEVICE_IDS = os.environ.get("DEVICE_IDS", "esp32-motor-01").split(",")


def compute_fft(samples: np.ndarray, sample_rate: float) -> dict:
    n = len(samples)
    fft_vals = np.fft.rfft(samples)
    freqs = np.fft.rfftfreq(n, d=1 / sample_rate)
    magnitude = np.abs(fft_vals) / n
    dominant_idx = int(np.argmax(magnitude[1:]) + 1)  # skip DC bin
    return {
        "freqs": freqs.tolist(),
        "magnitude": magnitude.tolist(),
        "dominant_freq": float(freqs[dominant_idx]),
        "dominant_magnitude": float(magnitude[dominant_idx]),
    }


def handler(event, context):
    now_ms = int(time.time() * 1000)
    window_start_ms = now_ms - WINDOW_SECONDS * 1000
    processed = []

    for device_id in DEVICE_IDS:
        device_id = device_id.strip()
        resp = buffer_table.query(
            KeyConditionExpression=Key("deviceId").eq(device_id)
            & Key("timestamp").between(window_start_ms, now_ms)
        )
        items = resp.get("Items", [])
        if not items:
            continue

        x = np.concatenate([np.array(i["x"], dtype=float) for i in items if i.get("x")])
        y = np.concatenate([np.array(i["y"], dtype=float) for i in items if i.get("y")])
        z = np.concatenate([np.array(i["z"], dtype=float) for i in items if i.get("z")])

        result = {
            "deviceId": device_id,
            "timestamp": now_ms,
            "axes": {
                "x": compute_fft(x, SAMPLE_RATE_HZ) if len(x) > 1 else None,
                "y": compute_fft(y, SAMPLE_RATE_HZ) if len(y) > 1 else None,
                "z": compute_fft(z, SAMPLE_RATE_HZ) if len(z) > 1 else None,
            },
            "sample_count": int(len(x)),
        }

        result_table.put_item(Item=result)

        # push to live topic — React subscribers get this without polling
        iot_client.publish(
            topic=f"{RESULTS_TOPIC_PREFIX}/{device_id}",
            qos=1,
            payload=json.dumps(result),
        )

        processed.append(device_id)

    return {"processed": processed, "windowEndMs": now_ms}

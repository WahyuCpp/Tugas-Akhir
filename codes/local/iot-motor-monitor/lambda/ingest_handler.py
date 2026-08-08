"""
Triggered by IoT Rule: SELECT *, topic(3) as deviceId FROM 'sensors/raw/+'

Expected incoming payload (published by ESP32):
{
  "deviceTimestamp": 123456,   # ms since ESP32 boot, monotonic
  "x": [..floats..],
  "y": [..floats..],
  "z": [..floats..],
  "deviceId": "esp32-motor-01"  # injected by the IoT Rule from the topic path
}
"""
import os
import time
import boto3

dynamodb = boto3.resource("dynamodb")
buffer_table = dynamodb.Table(os.environ["RAW_BUFFER_TABLE"])

BUFFER_TTL_SECONDS = 90  # window (60s) + safety margin for late/out-of-order batches


def handler(event, context):
    device_id = event.get("deviceId")
    if not device_id:
        raise ValueError("deviceId missing — check IoT Rule SQL topic() extraction")

    server_ts = int(time.time() * 1000)

    buffer_table.put_item(
        Item={
            "deviceId": device_id,
            "timestamp": server_ts,          # sort key: server-side arrival time
            "deviceTimestamp": event.get("deviceTimestamp"),
            "x": event.get("x", []),
            "y": event.get("y", []),
            "z": event.get("z", []),
            "ttl": int(time.time()) + BUFFER_TTL_SECONDS,
        }
    )

    return {"status": "buffered", "deviceId": device_id, "timestamp": server_ts}

"""
Behind API Gateway, Cognito-authorized.
GET /results/{deviceId}?start=<epoch_ms>&end=<epoch_ms>&limit=100
"""
import os
import json
import boto3
from boto3.dynamodb.conditions import Key

dynamodb = boto3.resource("dynamodb")
result_table = dynamodb.Table(os.environ["FFT_RESULTS_TABLE"])


def handler(event, context):
    device_id = event["pathParameters"]["deviceId"]
    qs = event.get("queryStringParameters") or {}
    limit = int(qs.get("limit", 100))

    key_condition = Key("deviceId").eq(device_id)
    if "start" in qs and "end" in qs:
        key_condition = key_condition & Key("timestamp").between(
            int(qs["start"]), int(qs["end"])
        )

    resp = result_table.query(
        KeyConditionExpression=key_condition,
        ScanIndexForward=False,  # most recent first
        Limit=limit,
    )

    return {
        "statusCode": 200,
        "headers": {
            "Content-Type": "application/json",
            "Access-Control-Allow-Origin": "*",  # tighten before production
        },
        "body": json.dumps(resp.get("Items", [])),
    }

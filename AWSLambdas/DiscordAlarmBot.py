import boto3
import json
import os
import hmac
import hashlib
import base64
import nacl.signing
import nacl.exceptions
import time
from datetime import datetime
from zoneinfo import ZoneInfo

iot = boto3.client('iot-data', region_name='us-east-1')
THING_NAME = 'ThingName'  # replace with your actual IoT thing name
DISCORD_PUBLIC_KEY = "publicKey"

def verify_signature(event):
    headers = event['headers']
    signature = headers['x-signature-ed25519']
    timestamp = headers['x-signature-timestamp']
    body = event['body']

    if not signature or not timestamp:
        print("Missing headers")
        return False
    
    try:
        message = timestamp + body
        public_key_bytes = bytes.fromhex(DISCORD_PUBLIC_KEY)
        verify_key = nacl.signing.VerifyKey(public_key_bytes)
        verify_key.verify(message.encode(), bytes.fromhex(signature))
        return True
    except nacl.exceptions.BadSignatureError:
        print("Bad signature")
        return False

def lambda_handler(event, context):
    print("Event:", json.dumps(event))

    if not verify_signature(event):
        return {"statusCode": 401, "body": "invalid request signature"}

    body = json.loads(event['body'])
    print("Parsed body:", body)
    
    # Discord PING request
    if body.get("type") == 1:
        print("here")
        return {
            "statusCode": 200,
            "headers": {
                "Content-Type": "application/json"
            },
            "body": json.dumps({"type": 1})
        }

    if body['data']:  # Slash command
        if body['data']['name'] == 'alarm':
            status = body['data']['options'][0]['value']

            payload = {
                "state": {
                    "desired": {
                        "Message" : {
                            "default" : "Alarm Message",
                            "email" : f"Alarm {status}"
                        }
                    }
                }
            }

            iot.update_thing_shadow(
                thingName=THING_NAME,
                payload=json.dumps(payload)
            )

            return {
                "statusCode": 200,
                "body": json.dumps({
                    "type": 4,
                    "data": {
                        "content": f"✅ Alarm set to `{status}`"
                    }
                })
            }
        elif body['data']['name'] == 'timer':
            pdt = ZoneInfo("America/Los_Angeles")
            start_str = body['data']['options'][0]['value']
            stop_str = body['data']['options'][1]['value']

            # Localize to PDT
            start_dt = datetime.fromisoformat(start_str).replace(tzinfo=pdt)
            stop_dt = datetime.fromisoformat(stop_str).replace(tzinfo=pdt)

            # Convert to UNIX timestamp
            on_unix = int(start_dt.timestamp())
            off_unix = int(stop_dt.timestamp())

            # Update device shadow
            shadow_payload = {
                "state": {
                    "desired": {
                        "timer": {
                            "on": on_unix,
                            "off": off_unix
                        }
                    }
                }
            }

            iot.update_thing_shadow(
                thingName=THING_NAME,
                payload=json.dumps(shadow_payload)
            )

            # Send confirmation to Discord
            return {
                "statusCode": 200,
                "body": json.dumps({
                    "type": 4,
                    "data": {
                        "content": f"Alarm timer set!\n🕐 On: {on_unix}\n🕔 Off: {off_unix}"
                    }
                })
            }


    return { "statusCode": 400, "body": "unrecognized type" }

import json
import urllib3
from datetime import datetime
from zoneinfo import ZoneInfo

http = urllib3.PoolManager()

WEBHOOK_URL = "webhookURL"  # ← REPLACE THIS!

def unix_to_readable(ts):   
    # Convert timestamp to PDT (America/Los_Angeles)
    pdt_time = datetime.fromtimestamp(ts, ZoneInfo("America/Los_Angeles"))
    pdt_str = pdt_time.strftime('%Y-%m-%d %I:%M:%S %p %Z')

    # Also include UTC time
    utc_time = datetime.utcfromtimestamp(ts).replace(tzinfo=ZoneInfo("UTC"))
    utc_str = utc_time.strftime('%Y-%m-%d %H:%M:%S %Z')

    return f"{pdt_str} / {utc_str}"

def lambda_handler(event, context):
    try:
        # Safely extract the email field from the shadow state
        desired = event.get("desired", {})

        # Default payload
        payload = {
            "content": "❓ Unknown message format received."
        }

        # Case 1: Alarm email message
        if "Message" in desired and "email" in desired["Message"]:
            message = desired["Message"]["email"]
            payload["content"] = f"🚨 Alarm Message:\n```{message}```"

        # Case 2: Timer message
        elif "timer" in desired and "on" in desired["timer"] and "off" in desired["timer"]:
            on = desired["timer"]["on"]
            off = desired["timer"]["off"]
            payload["content"] = (
                "⏰ Alarm Timer Updated:\n"
                f"🟢 Start: `{unix_to_readable(on)}`\n"
                f"🔴 Stop : `{unix_to_readable(off)}`"
            )

        response = http.request(
            "POST",
            WEBHOOK_URL,
            body=json.dumps(payload),
            headers={"Content-Type": "application/json"}
        )

        return {
            "statusCode": response.status,
            "body": response.data.decode()
        }

    except Exception as e:
        return {
            "statusCode": 500,
            "body": str(e)
        }

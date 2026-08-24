#!/usr/bin/env python3
"""Send an email via SMTP. If SMTP is not configured, prints the message to stdout."""

import json
import os
import smtplib
import ssl
import sys
from email.mime.text import MIMEText
from email.utils import formataddr


def load_payload(path: str) -> dict:
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def send_email(payload: dict) -> bool:
    host = os.environ.get("SMTP_HOST", "").strip()
    port = int(os.environ.get("SMTP_PORT", "587").strip() or 587)
    user = os.environ.get("SMTP_USER", "").strip()
    password = os.environ.get("SMTP_PASS", "").strip()
    from_email = os.environ.get("FROM_EMAIL", user).strip()

    to = payload.get("to", "").strip()
    subject = payload.get("subject", "").strip()
    body = payload.get("body", "").strip()
    is_html = bool(payload.get("html"))

    if not host or not user or not password or not from_email:
        print("SMTP not configured. Email would be sent to:", to)
        print("Subject:", subject)
        print("Body:\n" + body)
        return True

    if not to or not subject or not body:
        print("Missing to, subject, or body", file=sys.stderr)
        return False

    msg = MIMEText(body, "html" if is_html else "plain", "utf-8")
    msg["Subject"] = subject
    msg["From"] = formataddr(("Online Compiler", from_email))
    msg["To"] = to

    try:
        if port == 465:
            context = ssl.create_default_context()
            with smtplib.SMTP_SSL(host, port, context=context, timeout=30) as server:
                server.login(user, password)
                server.sendmail(from_email, [to], msg.as_string())
        else:
            with smtplib.SMTP(host, port, timeout=30) as server:
                server.ehlo()
                if server.has_extn("STARTTLS"):
                    context = ssl.create_default_context()
                    server.starttls(context=context)
                    server.ehlo()
                if user and password:
                    server.login(user, password)
                server.sendmail(from_email, [to], msg.as_string())
        print("Email sent to:", to)
        return True
    except Exception as e:
        print("Failed to send email:", e, file=sys.stderr)
        return False


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: send_email.py <json-file>", file=sys.stderr)
        sys.exit(1)

    payload = load_payload(sys.argv[1])
    ok = send_email(payload)
    sys.exit(0 if ok else 1)

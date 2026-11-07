#!/usr/bin/env bash
# Signs VSL Finesse with a Developer ID certificate, notarizes it with Apple
# and staples the ticket, so it opens with a double-click on any Mac.
#
#   scripts/sign-and-notarize.sh <path/to/VSL Finesse.app> <path/to/vsl-finesse> <output dir>
#
# Needs these environment variables (GitHub Actions passes them from secrets):
#   MACOS_CERTIFICATE_P12_BASE64  Developer ID Application certificate + key (.p12), base64
#   MACOS_CERTIFICATE_PASSWORD    password of the .p12
#   NOTARY_API_KEY_P8_BASE64      App Store Connect API key (.p8), base64
#   NOTARY_API_KEY_ID             its Key ID
#   NOTARY_API_ISSUER_ID          its Issuer ID
set -euo pipefail

APP="$1"
CLI="$2"
OUT="$3"
ENTITLEMENTS="$(cd "$(dirname "$0")/.." && pwd)/app/VSLFinesse.entitlements"
WORK="${RUNNER_TEMP:-$(mktemp -d)}/vslf-signing"
mkdir -p "$WORK" "$OUT"

# --- A temporary keychain holding the certificate -----------------------------
KEYCHAIN="$WORK/signing.keychain-db"
KEYCHAIN_PASSWORD="$(uuidgen)"
security create-keychain -p "$KEYCHAIN_PASSWORD" "$KEYCHAIN"
security set-keychain-settings -lut 21600 "$KEYCHAIN"
security unlock-keychain -p "$KEYCHAIN_PASSWORD" "$KEYCHAIN"
trap 'security delete-keychain "$KEYCHAIN" >/dev/null 2>&1 || true; rm -f "$WORK/cert.p12" "$WORK/key.p8"' EXIT

echo "$MACOS_CERTIFICATE_P12_BASE64" | base64 --decode > "$WORK/cert.p12"
security import "$WORK/cert.p12" -k "$KEYCHAIN" -P "$MACOS_CERTIFICATE_PASSWORD" -T /usr/bin/codesign
security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "$KEYCHAIN_PASSWORD" "$KEYCHAIN" >/dev/null
security list-keychains -d user -s "$KEYCHAIN" $(security list-keychains -d user | tr -d '"')

IDENTITY="$(security find-identity -v -p codesigning "$KEYCHAIN" | grep 'Developer ID Application' | head -1 | sed -E 's/.*"(.*)"/\1/')"
if [[ -z "$IDENTITY" ]]; then
    echo "No 'Developer ID Application' certificate in MACOS_CERTIFICATE_P12_BASE64." >&2
    echo "(An 'Apple Development' or 'Mac App Distribution' certificate can't be notarized for direct download.)" >&2
    exit 1
fi
echo "Signing as: $IDENTITY"

# --- Sign: hardened runtime + secure timestamp (both required by notarization)
codesign --force --options runtime --timestamp --entitlements "$ENTITLEMENTS" --sign "$IDENTITY" "$APP"
codesign --force --options runtime --timestamp --sign "$IDENTITY" "$CLI"
codesign --verify --strict --verbose=2 "$APP"
codesign --verify --strict --verbose=2 "$CLI"

# --- Notarize --------------------------------------------------------------------
echo "$NOTARY_API_KEY_P8_BASE64" | base64 --decode > "$WORK/key.p8"
SUBMISSION="$WORK/submission"
mkdir -p "$SUBMISSION"
cp -R "$APP" "$SUBMISSION/"
cp "$CLI" "$SUBMISSION/"
ditto -c -k --keepParent "$SUBMISSION" "$WORK/submission.zip"

RESULT="$(xcrun notarytool submit "$WORK/submission.zip" \
              --key "$WORK/key.p8" --key-id "$NOTARY_API_KEY_ID" --issuer "$NOTARY_API_ISSUER_ID" \
              --wait --timeout 30m --output-format json)"
echo "$RESULT"
STATUS="$(echo "$RESULT" | /usr/bin/python3 -c 'import json,sys; print(json.load(sys.stdin).get("status",""))')"
ID="$(echo "$RESULT" | /usr/bin/python3 -c 'import json,sys; print(json.load(sys.stdin).get("id",""))')"
if [[ "$STATUS" != "Accepted" ]]; then
    echo "Notarization failed ($STATUS). Apple's log:" >&2
    [[ -n "$ID" ]] && xcrun notarytool log "$ID" --key "$WORK/key.p8" --key-id "$NOTARY_API_KEY_ID" --issuer "$NOTARY_API_ISSUER_ID" >&2 || true
    exit 1
fi

# --- Staple the ticket to the app so it opens offline too ------------------------
xcrun stapler staple "$APP"
xcrun stapler validate "$APP"
spctl --assess --type execute --verbose=2 "$APP"

ditto -c -k --keepParent "$APP" "$OUT/VSL-Finesse-macOS.zip"
cp "$CLI" "$OUT/"
echo "Signed and notarized: $OUT"

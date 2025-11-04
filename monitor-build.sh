#!/bin/bash
# Monitor the WebRTC 800ms build progress

RUN_ID="19022458302"

echo "Monitoring WebRTC AEC3 800ms Build"
echo "==================================="
echo ""

while true; do
    clear
    echo "WebRTC AEC3 800ms Build Monitor"
    echo "================================"
    echo ""
    echo "Run ID: $RUN_ID"
    echo "URL: https://github.com/lukepg/webrtc-aec3-800ms/actions/runs/$RUN_ID"
    echo ""

    # Get run status
    gh run view $RUN_ID --repo lukepg/webrtc-aec3-800ms 2>&1

    # Check if complete
    STATUS=$(gh run view $RUN_ID --repo lukepg/webrtc-aec3-800ms --json status -q .status 2>&1)

    echo ""
    echo "Current status: $STATUS"
    echo ""

    if [[ "$STATUS" == "completed" ]]; then
        CONCLUSION=$(gh run view $RUN_ID --repo lukepg/webrtc-aec3-800ms --json conclusion -q .conclusion 2>&1)
        echo "Build completed with: $CONCLUSION"

        if [[ "$CONCLUSION" == "success" ]]; then
            echo ""
            echo "✅ Build successful! Downloading artifacts..."
            gh run download $RUN_ID --repo lukepg/webrtc-aec3-800ms -D artifacts/
            echo ""
            echo "Artifacts downloaded to: artifacts/"
            ls -lh artifacts/*/libwebrtc_apms.so
        else
            echo ""
            echo "❌ Build failed. Showing logs:"
            gh run view $RUN_ID --repo lukepg/webrtc-aec3-800ms --log-failed
        fi
        break
    fi

    echo "Checking again in 60 seconds... (Ctrl+C to stop)"
    sleep 60
done

echo ""
echo "Monitoring complete."

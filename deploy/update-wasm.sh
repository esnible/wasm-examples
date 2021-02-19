#!/bin/bash
#
# Copyright 2021 IBM Corporation


# set -x
set -o errexit

if [ "$#" -lt 2 ]
then
  echo "Usage: update-wasm.sh <.wasm file> <root-id> [<config-string>]"
  exit 1
fi

# Note: This script is for informal testing and debugging.  It uses a ConfigMap with a fixed name.
# Use as a guide only!  This script always mounts the files on the sidecar with the
# name /var/local/wasm/new-filter.wasm which means you can't use this for two different filters!

# The filename containing Wasm byte codes to mount on the sidecar, for example regex-repl.wasm
WASMFILE=$1
# Name of Kubernetes ConfigMap to hold the Wasm byte codes.
CONFIGMAP=new-filter
# The name of the plugin (C++ and AssemblyScript SDKs), for example regex-repl
ROOT_ID=$2
# The configuration value to give to the plugin (string, possible JSON encoded)
CONFIG_VALUE=${3:-"\"\""}
# The Istio EnvoyFilter CR name, for example httpbin
FILTERNAME=httpbin
# The workload, for example "app=httpbin", something that could be passed to kubectl's --selector
WORKLOAD=app=httpbin
# The target, a pod or deployment (deployment should have one instance), for example "deployment/httpbin"
TARGET=deployment/httpbin
# We keep this around after running in case you want to see it or used it to delete your filter.
FILTERFILE="/tmp/httpbin-ef.yaml"

# STEP 1.  Create the EnvoyFilter
cat > "$FILTERFILE" << EOF
apiVersion: networking.istio.io/v1alpha3
kind: EnvoyFilter
metadata:
  name: $FILTERNAME
spec:
  workloadSelector:
    labels:
      ${WORKLOAD/=/: }
  configPatches:
  - applyTo: HTTP_FILTER
    match:
      context: SIDECAR_INBOUND
      listener:
        filterChain:
          filter:
            name: envoy.http_connection_manager
            subFilter:
              name: envoy.router
    patch:
      operation: INSERT_BEFORE
      value:
        name: mydummy
        typed_config:
          '@type': type.googleapis.com/udpa.type.v1.TypedStruct
          type_url: type.googleapis.com/envoy.extensions.filters.http.wasm.v3.Wasm
          value:
            config:
              configuration:
                '@type': type.googleapis.com/google.protobuf.StringValue
                value: $CONFIG_VALUE
              root_id: "$ROOT_ID"
              vm_config:
                code:
                  local:
                    filename: /var/local/wasm/new-filter.wasm
                runtime: envoy.wasm.runtime.v8
                vm_id: myvmdummy
EOF


# STEP 2.  Recreate the ConfigMap holding the WASM.  (It would be better to patch it,
# but I don't know how to do it when the file is big.  TODO!!!)
echo Creating ConfigMap "$CONFIGMAP" from "$WASMFILE"
if kubectl get configmap new-filter > /dev/null ; then
   kubectl delete configmap new-filter || true
fi
kubectl create configmap new-filter --from-file=new-filter.wasm="$WASMFILE"

# STEP 3. Wait until the pod has the exact byte codes we want, which may take up to a minute!
echo "Waiting for binary to be loaded upon sidecar"
until diff "$WASMFILE" <(kubectl exec "$TARGET" -c istio-proxy -- cat /var/local/wasm/new-filter.wasm) > /dev/null
do
   sleep 5; echo continuing to wait...
done

echo binary is loaded upon "$TARGET"

# STEP 4. Envoy won't reload .wasm if it changes; but it will if the XDS removes and restores the EnvoyFilter
kubectl delete -f "$FILTERFILE" > /dev/null 2> /dev/null || true
kubectl apply -f "$FILTERFILE"

echo ""
echo attached root_id \"${ROOT_ID}\" Wasm to workload $WORKLOAD
echo When $WASMFILE is started by Envoy, it will be configured with $CONFIG_VALUE

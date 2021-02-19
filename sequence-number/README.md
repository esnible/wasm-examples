
# Unique sequence number example in WebAssembly for Proxies

This example contains an Envoy filter that adds a header containing a unique sequence number
to all responses from a server.

This example is designed to be tested with the
[Istio Httpbin sample](/istio/istio/tree/release-1.9/samples/httpbin).
This repo uses a modified version of it that mounts a ConfigMap as a .wasm file on the sidecar.
To test this example, deploy Httpbin with

``` bash
kubectl apply -f ../deploy/httpbin-with-wasm-mount.yaml
```

I have only provided a C++ example of this filter.

## C++

To compile via the _Makefile_,

``` bash
docker run -v $PWD:/work -w /work wasmsdk:v2 /build_wasm.sh
```

At this point, you have _sequence-number.wasm_ which contains the compiled version.  To deploy:

``` bash
../deploy/update-wasm.sh sequence-number.wasm sequence-number
```

## Test

``` bash
kubectl exec -ti deploy/httpbin -c istio-proxy -- curl -v http://httpbin.default:8000/status/200
```

If it works, you'll see 

```
< sequence-number: 1
```

in the response headers.

If you issue the command again, the `1` will become `2`.

No matter how many simultaneous HTTP requests flow through the filter, 

To achieve this, the sequence number is kept in _shared data_ and the shared data is
updated atomically using `setSharedData("counter", std::to_string(counter), cas)`.

Testing this way doesn't put enough load on the server to let _setSharedData()_ fail with `WasmResult::CasMismatch`.  A simple way supply enough load is

``` bash
kubectl exec -it deploy/httpbin -c istio-proxy -- /bin/bash
for i in `seq 100`; do ( curl -v http://httpbin.default:8000/status/418 & ); done
```

If you fork off 100 requests while monitoring the sidecar with `kubectl logs deployment/httpbin -c istio-proxy -f` you should see an occasional

```
SequenceRoot::counter() setSharedData() FAILED.  wrSet is WasmResult::CasMismatch
```

When those occur the logic in the sample retries until the atomic counter update happens.

## Cleanup

``` bash
kubectl delete envoyfilter httpbin
kubectl delete configmap new-filter
kubectl delete -f ../deploy/httpbin-with-wasm-mount.yaml
```

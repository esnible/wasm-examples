
# Regex path replacement example in WebAssembly for Proxies

This example contains an Envoy filter that redirects incoming from `http://service/banana/X` to
`http://service/status/X`.

This example is designed to be tested with the
[Istio Httpbin sample](https://github.com/istio/istio/tree/release-1.9/samples/httpbin).
This repo uses a modified version of it that mounts a ConfigMap as a .wasm file on the sidecar.
To test this example, deploy Httpbin with

``` bash
kubectl apply -f ../deploy/httpbin-with-wasm-mount.yaml
```

To help you understand the differences between the C++, AssemblyScript, and Golang SDKs for writing
Istio filters I've implemented this filter in all three languages.

## C++

To compile via the _Makefile_,

``` bash
docker run -v $PWD:/work -w /work wasmsdk:v2 /build_wasm.sh
```

At this point, you have _regex-repl.wasm_ which contains the compiled version.  To deploy:

``` bash
../deploy/update-wasm.sh regex-repl.wasm regex-repl
```

## AssemblyScript

To compile,

``` bash
npm install
npm run asbuild:untouched -- regexp-repl/regex-repl.ts -b regexp-repl/regex-repl.ts.wasm -t regexp-repl/regex-repl.wat
```

Note: expect a `WARNING AS201`.

At this point, you have _regex-repl.ts.wasm_ which contains the compiled version.  To deploy:

``` bash
../deploy/update-wasm.sh regex-repl.ts.wasm regex-repl
```


## Golang

To compile,

``` bash
TETRATE_GOLANG_SDK_DIR=~/src/wasm/proxy-wasm-go-sdk # Or wherever you "git clone" d the SDK
docker run --workdir /tmp/proxy-wasm-go --volume $PWD:/tmp/proxy-wasm-go --volume $TETRATE_GOLANG_SDK_DIR/proxywasm:/tmp/proxy-wasm-go/proxywasm tinygo/tinygo:0.16.0 \
  tinygo build -o /tmp/proxy-wasm-go/regex-repl.go.wasm -scheduler=none -target=wasi /tmp/proxy-wasm-go/regex-repl.go
```

At this point, you have _regex-repl.go.wasm_ which contains the compiled version.  To deploy:

``` bash
../deploy/update-wasm.sh regex-repl.go.wasm regex-repl
```

## Test

``` bash
kubectl exec -ti deploy/httpbin -c istio-proxy -- curl -v http://httpbin.default:8000/banana/418
```

If it works, you'll see a teapot.  If it doesn't work, you'll get a 404.

## Cleanup

``` bash
kubectl delete envoyfilter httpbin
kubectl delete configmap new-filter
kubectl delete -f ../deploy/httpbin-with-wasm-mount.yaml
```

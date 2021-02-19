
# Sample WebAssembly filters for Istio

These examples should work with Istio 1.8, 1.9, and future 1.10 (current master).  These examples
are contributed as a companion to my IstioCon talk, [Extending Envoy with WASM from start to finish](https://events.istio.io/istiocon-2021/sessions/extending-envoy-with-wasm-from-start-to-finish/).

All of these examples can be tested without any 3rd party tools or cloud-specific Kubernetes storage.
Users might get better performance using another technique or using the [Wasm Remote Fetch](https://istio.io/latest/docs/ops/configuration/extensibility/wasm-module-distribution/) experimental feature
launched with Istio 1.9.  The ConfigMap approach is used here because it is the simplest.

These filters for have been tested with the C++, AssemblyScript, and Go
WebAssembly for Proxies SDKs.

- [C++ SDK](https://github.com/proxy-wasm/proxy-wasm-cpp-sdk) 
- [AssemblyScript (JavaScript) SDK](https://github.com/solo-io/proxy-runtime)
- [Go (Golang TinyGo) SDK](https://github.com/tetratelabs/proxy-wasm-go-sdk)

I am preparing to contribute these with an Apache license to one of the existing examples repos in
the [istio-ecosystem](https://github.com/istio-ecosystem) org.

## Preparing the language SDKs

To prepare the WebAssembly for Proxies C++ SDK:

``` bash
git clone git@github.com:proxy-wasm/proxy-wasm-cpp-sdk.git
cd proxy-wasm-cpp-sdk
# The next line takes 30+ minutes on MacBook (64 GB, 2.3 GHz 8-Core Intel Core i9)
docker build -t wasmsdk:v2 -f Dockerfile-sdk .
```

## regex path replacer

The [regexp replacer](regex-repl) example shows how to redirect the HTTP path of incoming requests to a server.

## sequence number

The [sequence number](sequence-number) example shows how to used &ldquo;shared data&rdquo; to coordinate multiple instances
of a filter.

## http-call

The [HTTP call](httpcall) example shows how to make outgoing HTTP calls, and modify the body of responses
based on the results of that call.
.
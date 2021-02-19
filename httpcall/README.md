
# External HTTP call example in WebAssembly for Proxies

This example contains an Envoy filter that makes an external HTTP call and uses the results
of that call in the response returned for the original invocation to a microservice.

This example is designed to be tested with the
[Istio Httpbin sample](https://github.com/istio/istio/tree/release-1.9/samples/httpbin).
This repo uses a modified version of it that mounts a ConfigMap as a .wasm file on the sidecar.
To test this example, deploy Httpbin with

``` bash
kubectl apply -f ../deploy/httpbin-with-wasm-mount.yaml
```

## C++

To compile via the _Makefile_,

``` bash
docker run -v $PWD:/work -w /work wasmsdk:v2 /build_wasm.sh
```

At this point, you have _httpcall-in-onresponse.wasm_ which contains the compiled version.  To deploy:

``` bash
../deploy/update-wasm.sh httpcall-in-onresponse.wasm http-call "outbound|80||example.com"
```

## AssemblyScript

A full AssemblyScript version isn't ready.  However, if you wish to try yourself, here is the
code to make an HTTP call:

``` JavaScript
  let cluster = "outbound|80||example.com"
  let callHeaders: Headers = [];
  callHeaders.push(new HeaderPair(String.UTF8.encode(":method"), String.UTF8.encode("GET")));
  callHeaders.push(new HeaderPair(String.UTF8.encode(":path"), String.UTF8.encode("/")));
  callHeaders.push(new HeaderPair(String.UTF8.encode(":authority"), String.UTF8.encode("example.com")));
```

Because of the nature of JavaScript/AssemblyScript, it was easy to create a callback without using `bind()`.

## Golang

A full TinyGo version isn't ready.

Create the `HttpCalloutCallBack` you'll
supply to proxywasm.DispatchHttpCall()` by creating a _closure_.

``` Go
  ...
  _, err := proxywasm.DispatchHttpCall(clusterName, hs, "", [][2]string{},
    5000, createMyHttpCallback(ctx))
  ...

func createMyHttpCallback(mhc *myHttpContext) proxywasm.HttpCalloutCallBack {
  return func(numHeaders, bodySize, numTrailers int) {
    mhc.httpCallResponseCallback(numHeaders, bodySize, numTrailers)
  }
}
```

The function _createMyHttpCallback()_ creates returns an HttpCalloutCallBack with access to a
pointer to your _HttpContext_ instance.

### Test

The Wasm environment is sandboxed and can't make HTTP calls to Envoy clusters it doesn't know about.
In an Istio environment that means in-cluster Services and external clustered that have an
Istio _ServiceEntry_.

When we deployed the plugin we configured it with `"outbound|80||example.com"` so we'll need Envoy to
know about that cluster.

``` bash
kubectl apply -f - <<EOF
apiVersion: networking.istio.io/v1alpha3
kind: ServiceEntry
metadata:
  name: example-com
spec:
  hosts:
  - example.com
  ports:
  - number: 80
    name: http
    protocol: HTTP
EOF
```

``` bash
kubectl exec -ti deploy/httpbin -c istio-proxy -- curl -v http://httpbin.default:8000/status/418
```

If it works, you WONT see a teapot.  Instead, you'll still see the `HTTP/1.1 418 Unknown` returned by
_Httpbin_ itself, but the ASCII teapot body will be replaced with something like

```
httpCall() yielded status 200 and a buffer of 1256 bytes.
```

This indicates the call to example.com returned a 200 OK and an HTTP body that was 1256 bytes long.

If you delete the Istio ServiceEntry, the filter will be blocked from contacting example.com:

``` bash
kubectl delete serviceentry example-com
kubectl exec -ti deploy/httpbin -c istio-proxy -- curl -v http://httpbin.default:8000/status/418
...
* Connected to httpbin.default (172.21.147.162) port 8000 (#0)
> GET /status/418 HTTP/1.1
...
< HTTP/1.1 500 Internal Server Error
...
HttpCall::onResponseHeaders() failed http call, wrSec is BadArgument
```

## Cleanup

``` bash
kubectl delete serviceentry example-com
kubectl delete envoyfilter httpbin
kubectl delete configmap new-filter
kubectl delete -f ../deploy/httpbin-with-wasm-mount.yaml
```

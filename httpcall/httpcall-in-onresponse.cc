/////////////////////////////////////////////////////////////////////////
// (c) Copyright 2020 IBM Corp. Licensed Materials - Property of IBM.
/////////////////////////////////////////////////////////////////////////

// See https://github.com/proxy-wasm/proxy-wasm-cpp-sdk

#include <string>
#include <unordered_map>

#include "proxy_wasm_intrinsics.h"

  
// "root context" class handles lifecycle of the extension
class HttpCallRoot : public RootContext {
public:
  explicit HttpCallRoot(uint32_t id, std::string_view root_id)
      : RootContext(id, root_id), strId_(std::to_string(id)) {
    logWarn("HttpCallRoot(context_id:" + strId_ + ", \"" + std::string(root_id) + "\")");
  }

  // Called once when the VM loads and once when each hook loads and whenever
  // configuration changes. Returns false if the configuration is invalid.
  virtual bool onConfigure(size_t configuration_size) override;

  // Called when the VM is being torn down.
  virtual bool onDone() override;

  // Called to indicate that no more calls will come and this context is being deleted.
  virtual void onDelete() override;

  std::string_view getCluster() { return cluster_; }
  std::string& getPath() { return path_; };

private:
  std::string strId_;
  std::string cluster_;
  std::string path_;
};

// "context" class handles lifecycle of an HTTP request/response
class HttpCall : public Context {
public:
  explicit HttpCall(uint32_t id, RootContext* root) : Context(id, root),
    strId_(std::to_string(id)),
    // Create an HttpCallCallback that points to this instance's callback-handling member function
    boundCallback_(std::bind(&HttpCall::myHttpCallback, this,
          std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)) {
  }

  FilterHeadersStatus onResponseHeaders(uint32_t headers, bool end_of_stream) override;

  HttpCallRoot* myRoot() { return (HttpCallRoot *) root(); }
  void myHttpCallback(uint32_t headerCount, size_t body_size, uint32_t trailerCount);

private:
  std::string strId_;

  // Save the HTTP status originally provided to onResponseHeaders()
  uint32_t status_;

  // A HttpCallCallback that points to this instance
  HttpCallCallback boundCallback_;
};

static RegisterContextFactory register_ExampleContext(CONTEXT_FACTORY(HttpCall),
   ROOT_FACTORY(HttpCallRoot), 
   "http-call");

std::string& boolToString(bool b);

bool HttpCallRoot::onConfigure(size_t configuration_size) {
  logWarn("context_id:" + strId_ + " HttpCallRoot::onConfigure(" + std::to_string(configuration_size) + ")");

  WasmDataPtr configuration_data = getBufferBytes(WasmBufferType::PluginConfiguration, 0, configuration_size);
  std::string configuration = configuration_data->toString();

  // TODO read the config as JSON.  Parsing JSON isn't built-in.  Plugins can follow 
  // https://github.com/istio-ecosystem/wasm-extensions/tree/master/extensions/basic_auth
  // as an example.  As a work around, we will treat the whole string as the Envoy cluster,
  // and always fetch the root / PATH from that cluster.
  cluster_ = configuration;
  path_ = "/";

  return true;
}

bool HttpCallRoot::onDone() { 
  logWarn("context id: " + std::to_string(id()) + ": HttpCallRoot::onDone()");
  return RootContext::onDone();
}

void HttpCallRoot::onDelete() { 
  logWarn("context id: " + std::to_string(id()) + ": HttpCallRoot::onDelete()");
  RootContext::onDelete();
}

// ==================

FilterHeadersStatus HttpCall::onResponseHeaders(uint32_t headerCount, bool end_of_stream)  {

  // This logs something like "wasm log: context id: 2: HttpCall::onResponseHeaders(9, false)"
  logWarn("context id: " + std::to_string(id()) + ": HttpCall::onResponseHeaders(" + std::to_string(headerCount) + ", " + boolToString(end_of_stream) + ")");

  // onResponseHeaders sees the HTTP headers plus :status.  Save that status, we will use it later.
  WasmDataPtr wdpGhmv = getHeaderMapValue(WasmHeaderMapType::ResponseHeaders, ":status");
  std::string sStatus = wdpGhmv->toString();
  status_ = std::stoi(sStatus);

  auto cluster = this->myRoot()->getCluster();
  auto path = this->myRoot()->getPath();
  logWarn("context id: " + std::to_string(id()) + ": HttpCall::onResponseHeaders() will GET "
    + path + " from Envoy cluster " + std::string(cluster));

    std::vector<std::pair<std::string, std::string>> callHeaders;
    callHeaders.push_back(std::pair<std::string, std::string>(":method", "GET"));
    callHeaders.push_back(std::pair<std::string, std::string>(":path", path));
    callHeaders.push_back(std::pair<std::string, std::string>(":authority", "example.com"));

    WasmResult wr = this->root()->httpCall(
      cluster, // std::string_view "uri" (actually Envoy cluster)
      callHeaders, // HeaderStringPairs &requestHeaders
      "", // std::string_view request_body; unused for GET
      HeaderStringPairs(), // HeaderStringPairs &requestTrailers; none
      5000, // unit32_t timeout, 5 seconds
      boundCallback_);

    if (wr != WasmResult::Ok) {
      logWarn("HttpCall::onResponseHeaders() failed http call, wrSec is " + toString(wr));

      sendLocalResponse(500, "Failed/WASM", 
      "HttpCall::onResponseHeaders() failed http call, wrSec is " + toString(wr) + "\n", 
      HeaderStringPairs());
    } else {
      logWarn("HttpCall::onResponseHeaders() succeeded http call");
    }

    return FilterHeadersStatus::StopIteration;
}

// http callback: called when there's a response. if the request failed, headers will be 0
void HttpCall::myHttpCallback(uint32_t headerCount, size_t bodySize, uint32_t trailerCount) {
  // Note: this isn't really a warning, but we set it high to see this in the logs during testing.
  logWarn("context id: " + std::to_string(id()) + ": HttpCall::myHttpCallback(" + 
            std::to_string(headerCount) + ", " +
            std::to_string(bodySize) + ", " +
            std::to_string(trailerCount) + ")");

  std::string status = "<none>";
  if (headerCount > 0) {
    status = getHeaderMapValue(WasmHeaderMapType::HttpCallResponseHeaders, ":status")->toString();
    logWarn("context id: " + std::to_string(id()) + ": HttpCall::myHttpCallback :status is " + status);
  } else {
    logWarn("context id: " + std::to_string(id()) + ": HttpCall::myHttpCallback no :status");
  }

  // To inspect the body fetched by the HttpCall() use
  // WasmDataPtr wdpBody = getBufferBytes(WasmBufferType::HttpCallResponseBody, 0, body_size);

  // YOU MUST CALL setEffectiveContext() BEFORE calling continueResponse(), sendLocalResponse() and friends!
  WasmResult wrSec = setEffectiveContext();
  if (wrSec == WasmResult::Ok) {
    logWarn("context_id:" + strId_ + " HttpCall::myHttpCallback() setEffectiveContext() succeeded.");
  } else {
    logWarn("context_id:" + strId_ + " HttpCall::myHttpCallback() setEffectiveContext() FAILED.  wrSec is " + toString(wrSec));
  }

  auto newBody = "httpCall() yielded status " + status + " and a buffer of " + std::to_string(bodySize) + " bytes.\n";
  WasmResult wrSlr = sendLocalResponse(this->status_, 
    // details
    "", 
    newBody, 
    // Additional headers: none
    HeaderStringPairs());
  if (wrSlr == WasmResult::Ok) {
    logWarn("context_id:" + strId_ + " HttpCall::myHttpCallback() sendLocalResponse() succeeded.");
  } else {
    logWarn("context_id:" + strId_ + " HttpCall::myHttpCallback() sendLocalResponse() FAILED.  wrSlr is " + toString(wrSlr));
  }
  // Because I changed the response body, I called sendLocalResponse().  If I was just adding a header,
  // I would have called continueResponse().
}

// ==================

std::string& boolToString(bool b)
{
  static std::string t = "true";
  static std::string f = "false";

  return b ? t : f;
}

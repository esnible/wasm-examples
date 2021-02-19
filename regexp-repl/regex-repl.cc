/////////////////////////////////////////////////////////////////////////
// (c) Copyright 2021 IBM Corp. Licensed Materials - Property of IBM.
/////////////////////////////////////////////////////////////////////////

// See https://github.com/proxy-wasm/proxy-wasm-cpp-sdk

#include <string>
#include <unordered_map>
#include <regex>

#include "proxy_wasm_intrinsics.h"

// "root context" class handles lifecycle of the extension
class RegexpReplRoot : public RootContext {
public:
  explicit RegexpReplRoot(uint32_t id, std::string_view root_id)
      : RootContext(id, root_id), strId_(std::to_string(id)) {
    // You probably want to warn at a lower level!
    logWarn("RegexpReplRoot(context_id:" + strId_ + ", \"" + std::string(root_id) + "\")");
  }

  // Called once when the VM loads and once when each hook loads and whenever
  // configuration changes. Returns false if the configuration is invalid.
  virtual bool onConfigure(size_t configuration_size) override;

  // Example showing regex pattern/replace
  // Regular expression, possibly with a capture group, e.g. "banana/([0-9]*)";
  const char *getPattern() { return pattern_; } 
  // A replacement, possibly referencing capture groups, e.g. "status/$1";
  const char *getReplaceWith() {return replaceWith_; }

private:
  std::string strId_;
  const char *pattern_ = NULL;
  const char *replaceWith_ = NULL;
};

// "context" class handles lifecycle of an HTTP request/response
class RegexpRepl : public Context {
public:
  explicit RegexpRepl(uint32_t id, RootContext* root) : Context(id, root), strId_(std::to_string(id)) {
    logWarn("context id: " + strId_ + ": RegexpRepl(...)");
  }

  FilterHeadersStatus onRequestHeaders(uint32_t, bool) override;
  RegexpReplRoot* myRoot() { return (RegexpReplRoot *) this->root(); }

private:
  std::string strId_;
};

// This static constructor registers your classes with the VM at startup
static RegisterContextFactory register_ExampleContext(
  CONTEXT_FACTORY(RegexpRepl),
  ROOT_FACTORY(RegexpReplRoot), 
  "regex-repl");

class LogAtStartup {
public:
  explicit LogAtStartup() {
    logWarn("regex-repl.cc will log this when loaded.");
  }
};
static LogAtStartup dummy;

std::string& boolToString(bool b);

// Called per Envoy worker thread when the VM loads.
bool RegexpReplRoot::onConfigure(size_t configuration_size) {
  logWarn("context_id:" + strId_ + " RegexpReplRoot::onConfigure(" + std::to_string(configuration_size) + ")");

  WasmDataPtr configuration_data = getBufferBytes(WasmBufferType::PluginConfiguration, 0, configuration_size);
  std::string configuration = configuration_data->toString();
  logWarn("ignoring configuration ->" + configuration);

  // TODO read the config as JSON.  Parsing JSON isn't built-in.  Plugins can follow 
  // https://github.com/istio-ecosystem/wasm-extensions/tree/master/extensions/basic_auth
  // as an example.
  pattern_ = "banana/([0-9]*)";
  replaceWith_ = "status/$1";

  return true;
}

// ==================

// Do the actual path rewrite here
FilterHeadersStatus RegexpRepl::onRequestHeaders(uint32_t header_count, bool end_of_stream) {
  logWarn("context_id:" + strId_ + " RegexpRepl::onRequestHeaders(" + std::to_string(header_count) + ", " + boolToString(end_of_stream) + ")");

   // Envoy's "headers" include HTTP headers **and** colon-prefixed Envoy HTTP properties.
   // For example:
   // :authority: httpbin.default:8000
   // :path: /status/200
   // :method: GET
   // user-agent: curl/7.58.0
   // accept: */*
   // x-forwarded-proto: http
   // x-request-id: 1be9883a-3ac3-4dba-9991-1a1301f51ace

  // TODO modify the request headers using the configuration
  char const *key = ":path";
  const char *pattern = myRoot()->getPattern(); // e.g. "banana/([0-9]*)";
  const char *replaceWith = myRoot()->getReplaceWith(); // e.g. "status/$1";

  WasmDataPtr wdpGhmv = getRequestHeader(key);
  std::string sValue = wdpGhmv->toString();

  std::regex re(pattern);
  std::string result = std::regex_replace(sValue, re, replaceWith);
  WasmResult wrRrh = replaceRequestHeader(key, result);
  if (wrRrh == WasmResult::Ok) {
    logWarn("context_id:" + strId_ + " RegexpRepl::onRequestHeaders() replaced " + sValue + " with " + result);
  } else {
    logWarn("context_id:" + strId_ + " RegexpRepl::onRequestHeaders() replaceRequestHeader() FAILED.  wrRrh is " + toString(wrRrh));
  }

  return FilterHeadersStatus::Continue;
}

// ==================

std::string& boolToString(bool b)
{
  static std::string t = "true";
  static std::string f = "false";

  return b ? t : f;
}

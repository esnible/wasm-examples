/////////////////////////////////////////////////////////////////////////
// (c) Copyright 2021 IBM Corp. Licensed Materials - Property of IBM.
/////////////////////////////////////////////////////////////////////////

// See https://github.com/proxy-wasm/proxy-wasm-cpp-sdk

#include <string>
#include <unordered_map>
#include <sstream>

#include "proxy_wasm_intrinsics.h"

// "root context" class handles lifecycle of the extension
class SequenceRoot : public RootContext {
public:
  explicit SequenceRoot(uint32_t id, std::string_view root_id)
      : RootContext(id, root_id), strId_(std::to_string(id)) {
    logWarn("SequenceRoot(context_id:" + strId_ + ", \"" + std::string(root_id) + "\")");
  }

  // Called when each hook loads. Returns false if the configuration is invalid.
  virtual bool onStart(size_t vm_configuration_size) override;

  // Called when the VM is being torn down.
  virtual bool onDone() override;

  // Called to indicate that no more calls will come and this context is being deleted.
  virtual void onDelete() override;

  // counter() demonstrates a sequence number that increases with each HTTP request.
  // The sequence number output will be unique, no matter how many Envoy worker threads
  // are processing the data.
  int counter() {
    int counter;
    WasmResult wrSet;

    // Loop until this thread atomically updated the shared counter
    do {
      // "cas" is a Compare And Swap value.  See
      // https://en.wikipedia.org/wiki/Compare-and-swap
      uint32_t cas = -1;

      WasmDataPtr wdpCount;
      WasmResult wrGet = getSharedData("counter", &wdpCount, &cas);
      if (wrGet != WasmResult::Ok) {
        // If wrGet is WasmResult::CasMismatch it isn't an error, so we shouldn't be logging
        // at WARN level.  We are logging at this high level to demonstrate that this actually
        // happens.
        logWarn("context_id:" + strId_ + " SequenceRoot::counter() getSharedData() FAILED.  wrGet is " + toString(wrGet));
        return -1;
      }

      std::string sCount = wdpCount->toString();
      counter = std::stoi(sCount);
      
      counter++;

      wrSet = setSharedData("counter", std::to_string(counter), cas);
      if (wrSet != WasmResult::Ok) {
        logWarn("context_id:" + strId_ + " SequenceRoot::counter() setSharedData() FAILED.  wrSet is " + toString(wrSet));
      }
    } while (wrSet == WasmResult::CasMismatch);

    return counter;
  }

private:
  std::string strId_;
};

// "context" class handles lifecycle of an HTTP request/response
class Sequence : public Context {
public:
  explicit Sequence(uint32_t id, RootContext* root) : Context(id, root), strId_(std::to_string(id)) {
    logWarn("context id: " + strId_ + ": Sequence(...)");
  }

  virtual FilterHeadersStatus onResponseHeaders(uint32_t, bool) override;

  SequenceRoot* myRoot() { return (SequenceRoot *) root(); }

private:
  std::string strId_;
};

// This static constructor registers your classes with the VM at startup
static RegisterContextFactory register_ExampleContext(CONTEXT_FACTORY(Sequence), 
  ROOT_FACTORY(SequenceRoot), 
  "sequence-number");

std::string& boolToString(bool b);

// Called when the filter starts.  Expect this to be called conncurrency+2 times.  Every time
// we set the shared data to 0; we only need to do that once.
bool SequenceRoot::onStart(size_t vm_configuration_size) {
  logWarn("context_id:" + strId_ + " SequenceRoot::onStart(" + std::to_string(vm_configuration_size) + ")");

  WasmResult wrSet = setSharedData("counter", "0");
  if (wrSet != WasmResult::Ok) {
    logWarn("context_id:" + strId_ + " SequenceRoot::onStart() setSharedData() FAILED.  wrSet is " + toString(wrSet));
  }

  return true;
}

// Called when the VM is being torn down.
bool SequenceRoot::onDone() {
  logWarn("context_id:" + strId_ + " SequenceRoot::onDone()");
  return true;
}

// Called to indicate that no more calls will come and this context is being
// deleted.
void SequenceRoot::onDelete() {
  logWarn("context_id:" + strId_ + " SequenceRoot::onDelete()");
}

// ==================

FilterHeadersStatus Sequence::onResponseHeaders(uint32_t header_count, bool end_of_stream) {
  logWarn("context_id:" + strId_ + " Sequence::onResponseHeaders(" + std::to_string(header_count) + ", " + boolToString(end_of_stream) + ")");

  char buffer[256];
  sprintf(buffer, "%d", this->myRoot()->counter());
  WasmResult wr = addResponseHeader("sequence-number", buffer);
  if (wr != WasmResult::Ok) {
    logWarn("context_id:" + strId_ + " Sequence::onResponseHeaders() addResponseHeader() FAILED.  wr is " + toString(wr));
  }

  return Context::onResponseHeaders(header_count, end_of_stream);
}

// ==================

std::string& boolToString(bool b)
{
  static std::string t = "true";
  static std::string f = "false";

  return b ? t : f;
}

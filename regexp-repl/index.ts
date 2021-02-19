/////////////////////////////////////////////////////////////////////////
// (c) Copyright 2021 IBM Corp. Licensed Materials - Property of IBM.
/////////////////////////////////////////////////////////////////////////


// See https://github.com/solo-io/proxy-runtime
// See https://github.com/envoyproxy/envoy/blob/master/include/envoy/http/filter.h

// TODO Why does Visual Studio Code object to the following line; note the directory doesn't exist.
export * from "@solo-io/proxy-runtime/proxy"; // this exports the required functions for the proxy to interact with us.
import { RootContext, Context, registerRootContext, 
  FilterHeadersStatusValues, 
  stream_context, 
  LogLevelValues, log } from "@solo-io/proxy-runtime";

class RegexpReplRoot extends RootContext {

  createContext(context_id: u32): Context {
    log(LogLevelValues.warn, "RegexpReplRoot.createContext(context_id:" + this.context_id.toString() + ")");
    return new RegexpRepl(context_id, this);
  }

  // (just for logging)
  onDelete(): void {
    log(LogLevelValues.warn, "context id: " + this.context_id.toString() + ": RegexpReplRoot.onDelete()");
    super.onDelete();
  }
}

class RegexpRepl extends Context {

  constructor(context_id: u32, root_context: RegexpReplRoot){
    super(context_id, root_context);
  }

  // onRequestHeaders sees the regular headers plus :authority, :path, and :method
  onRequestHeaders(header_count: u32, end_of_stream: bool): FilterHeadersStatusValues {
    log(LogLevelValues.warn, "context id: " + this.context_id.toString() + ": RegexpRepl.onRequestHeaders(" + header_count.toString() + ", " + end_of_stream.toString() + ")");

    // TODO modify the request headers using the configuration
     let key = ":path";
     let pattern = "banana/([0-9]*)";
     let replaceWith = "status/$1";

     let sValue = stream_context.headers.request.get(key);

    let re = new RegExp(pattern);
    // The next line doesn't work, because AssemblyScript doesn't yet support RegExp
    // let result = sValue.replace(re, replaceWith);
    // So instead we do something simpler.
    let result = sValue.replace("/banana/", "/status/")
    stream_context.headers.request.replace(key, result);
    // Unlike the C++ API, the AssemblyScript API does not provide a result code

    return super.onRequestHeaders(header_count, end_of_stream)
  }

  onDone(): bool { 
    log(LogLevelValues.warn, "context id: " + this.context_id.toString() + ": RegexpRepl.onDone()");
    return super.onDone();
  }

  onDelete(): void { 
    log(LogLevelValues.warn, "context id: " + this.context_id.toString() + ": RegexpRepl.onDelete()");
    super.onDelete();
  }
}

registerRootContext((context_id: u32) => { return new RegexpReplRoot(context_id); }, "experiment-1.9");

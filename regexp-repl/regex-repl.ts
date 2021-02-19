/////////////////////////////////////////////////////////////////////////
// (c) Copyright 2021 IBM Corp. Licensed Materials - Property of IBM.
/////////////////////////////////////////////////////////////////////////


// See https://github.com/solo-io/proxy-runtime

export * from "@solo-io/proxy-runtime/proxy"; // this exports the required functions for the proxy to interact with us.
import { RootContext, Context, registerRootContext, 
  FilterHeadersStatusValues, 
  stream_context, 
  LogLevelValues, log } from "@solo-io/proxy-runtime";

// "root context" class handles lifecycle of the extension
class RegexpReplRoot extends RootContext {

  createContext(context_id: u32): Context {
    log(LogLevelValues.warn, "RegexpReplRoot.createContext(context_id:" + this.context_id.toString() + ")");
    log(LogLevelValues.warn, "RegexpReplRoot.createContext() called with pattern set to " + this.getPattern());
    return new RegexpRepl(context_id, this);
  }

  // You may not need an onConfigure() override, as the AssemblyScript SDK keeps the config in memory.

  // (just for logging)
  onDelete(): void {
    log(LogLevelValues.warn, "context id: " + this.context_id.toString() + ": RegexpReplRoot.onDelete()");
    super.onDelete();
  }

  // Example showing regex pattern/replace
  // Regular expression, possibly with a capture group
  getPattern(): string { return "banana/([0-9]*)"; } 
  // A replacement, possibly referencing capture groups
  getReplaceWith(): string {return "status/$1"; }
}

// "context" class handles lifecycle of an HTTP request/response
class RegexpRepl extends Context {

  constructor(context_id: u32, root_context: RegexpReplRoot){
    super(context_id, root_context);

    log(LogLevelValues.warn, "context id: " + this.context_id.toString() + ": RegexpRepl constructor(" + context_id.toString() + ", " + "..." + ")");
  }

  myRoot(): RegexpReplRoot { return (this.root_context as RegexpReplRoot); }

  // onRequestHeaders sees the regular headers plus :authority, :path, and :method
  onRequestHeaders(header_count: u32, end_of_stream: bool): FilterHeadersStatusValues {
    log(LogLevelValues.warn, "context id: " + this.context_id.toString() + ": RegexpRepl.onRequestHeaders(" + header_count.toString() + ", " + end_of_stream.toString() + ")");

    let key = ":path";
    let sValue = stream_context.headers.request.get(key);

    let pattern = this.myRoot().getPattern();
    let replaceWith = this.myRoot().getReplaceWith();
  
    // The next lines don't work, because AssemblyScript doesn't yet support RegExp
    // let re = new RegExp(pattern);
    // let result = sValue.replace(re, replaceWith);
    // So instead we do something simpler for the demo.
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

registerRootContext((context_id: u32) => { return new RegexpReplRoot(context_id); }, "regex-repl");

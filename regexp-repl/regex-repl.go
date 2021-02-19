/////////////////////////////////////////////////////////////////////////
// (c) Copyright 2021 IBM Corp. Licensed Materials - Property of IBM.
/////////////////////////////////////////////////////////////////////////

package main

import (
	"regexp"

	"github.com/tetratelabs/proxy-wasm-go-sdk/proxywasm"
	"github.com/tetratelabs/proxy-wasm-go-sdk/proxywasm/types"
)

// "root context" interface handles lifecycle of the extension
type rootRegex struct {
	// embed the default root context to pick up methods
	proxywasm.DefaultRootContext
	contextID uint32

	pattern     string
	replaceWith string
}

// "context" class handles lifecycle of an HTTP request/response
type httpRegex struct {
	// embed the default HTTP context to pick up methods
	proxywasm.DefaultHttpContext
	contextID   uint32
	rootContext *rootRegex
}

// Ensure our filter supports the correct interface
var _ proxywasm.RootContext = &rootRegex{}
var _ proxywasm.HttpContext = &httpRegex{}

func main() {
	proxywasm.LogWarnf("regex/main.go main() REACHED")

	proxywasm.SetNewRootContext(newRegexRootContext)
	proxywasm.SetNewHttpContext(newRegexHttpContext)
}

// (This function only exists to give to proxywasm.SetNewRootContext() to create an instance)
func newRegexRootContext(contextID uint32) proxywasm.RootContext {
	return &rootRegex{contextID: contextID}
}

// (This function only exists to give to proxywasm.SetNewHttpContext() to create an instance)
func newRegexHttpContext(rootContextID, contextID uint32) proxywasm.HttpContext {
	// The Go SDK gives HTTP contexts the ID of their root context.  Look up the actual context.
	rootCtx, err := proxywasm.GetRootContextByID(rootContextID)
	if err != nil {
		proxywasm.LogErrorf("unable to get root context: %v", err)
	}
	myRootRegex := rootCtx.(*rootRegex)

	return &httpRegex{contextID: contextID, rootContext: myRootRegex}
}

// override
func (ctx *rootRegex) OnPluginStart(pluginCfgSize int) bool {
	data, err := proxywasm.GetPluginConfiguration(pluginCfgSize)
	if err != nil {
		proxywasm.LogWarnf("failed read plug-in config: %v", err)
	}

	proxywasm.LogWarnf("%d plug-in config: %s\n", ctx.contextID, string(data))

	// TODO Parse the data as JSON, to extract pattern and replaceWith keys, instead of just setting them.
	ctx.pattern = "banana/([0-9]*)"
	ctx.replaceWith = "status/$1"

	return true
}

// Only here for logging
func (ctx *rootRegex) OnVMDone() bool {
	proxywasm.LogWarnf("%d rootRegex.OnVMDone()", ctx.contextID)
	return true
}

func (ctx *httpRegex) OnHttpRequestHeaders(numHeaders int, endOfStream bool) types.Action {
	proxywasm.LogWarnf("%d httpRegex.OnHttpRequestHeaders(%d, %t)", ctx.contextID, numHeaders, endOfStream)

	re := regexp.MustCompile(ctx.rootContext.pattern)
	replaceWith := ctx.rootContext.replaceWith

	s, err := proxywasm.GetHttpRequestHeader(":path")
	if err != nil {
		proxywasm.LogWarnf("Could not get request header: %v", err)
	} else {
		result := re.ReplaceAllString(s, replaceWith)
		err = proxywasm.SetHttpRequestHeader(":path", result)
		if err != nil {
			proxywasm.LogWarnf("Could not set request header to %q: %v", result, err)
		}
	}

	return types.ActionContinue
}

func (ctx *httpRegex) OnHttpStreamDone() {
	proxywasm.LogWarnf("%d OnHttpStreamDone", ctx.contextID)
}

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <node.h>
#include <v8.h>
#include <node_buffer.h>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

#include "security_context.h"
#include "security_buffer_descriptor.h"

#ifndef ARRAY_SIZE
# define ARRAY_SIZE(a) (sizeof((a)) / sizeof((a)[0]))
#endif

static LPSTR DisplaySECError(DWORD ErrCode);

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// UV Lib callbacks
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
static void Process(uv_work_t* work_req) {
  // Grab the worker
  Worker *worker = static_cast<Worker*>(work_req->data);
  // Execute the worker code
  worker->execute(worker);
}

static void After(uv_work_t* work_req) {
  // Grab the scope of the call from Node
  Nan::HandleScope scope;

  // Get the worker reference
  Worker *worker = static_cast<Worker*>(work_req->data);

  // If we have an error
  if(worker->error) {
    Local<Value> err = v8::Exception::Error(Nan::New<String>(worker->error_message).ToLocalChecked());
    Local<Object> obj = err->ToObject();
    obj->Set(Nan::New<String>("code").ToLocalChecked(), Nan::New<Int32>(worker->error_code));
    Local<Value> info[2] = { err, Nan::Null() };
    // Execute the error
    Nan::TryCatch try_catch;

    // Call the callback
    worker->callback->Call(ARRAY_SIZE(info), info);

    // If we have an exception handle it as a fatalexception
    if (try_catch.HasCaught()) {
      Nan::FatalException(try_catch);
    }
  } else {
    // // Map the data
    Local<Value> result = worker->mapper(worker);
    // Set up the callback with a null first
    Local<Value> info[2] = { Nan::Null(), result};
    // Wrap the callback function call in a TryCatch so that we can call
    // node's FatalException afterwards. This makes it possible to catch
    // the exception from JavaScript land using the
    // process.on('uncaughtException') event.
    Nan::TryCatch try_catch;

    // Call the callback
    worker->callback->Call(ARRAY_SIZE(info), info);

    // If we have an exception handle it as a fatalexception
    if (try_catch.HasCaught()) {
      Nan::FatalException(try_catch);
    }
  }

  // Clean up the memory
  delete worker->callback;
  delete worker;
}

Nan::Persistent<FunctionTemplate> SecurityContext::constructor_template;

SecurityContext::SecurityContext() : Nan::ObjectWrap() {
}

SecurityContext::~SecurityContext() {
  if(this->hasContext) {
    _sspi_DeleteSecurityContext(&this->m_Context);
  }
}

NAN_METHOD(SecurityContext::New) {
  PSecurityFunctionTable pSecurityInterface = NULL;
  DWORD dwNumOfPkgs;
  SECURITY_STATUS status;

  // Create code object
  SecurityContext *security_obj = new SecurityContext();
  // Get security table interface
  pSecurityInterface = _ssip_InitSecurityInterface();
  // Call the security interface
  status = (*pSecurityInterface->EnumerateSecurityPackages)(
                                                    &dwNumOfPkgs,
                                                    &security_obj->m_PkgInfo);
  if(status != SEC_E_OK) {
    printf(TEXT("Failed in retrieving security packages, Error: %x"), GetLastError());
    return Nan::ThrowError("Failed in retrieving security packages");
  }

  // Wrap it
  security_obj->Wrap(info.This());
  // Return the object
  info.GetReturnValue().Set(info.This());
}

//
//  Async InitializeContext
//
typedef struct SecurityContextStaticInitializeCall {
  char *service_principal_name_str;
  char *decoded_input_str;
  int decoded_input_str_length;
  SecurityContext *context;
} SecurityContextStaticInitializeCall;

static void _initializeContext(Worker *worker) {
  // Status of operation
  SECURITY_STATUS status;
  BYTE *out_bound_data_str = NULL;
  SecurityContextStaticInitializeCall *call = (SecurityContextStaticInitializeCall *)worker->parameters;

  // Structures used for c calls
  SecBufferDesc ibd, obd;
  SecBuffer ib, ob;

  //
  // Prepare data structure for returned data from SSPI
  ob.BufferType = SECBUFFER_TOKEN;
  ob.cbBuffer = call->context->m_PkgInfo->cbMaxToken;
  // Allocate space for return data
  out_bound_data_str = new BYTE[ob.cbBuffer + sizeof(DWORD)];
  ob.pvBuffer = out_bound_data_str;
  // prepare buffer description
  obd.cBuffers  = 1;
  obd.ulVersion = SECBUFFER_VERSION;
  obd.pBuffers  = &ob;

  //
  // Prepare the data we are passing to the SSPI method
  if(call->decoded_input_str_length > 0) {
    ib.BufferType = SECBUFFER_TOKEN;
    ib.cbBuffer   = call->decoded_input_str_length;
    ib.pvBuffer   = call->decoded_input_str;
    // prepare buffer description
    ibd.cBuffers  = 1;
    ibd.ulVersion = SECBUFFER_VERSION;
    ibd.pBuffers  = &ib;
  }

  // Perform initialization step
  status = _sspi_initializeSecurityContext(
      &call->context->security_credentials->m_Credentials
    , NULL
    , const_cast<TCHAR*>(call->service_principal_name_str)
    , 0x02  // MUTUAL
    , 0
    , 0     // Network
    , call->decoded_input_str_length > 0 ? &ibd : NULL
    , 0
    , &call->context->m_Context
    , &obd
    , &call->context->CtxtAttr
    , &call->context->Expiration
  );

  // If we have a ok or continue let's prepare the result
  if(status == SEC_E_OK
    || status == SEC_I_COMPLETE_NEEDED
    || status == SEC_I_CONTINUE_NEEDED
    || status == SEC_I_COMPLETE_AND_CONTINUE
  ) {
    call->context->hasContext = true;
    call->context->payload = base64_encode((const unsigned char *)ob.pvBuffer, ob.cbBuffer);

    // Set the context
    worker->return_code = status;
    worker->return_value = call->context;
  } else if(status == SEC_E_INSUFFICIENT_MEMORY) {
    worker->error = TRUE;
    worker->error_code = status;
    worker->error_message = "SEC_E_INSUFFICIENT_MEMORY There is not enough memory available to complete the requested action.";
  } else if(status == SEC_E_INTERNAL_ERROR) {
    worker->error = TRUE;
    worker->error_code = status;
    worker->error_message = "SEC_E_INTERNAL_ERROR An error occurred that did not map to an SSPI error code.";
  } else if(status == SEC_E_INVALID_HANDLE) {
    worker->error = TRUE;
    worker->error_code = status;
    worker->error_message = "SEC_E_INVALID_HANDLE The handle passed to the function is not valid.";
  } else if(status == SEC_E_INVALID_TOKEN) {
    worker->error = TRUE;
    worker->error_code = status;
    worker->error_message = "SEC_E_INVALID_TOKEN The error is due to a malformed input token, such as a token corrupted in transit, a token of incorrect size, or a token passed into the wrong security package. Passing a token to the wrong package can happen if the client and server did not negotiate the proper security package.";
  } else if(status == SEC_E_LOGON_DENIED) {
    worker->error = TRUE;
    worker->error_code = status;
    worker->error_message = "SEC_E_LOGON_DENIED The logon failed.";
  } else if(status == SEC_E_NO_AUTHENTICATING_AUTHORITY) {
    worker->error = TRUE;
    worker->error_code = status;
    worker->error_message = "SEC_E_NO_AUTHENTICATING_AUTHORITY No authority could be contacted for authentication. The domain name of the authenticating party could be wrong, the domain could be unreachable, or there might have been a trust relationship failure.";
  } else if(status == SEC_E_NO_CREDENTIALS) {
    worker->error = TRUE;
    worker->error_code = status;
    worker->error_message = "SEC_E_NO_CREDENTIALS No credentials are available in the security package.";
  } else if(status == SEC_E_TARGET_UNKNOWN) {
    worker->error = TRUE;
    worker->error_code = status;
    worker->error_message = "SEC_E_TARGET_UNKNOWN The target was not recognized.";
  } else if(status == SEC_E_UNSUPPORTED_FUNCTION) {
    worker->error = TRUE;
    worker->error_code = status;
    worker->error_message = "SEC_E_UNSUPPORTED_FUNCTION A context attribute flag that is not valid (ISC_REQ_DELEGATE or ISC_REQ_PROMPT_FOR_CREDS) was specified in the fContextReq parameter.";
  } else if(status == SEC_E_WRONG_PRINCIPAL) {
    worker->error = TRUE;
    worker->error_code = status;
    worker->error_message = "SEC_E_WRONG_PRINCIPAL The principal that received the authentication request is not the same as the one passed into the pszTargetName parameter. This indicates a failure in mutual authentication.";
  } else {
    worker->error = TRUE;
    worker->error_code = status;
    worker->error_message = DisplaySECError(status);
  }

  // Clean up data
  if(call->decoded_input_str != NULL) free(call->decoded_input_str);
  if(call->service_principal_name_str != NULL) free(call->service_principal_name_str);
}

static Local<Value> _map_initializeContext(Worker *worker) {
  // Unwrap the security context
  SecurityContext *context = (SecurityContext *)worker->return_value;
  // Return the value
  return context->handle();
}

NAN_METHOD(SecurityContext::InitializeContext) {
  char *service_principal_name_str = NULL, *input_str = NULL, *decoded_input_str = NULL;
  int decoded_input_str_length = NULL;
  // Store reference to security credentials
  SecurityCredentials *security_credentials = NULL;

  // We need 3 parameters
  if(info.Length() != 4)
    return Nan::ThrowError("Initialize must be called with [credential:SecurityCredential, servicePrincipalName:string, input:string, callback:function]");

  // First parameter must be an instance of SecurityCredentials
  if(!SecurityCredentials::HasInstance(info[0]))
    return Nan::ThrowError("First parameter for Initialize must be an instance of SecurityCredentials");

  // Second parameter must be a string
  if(!info[1]->IsString())
    return Nan::ThrowError("Second parameter for Initialize must be a string");

  // Third parameter must be a base64 encoded string
  if(!info[2]->IsString())
    return Nan::ThrowError("Second parameter for Initialize must be a string");

  // Third parameter must be a callback
  if(!info[3]->IsFunction())
    return Nan::ThrowError("Third parameter for Initialize must be a callback function");

  // Let's unpack the values
  Local<String> service_principal_name = info[1]->ToString();
  service_principal_name_str = (char *)calloc(service_principal_name->Utf8Length() + 1, sizeof(char));
  service_principal_name->WriteUtf8(service_principal_name_str);

  // Unpack the user name
  Local<String> input = info[2]->ToString();

  if(input->Utf8Length() > 0) {
    input_str = (char *)calloc(input->Utf8Length() + 1, sizeof(char));
    input->WriteUtf8(input_str);

    // Now let's get the base64 decoded string
    decoded_input_str = (char *)base64_decode(input_str, &decoded_input_str_length);
    // Free original allocation
    free(input_str);
  }

  // Unpack the Security credentials
  security_credentials = Nan::ObjectWrap::Unwrap<SecurityCredentials>(info[0]->ToObject());
  // Create Security context instance
  Local<Object> security_context_value = Nan::New(constructor_template)->GetFunction()->NewInstance();
  // Unwrap the security context
  SecurityContext *security_context = Nan::ObjectWrap::Unwrap<SecurityContext>(security_context_value);
  // Add a reference to the security_credentials
  security_context->security_credentials = security_credentials;

  // Build the call function
  SecurityContextStaticInitializeCall *call = (SecurityContextStaticInitializeCall *)calloc(1, sizeof(SecurityContextStaticInitializeCall));
  call->context = security_context;
  call->decoded_input_str = decoded_input_str;
  call->decoded_input_str_length = decoded_input_str_length;
  call->service_principal_name_str = service_principal_name_str;

  // Callback
  Local<Function> callback = Local<Function>::Cast(info[3]);

  // Let's allocate some space
  Worker *worker = new Worker();
  worker->error = false;
  worker->request.data = worker;
  worker->callback = new Nan::Callback(callback);
  worker->parameters = call;
  worker->execute = _initializeContext;
  worker->mapper = _map_initializeContext;

  // Schedule the worker with lib_uv
  uv_queue_work(uv_default_loop(), &worker->request, Process, (uv_after_work_cb)After);

  // Return no value as it's callback based
  info.GetReturnValue().Set(Nan::Undefined());
}

NAN_GETTER(SecurityContext::PayloadGetter) {
  // Unpack the context object
  SecurityContext *context = Nan::ObjectWrap::Unwrap<SecurityContext>(info.This());
  // Return the low bits
  info.GetReturnValue().Set(Nan::New<String>(context->payload).ToLocalChecked());
}

NAN_GETTER(SecurityContext::HasContextGetter) {
  // Unpack the context object
  SecurityContext *context = Nan::ObjectWrap::Unwrap<SecurityContext>(info.This());
  // Return the low bits
  info.GetReturnValue().Set(Nan::New<Boolean>(context->hasContext));
}

//
//  Async InitializeContextStep
//
typedef struct SecurityContextStepStaticInitializeCall {
  char *service_principal_name_str;
  char *decoded_input_str;
  int decoded_input_str_length;
  SecurityContext *context;
} SecurityContextStepStaticInitializeCall;

static void _initializeContextStep(Worker *worker) {
  // Outbound data array
  BYTE *out_bound_data_str = NULL;
  // Status of operation
  SECURITY_STATUS status;
  // Unpack data
  SecurityContextStepStaticInitializeCall *call = (SecurityContextStepStaticInitializeCall *)worker->parameters;
  SecurityContext *context = call->context;
  // Structures used for c calls
  SecBufferDesc ibd, obd;
  SecBuffer ib, ob;

  //
  // Prepare data structure for returned data from SSPI
  ob.BufferType = SECBUFFER_TOKEN;
  ob.cbBuffer = context->m_PkgInfo->cbMaxToken;
  // Allocate space for return data
  out_bound_data_str = new BYTE[ob.cbBuffer + sizeof(DWORD)];
  ob.pvBuffer = out_bound_data_str;
  // prepare buffer description
  obd.cBuffers  = 1;
  obd.ulVersion = SECBUFFER_VERSION;
  obd.pBuffers  = &ob;

  //
  // Prepare the data we are passing to the SSPI method
  if(call->decoded_input_str_length > 0) {
    ib.BufferType = SECBUFFER_TOKEN;
    ib.cbBuffer   = call->decoded_input_str_length;
    ib.pvBuffer   = call->decoded_input_str;
    // prepare buffer description
    ibd.cBuffers  = 1;
    ibd.ulVersion = SECBUFFER_VERSION;
    ibd.pBuffers  = &ib;
  }

  // Perform initialization step
  status = _sspi_initializeSecurityContext(
      &context->security_credentials->m_Credentials
    , context->hasContext == true ? &context->m_Context : NULL
    , const_cast<TCHAR*>(call->service_principal_name_str)
    , 0x02  // MUTUAL
    , 0
    , 0     // Network
    , call->decoded_input_str_length ? &ibd : NULL
    , 0
    , &context->m_Context
    , &obd
    , &context->CtxtAttr
    , &context->Expiration
  );

  // If we have a ok or continue let's prepare the result
  if(status == SEC_E_OK
    || status == SEC_I_COMPLETE_NEEDED
    || status == SEC_I_CONTINUE_NEEDED
    || status == SEC_I_COMPLETE_AND_CONTINUE
  ) {
    // Set the new payload
    if(context->payload != NULL) free(context->payload);
    context->payload = base64_encode((const unsigned char *)ob.pvBuffer, ob.cbBuffer);
    worker->return_code = status;
    worker->return_value = context;
  } else {
    worker->error = TRUE;
    worker->error_code = status;
    worker->error_message = DisplaySECError(status);
  }

  // Clean up data
  if(call->decoded_input_str != NULL) free(call->decoded_input_str);
  if(call->service_principal_name_str != NULL) free(call->service_principal_name_str);
}

static Local<Value> _map_initializeContextStep(Worker *worker) {
  // Unwrap the security context
  SecurityContext *context = (SecurityContext *)worker->return_value;
  // Return the value
  return context->handle();
}

NAN_METHOD(SecurityContext::InitalizeStep) {
  char *service_principal_name_str = NULL, *input_str = NULL, *decoded_input_str = NULL;
  int decoded_input_str_length = NULL;

  // We need 3 parameters
  if(info.Length() != 3)
    return Nan::ThrowError("Initialize must be called with [servicePrincipalName:string, input:string, callback:function]");

  // Second parameter must be a string
  if(!info[0]->IsString())
    return Nan::ThrowError("First parameter for Initialize must be a string");

  // Third parameter must be a base64 encoded string
  if(!info[1]->IsString())
    return Nan::ThrowError("Second parameter for Initialize must be a string");

  // Third parameter must be a base64 encoded string
  if(!info[2]->IsFunction())
    return Nan::ThrowError("Third parameter for Initialize must be a callback function");

  // Let's unpack the values
  Local<String> service_principal_name = info[0]->ToString();
  service_principal_name_str = (char *)calloc(service_principal_name->Utf8Length() + 1, sizeof(char));
  service_principal_name->WriteUtf8(service_principal_name_str);

  // Unpack the user name
  Local<String> input = info[1]->ToString();

  if(input->Utf8Length() > 0) {
    input_str = (char *)calloc(input->Utf8Length() + 1, sizeof(char));
    input->WriteUtf8(input_str);
    // Now let's get the base64 decoded string
    decoded_input_str = (char *)base64_decode(input_str, &decoded_input_str_length);
    // Free input string
    free(input_str);
  }

  // Unwrap the security context
  SecurityContext *security_context = Nan::ObjectWrap::Unwrap<SecurityContext>(info.This());

  // Create call structure
  SecurityContextStepStaticInitializeCall *call = (SecurityContextStepStaticInitializeCall *)calloc(1, sizeof(SecurityContextStepStaticInitializeCall));
  call->context = security_context;
  call->decoded_input_str = decoded_input_str;
  call->decoded_input_str_length = decoded_input_str_length;
  call->service_principal_name_str = service_principal_name_str;

  // Callback
  Local<Function> callback = Local<Function>::Cast(info[2]);

  // Let's allocate some space
  Worker *worker = new Worker();
  worker->error = false;
  worker->request.data = worker;
  worker->callback = new Nan::Callback(callback);
  worker->parameters = call;
  worker->execute = _initializeContextStep;
  worker->mapper = _map_initializeContextStep;

  // Schedule the worker with lib_uv
  uv_queue_work(uv_default_loop(), &worker->request, Process, (uv_after_work_cb)After);

  // Return no value as it's callback based
  info.GetReturnValue().Set(Nan::Undefined());
}

//
//  Async EncryptMessage
//
typedef struct SecurityContextEncryptMessageCall {
  SecurityContext *context;
  SecurityBufferDescriptor *descriptor;
  unsigned long flags;
} SecurityContextEncryptMessageCall;

static void _encryptMessage(Worker *worker) {
  SECURITY_STATUS status;
  // Unpack call
  SecurityContextEncryptMessageCall *call = (SecurityContextEncryptMessageCall *)worker->parameters;
  // Unpack the security context
  SecurityContext *context = call->context;
  SecurityBufferDescriptor *descriptor = call->descriptor;

  // Let's execute encryption
  status = _sspi_EncryptMessage(
      &context->m_Context
    , call->flags
    , &descriptor->secBufferDesc
    , 0
  );

  // We've got ok
  if(status == SEC_E_OK) {
    int bytesToAllocate = (int)descriptor->bufferSize();
    // Free up existing payload
    if(context->payload != NULL) free(context->payload);
    // Save the payload
    context->payload = base64_encode((unsigned char *)descriptor->toBuffer(), bytesToAllocate);
    // Set result
    worker->return_code = status;
    worker->return_value = context;
  } else {
    worker->error = TRUE;
    worker->error_code = status;
    worker->error_message = DisplaySECError(status);
  }
}

static Local<Value> _map_encryptMessage(Worker *worker) {
  // Unwrap the security context
  SecurityContext *context = (SecurityContext *)worker->return_value;
  // Return the value
  return context->handle();
}

NAN_METHOD(SecurityContext::EncryptMessage) {
  if(info.Length() != 3)
    return Nan::ThrowError("EncryptMessage takes an instance of SecurityBufferDescriptor, an integer flag and a callback function");
  if(!SecurityBufferDescriptor::HasInstance(info[0]))
    return Nan::ThrowError("EncryptMessage takes an instance of SecurityBufferDescriptor, an integer flag and a callback function");
  if(!info[1]->IsUint32())
    return Nan::ThrowError("EncryptMessage takes an instance of SecurityBufferDescriptor, an integer flag and a callback function");
  if(!info[2]->IsFunction())
    return Nan::ThrowError("EncryptMessage takes an instance of SecurityBufferDescriptor, an integer flag and a callback function");

  // Unpack the security context
  SecurityContext *security_context = Nan::ObjectWrap::Unwrap<SecurityContext>(info.This());

  // Unpack the descriptor
  SecurityBufferDescriptor *descriptor = Nan::ObjectWrap::Unwrap<SecurityBufferDescriptor>(info[0]->ToObject());

  // Create call structure
  SecurityContextEncryptMessageCall *call = (SecurityContextEncryptMessageCall *)calloc(1, sizeof(SecurityContextEncryptMessageCall));
  call->context = security_context;
  call->descriptor = descriptor;
  call->flags = (unsigned long)info[1]->ToInteger()->Value();

  // Callback
  Local<Function> callback = Local<Function>::Cast(info[2]);

  // Let's allocate some space
  Worker *worker = new Worker();
  worker->error = false;
  worker->request.data = worker;
  worker->callback = new Nan::Callback(callback);
  worker->parameters = call;
  worker->execute = _encryptMessage;
  worker->mapper = _map_encryptMessage;

  // Schedule the worker with lib_uv
  uv_queue_work(uv_default_loop(), &worker->request, Process, (uv_after_work_cb)After);

  // Return no value as it's callback based
  info.GetReturnValue().Set(Nan::Undefined());
}

//
//  Async DecryptMessage
//
typedef struct SecurityContextDecryptMessageCall {
  SecurityContext *context;
  SecurityBufferDescriptor *descriptor;
} SecurityContextDecryptMessageCall;

static void _decryptMessage(Worker *worker) {
  unsigned long quality = 0;
  SECURITY_STATUS status;

  // Unpack parameters
  SecurityContextDecryptMessageCall *call = (SecurityContextDecryptMessageCall *)worker->parameters;
  SecurityContext *context = call->context;
  SecurityBufferDescriptor *descriptor = call->descriptor;

  // Let's execute encryption
  status = _sspi_DecryptMessage(
      &context->m_Context
    , &descriptor->secBufferDesc
    , 0
    , (unsigned long)&quality
  );

  // We've got ok
  if(status == SEC_E_OK) {
    int bytesToAllocate = (int)descriptor->bufferSize();
    // Free up existing payload
    if(context->payload != NULL) free(context->payload);
    // Save the payload
    context->payload = base64_encode((unsigned char *)descriptor->toBuffer(), bytesToAllocate);
    // Set return values
    worker->return_code = status;
    worker->return_value = context;
  } else {
    worker->error = TRUE;
    worker->error_code = status;
    worker->error_message = DisplaySECError(status);
  }
}

static Local<Value> _map_decryptMessage(Worker *worker) {
  // Unwrap the security context
  SecurityContext *context = (SecurityContext *)worker->return_value;
  // Return the value
  return context->handle();
}

NAN_METHOD(SecurityContext::DecryptMessage) {
  if(info.Length() != 2)
    return Nan::ThrowError("DecryptMessage takes an instance of SecurityBufferDescriptor and a callback function");
  if(!SecurityBufferDescriptor::HasInstance(info[0]))
    return Nan::ThrowError("DecryptMessage takes an instance of SecurityBufferDescriptor and a callback function");
  if(!info[1]->IsFunction())
    return Nan::ThrowError("DecryptMessage takes an instance of SecurityBufferDescriptor and a callback function");

  // Unpack the security context
  SecurityContext *security_context = Nan::ObjectWrap::Unwrap<SecurityContext>(info.This());
  // Unpack the descriptor
  SecurityBufferDescriptor *descriptor = Nan::ObjectWrap::Unwrap<SecurityBufferDescriptor>(info[0]->ToObject());
  // Create call structure
  SecurityContextDecryptMessageCall *call = (SecurityContextDecryptMessageCall *)calloc(1, sizeof(SecurityContextDecryptMessageCall));
  call->context = security_context;
  call->descriptor = descriptor;

  // Callback
  Local<Function> callback = Local<Function>::Cast(info[1]);

  // Let's allocate some space
  Worker *worker = new Worker();
  worker->error = false;
  worker->request.data = worker;
  worker->callback = new Nan::Callback(callback);
  worker->parameters = call;
  worker->execute = _decryptMessage;
  worker->mapper = _map_decryptMessage;

  // Schedule the worker with lib_uv
  uv_queue_work(uv_default_loop(), &worker->request, Process, (uv_after_work_cb)After);

  // Return no value as it's callback based
  info.GetReturnValue().Set(Nan::Undefined());
}

//
//  Async QueryContextAttributes
//
typedef struct SecurityContextQueryContextAttributesCall {
  SecurityContext *context;
  uint32_t attribute;
} SecurityContextQueryContextAttributesCall;

static void _queryContextAttributes(Worker *worker) {
  SECURITY_STATUS status;

  // Cast to data structure
  SecurityContextQueryContextAttributesCall *call = (SecurityContextQueryContextAttributesCall *)worker->parameters;

  // Allocate some space
  SecPkgContext_Sizes *sizes = (SecPkgContext_Sizes *)calloc(1, sizeof(SecPkgContext_Sizes));
  // Let's grab the query context attribute
  status = _sspi_QueryContextAttributes(
    &call->context->m_Context,
    call->attribute,
    sizes
  );

  if(status == SEC_E_OK) {
    worker->return_code = status;
    worker->return_value = sizes;
  } else {
    worker->error = TRUE;
    worker->error_code = status;
    worker->error_message = DisplaySECError(status);
  }
}

static Local<Value> _map_queryContextAttributes(Worker *worker) {
  // Cast to data structure
  SecurityContextQueryContextAttributesCall *call = (SecurityContextQueryContextAttributesCall *)worker->parameters;
  // Unpack the attribute
  uint32_t attribute = call->attribute;

  // Convert data
  if(attribute == SECPKG_ATTR_SIZES) {
    SecPkgContext_Sizes *sizes = (SecPkgContext_Sizes *)worker->return_value;
    // Create object
    Local<Object> value = Nan::New<Object>();
    value->Set(Nan::New<String>("maxToken").ToLocalChecked(), Nan::New<Integer>(uint32_t(sizes->cbMaxToken)));
    value->Set(Nan::New<String>("maxSignature").ToLocalChecked(), Nan::New<Integer>(uint32_t(sizes->cbMaxSignature)));
    value->Set(Nan::New<String>("blockSize").ToLocalChecked(), Nan::New<Integer>(uint32_t(sizes->cbBlockSize)));
    value->Set(Nan::New<String>("securityTrailer").ToLocalChecked(), Nan::New<Integer>(uint32_t(sizes->cbSecurityTrailer)));
    return value;
  }

  // Return the value
  return Nan::Null();
}

NAN_METHOD(SecurityContext::QueryContextAttributes) {
  if(info.Length() != 2)
    return Nan::ThrowError("QueryContextAttributes method takes a an integer Attribute specifier and a callback function");
  if(!info[0]->IsInt32())
    return Nan::ThrowError("QueryContextAttributes method takes a an integer Attribute specifier and a callback function");
  if(!info[1]->IsFunction())
    return Nan::ThrowError("QueryContextAttributes method takes a an integer Attribute specifier and a callback function");

  // Unpack the security context
  SecurityContext *security_context = Nan::ObjectWrap::Unwrap<SecurityContext>(info.This());

  // Unpack the int value
  uint32_t attribute = info[0]->ToInt32()->Value();

  // Check that we have a supported attribute
  if(attribute != SECPKG_ATTR_SIZES)
    return Nan::ThrowError("QueryContextAttributes only supports the SECPKG_ATTR_SIZES attribute");

  // Create call structure
  SecurityContextQueryContextAttributesCall *call = (SecurityContextQueryContextAttributesCall *)calloc(1, sizeof(SecurityContextQueryContextAttributesCall));
  call->attribute = attribute;
  call->context = security_context;

  // Callback
  Local<Function> callback = Local<Function>::Cast(info[1]);

  // Let's allocate some space
  Worker *worker = new Worker();
  worker->error = false;
  worker->request.data = worker;
  worker->callback = new Nan::Callback(callback);
  worker->parameters = call;
  worker->execute = _queryContextAttributes;
  worker->mapper = _map_queryContextAttributes;

  // Schedule the worker with lib_uv
  uv_queue_work(uv_default_loop(), &worker->request, Process, (uv_after_work_cb)After);

  // Return no value as it's callback based
  info.GetReturnValue().Set(Nan::Undefined());
}

void SecurityContext::Initialize(Nan::ADDON_REGISTER_FUNCTION_ARGS_TYPE target) {
  // Grab the scope of the call from Node
  Nan::HandleScope scope;

  // Define a new function template
  Local<FunctionTemplate> t = Nan::New<v8::FunctionTemplate>(static_cast<NAN_METHOD((*))>(SecurityContext::New));
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(Nan::New<String>("SecurityContext").ToLocalChecked());

  // Class methods
  Nan::SetMethod(t, "initialize", SecurityContext::InitializeContext);

  // Set up method for the instance
  Nan::SetPrototypeMethod(t, "initialize", SecurityContext::InitalizeStep);
  Nan::SetPrototypeMethod(t, "decryptMessage", SecurityContext::DecryptMessage);
  Nan::SetPrototypeMethod(t, "queryContextAttributes", SecurityContext::QueryContextAttributes);
  Nan::SetPrototypeMethod(t, "encryptMessage", SecurityContext::EncryptMessage);

  // Get prototype
  Local<ObjectTemplate> proto = t->PrototypeTemplate();

  // Getter for the response
  Nan::SetAccessor(proto, Nan::New<String>("payload").ToLocalChecked(), SecurityContext::PayloadGetter);
  Nan::SetAccessor(proto, Nan::New<String>("hasContext").ToLocalChecked(), SecurityContext::HasContextGetter);

  // Set persistent
  SecurityContext::constructor_template.Reset(t);

  // Set the symbol
  target->ForceSet(Nan::New<String>("SecurityContext").ToLocalChecked(), t->GetFunction());
}

static LPSTR DisplaySECError(DWORD ErrCode) {
  LPSTR pszName = NULL; // WinError.h

  switch(ErrCode) {
    case SEC_E_BUFFER_TOO_SMALL:
      pszName = "SEC_E_BUFFER_TOO_SMALL - The message buffer is too small. Used with the Digest SSP.";
      break;

    case SEC_E_CRYPTO_SYSTEM_INVALID:
      pszName = "SEC_E_CRYPTO_SYSTEM_INVALID - The cipher chosen for the security context is not supported. Used with the Digest SSP.";
      break;
    case SEC_E_INCOMPLETE_MESSAGE:
      pszName = "SEC_E_INCOMPLETE_MESSAGE - The data in the input buffer is incomplete. The application needs to read more data from the server and call DecryptMessageSync (General) again.";
      break;

    case SEC_E_INVALID_HANDLE:
      pszName = "SEC_E_INVALID_HANDLE - A context handle that is not valid was specified in the phContext parameter. Used with the Digest and Schannel SSPs.";
      break;

    case SEC_E_INVALID_TOKEN:
      pszName = "SEC_E_INVALID_TOKEN - The buffers are of the wrong type or no buffer of type SECBUFFER_DATA was found. Used with the Schannel SSP.";
      break;

    case SEC_E_MESSAGE_ALTERED:
      pszName = "SEC_E_MESSAGE_ALTERED - The message has been altered. Used with the Digest and Schannel SSPs.";
      break;

    case SEC_E_OUT_OF_SEQUENCE:
      pszName = "SEC_E_OUT_OF_SEQUENCE - The message was not received in the correct sequence.";
      break;

    case SEC_E_QOP_NOT_SUPPORTED:
      pszName = "SEC_E_QOP_NOT_SUPPORTED - Neither confidentiality nor integrity are supported by the security context. Used with the Digest SSP.";
      break;

    case SEC_I_CONTEXT_EXPIRED:
      pszName = "SEC_I_CONTEXT_EXPIRED - The message sender has finished using the connection and has initiated a shutdown.";
      break;

    case SEC_I_RENEGOTIATE:
      pszName = "SEC_I_RENEGOTIATE - The remote party requires a new handshake sequence or the application has just initiated a shutdown.";
      break;

    case SEC_E_ENCRYPT_FAILURE:
      pszName = "SEC_E_ENCRYPT_FAILURE - The specified data could not be encrypted.";
      break;

    case SEC_E_DECRYPT_FAILURE:
      pszName = "SEC_E_DECRYPT_FAILURE - The specified data could not be decrypted.";
      break;
    case -1:
      pszName = "Failed to load security.dll library";
      break;
  }

  return pszName;
}


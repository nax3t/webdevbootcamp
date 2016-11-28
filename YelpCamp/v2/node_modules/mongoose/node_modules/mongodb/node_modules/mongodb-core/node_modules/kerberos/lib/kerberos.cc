#include "kerberos.h"
#include <stdlib.h>
#include <errno.h>
#include "worker.h"
#include "kerberos_context.h"

#ifndef ARRAY_SIZE
# define ARRAY_SIZE(a) (sizeof((a)) / sizeof((a)[0]))
#endif

void die(const char *message) {
  if(errno) {
    perror(message);
  } else {
    printf("ERROR: %s\n", message);
  }

  exit(1);
}

// Call structs
typedef struct AuthGSSClientCall {
  uint32_t  flags;
  char *uri;
} AuthGSSClientCall;

typedef struct AuthGSSClientStepCall {
  KerberosContext *context;
  char *challenge;
} AuthGSSClientStepCall;

typedef struct AuthGSSClientUnwrapCall {
  KerberosContext *context;
  char *challenge;
} AuthGSSClientUnwrapCall;

typedef struct AuthGSSClientWrapCall {
  KerberosContext *context;
  char *challenge;
  char *user_name;
} AuthGSSClientWrapCall;

typedef struct AuthGSSClientCleanCall {
  KerberosContext *context;
} AuthGSSClientCleanCall;

typedef struct AuthGSSServerInitCall {
  char *service;
  bool constrained_delegation;
  char *username;
} AuthGSSServerInitCall;

typedef struct AuthGSSServerCleanCall {
  KerberosContext *context;
} AuthGSSServerCleanCall;

typedef struct AuthGSSServerStepCall {
  KerberosContext *context;
  char *auth_data;
} AuthGSSServerStepCall;

Kerberos::Kerberos() : Nan::ObjectWrap() {
}

Nan::Persistent<FunctionTemplate> Kerberos::constructor_template;

void Kerberos::Initialize(Nan::ADDON_REGISTER_FUNCTION_ARGS_TYPE target) {
  // Grab the scope of the call from Node
  Nan::HandleScope scope;

  // Define a new function template
  Local<FunctionTemplate> t = Nan::New<FunctionTemplate>(New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(Nan::New<String>("Kerberos").ToLocalChecked());

  // Set up method for the Kerberos instance
  Nan::SetPrototypeMethod(t, "authGSSClientInit", AuthGSSClientInit);
  Nan::SetPrototypeMethod(t, "authGSSClientStep", AuthGSSClientStep);
  Nan::SetPrototypeMethod(t, "authGSSClientUnwrap", AuthGSSClientUnwrap);
  Nan::SetPrototypeMethod(t, "authGSSClientWrap", AuthGSSClientWrap);
  Nan::SetPrototypeMethod(t, "authGSSClientClean", AuthGSSClientClean);
  Nan::SetPrototypeMethod(t, "authGSSServerInit", AuthGSSServerInit);
  Nan::SetPrototypeMethod(t, "authGSSServerClean", AuthGSSServerClean);
  Nan::SetPrototypeMethod(t, "authGSSServerStep", AuthGSSServerStep);

  constructor_template.Reset(t);

  // Set the symbol
  target->ForceSet(Nan::New<String>("Kerberos").ToLocalChecked(), t->GetFunction());
}

NAN_METHOD(Kerberos::New) {
  // Create a Kerberos instance
  Kerberos *kerberos = new Kerberos();
  // Return the kerberos object
  kerberos->Wrap(info.This());
  info.GetReturnValue().Set(info.This());
}

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// authGSSClientInit
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
static void _authGSSClientInit(Worker *worker) {
  gss_client_state *state;
  gss_client_response *response;

  // Allocate state
  state = (gss_client_state *)malloc(sizeof(gss_client_state));
  if(state == NULL) die("Memory allocation failed");

  // Unpack the parameter data struct
  AuthGSSClientCall *call = (AuthGSSClientCall *)worker->parameters;
  // Start the kerberos client
  response = authenticate_gss_client_init(call->uri, call->flags, state);

  // Release the parameter struct memory
  free(call->uri);
  free(call);

  // If we have an error mark worker as having had an error
  if(response->return_code == AUTH_GSS_ERROR) {
    worker->error = TRUE;
    worker->error_code = response->return_code;
    worker->error_message = response->message;
    free(state);
  } else {
    worker->return_value = state;
  }

  // Free structure
  free(response);
}

static Local<Value> _map_authGSSClientInit(Worker *worker) {
  KerberosContext *context = KerberosContext::New();
  context->state = (gss_client_state *)worker->return_value;
  return context->handle();
}

// Initialize method
NAN_METHOD(Kerberos::AuthGSSClientInit) {
  // Ensure valid call
    if(info.Length() != 3) return Nan::ThrowError("Requires a service string uri, integer flags and a callback function");
  if(info.Length() == 3 && (!info[0]->IsString() || !info[1]->IsInt32() || !info[2]->IsFunction()))
      return Nan::ThrowError("Requires a service string uri, integer flags and a callback function");

  Local<String> service = info[0]->ToString();
  // Convert uri string to c-string
  char *service_str = (char *)calloc(service->Utf8Length() + 1, sizeof(char));
  if(service_str == NULL) die("Memory allocation failed");

  // Write v8 string to c-string
  service->WriteUtf8(service_str);

  // Allocate a structure
  AuthGSSClientCall *call = (AuthGSSClientCall *)calloc(1, sizeof(AuthGSSClientCall));
  if(call == NULL) die("Memory allocation failed");
  call->flags =info[1]->ToInt32()->Uint32Value();
  call->uri = service_str;

  // Unpack the callback
  Local<Function> callbackHandle = Local<Function>::Cast(info[2]);
  Nan::Callback *callback = new Nan::Callback(callbackHandle);

  // Let's allocate some space
  Worker *worker = new Worker();
  worker->error = false;
  worker->request.data = worker;
  worker->callback = callback;
  worker->parameters = call;
  worker->execute = _authGSSClientInit;
  worker->mapper = _map_authGSSClientInit;

  // Schedule the worker with lib_uv
  uv_queue_work(uv_default_loop(), &worker->request, Kerberos::Process, (uv_after_work_cb)Kerberos::After);
  // Return no value as it's callback based
  info.GetReturnValue().Set(Nan::Undefined());
}

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// authGSSClientStep
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
static void _authGSSClientStep(Worker *worker) {
  gss_client_state *state;
  gss_client_response *response;
  char *challenge;

  // Unpack the parameter data struct
  AuthGSSClientStepCall *call = (AuthGSSClientStepCall *)worker->parameters;
  // Get the state
  state = call->context->state;
  challenge = call->challenge;

  // Check what kind of challenge we have
  if(call->challenge == NULL) {
    challenge = (char *)"";
  }

  // Perform authentication step
  response = authenticate_gss_client_step(state, challenge);

  // If we have an error mark worker as having had an error
  if(response->return_code == AUTH_GSS_ERROR) {
    worker->error = TRUE;
    worker->error_code = response->return_code;
    worker->error_message = response->message;
  } else {
    worker->return_code = response->return_code;
  }

  // Free up structure
  if(call->challenge != NULL) free(call->challenge);
  free(call);
  free(response);
}

static Local<Value> _map_authGSSClientStep(Worker *worker) {
  Nan::HandleScope scope;
  // Return the return code
  return Nan::New<Int32>(worker->return_code);
}

// Initialize method
NAN_METHOD(Kerberos::AuthGSSClientStep) {
  // Ensure valid call
  if(info.Length() != 2 && info.Length() != 3) return Nan::ThrowError("Requires a GSS context, optional challenge string and callback function");
  if(info.Length() == 2 && (!KerberosContext::HasInstance(info[0]) || !info[1]->IsFunction())) return Nan::ThrowError("Requires a GSS context, optional challenge string and callback function");
  if(info.Length() == 3 && (!KerberosContext::HasInstance(info[0]) || !info[1]->IsString() || !info[2]->IsFunction())) return Nan::ThrowError("Requires a GSS context, optional challenge string and callback function");

  // Challenge string
  char *challenge_str = NULL;
  // Let's unpack the parameters
  Local<Object> object = info[0]->ToObject();
  KerberosContext *kerberos_context = KerberosContext::Unwrap<KerberosContext>(object);

  if (!kerberos_context->IsClientInstance()) {
      return Nan::ThrowError("GSS context is not a client instance");
  }

  int callbackArg = 1;

  // If we have a challenge string
  if(info.Length() == 3) {
    // Unpack the challenge string
    Local<String> challenge = info[1]->ToString();
    // Convert uri string to c-string
    challenge_str = (char *)calloc(challenge->Utf8Length() + 1, sizeof(char));
    if(challenge_str == NULL) die("Memory allocation failed");
    // Write v8 string to c-string
    challenge->WriteUtf8(challenge_str);

    callbackArg = 2;
  }

  // Allocate a structure
  AuthGSSClientStepCall *call = (AuthGSSClientStepCall *)calloc(1, sizeof(AuthGSSClientStepCall));
  if(call == NULL) die("Memory allocation failed");
  call->context = kerberos_context;
  call->challenge = challenge_str;

  // Unpack the callback
  Local<Function> callbackHandle = Local<Function>::Cast(info[callbackArg]);
  Nan::Callback *callback = new Nan::Callback(callbackHandle);

  // Let's allocate some space
  Worker *worker = new Worker();
  worker->error = false;
  worker->request.data = worker;
  worker->callback = callback;
  worker->parameters = call;
  worker->execute = _authGSSClientStep;
  worker->mapper = _map_authGSSClientStep;

  // Schedule the worker with lib_uv
  uv_queue_work(uv_default_loop(), &worker->request, Kerberos::Process, (uv_after_work_cb)Kerberos::After);

  // Return no value as it's callback based
  info.GetReturnValue().Set(Nan::Undefined());
}

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// authGSSClientUnwrap
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
static void _authGSSClientUnwrap(Worker *worker) {
  gss_client_response *response;
  char *challenge;

  // Unpack the parameter data struct
  AuthGSSClientUnwrapCall *call = (AuthGSSClientUnwrapCall *)worker->parameters;
  challenge = call->challenge;

  // Check what kind of challenge we have
  if(call->challenge == NULL) {
    challenge = (char *)"";
  }

  // Perform authentication step
  response = authenticate_gss_client_unwrap(call->context->state, challenge);

  // If we have an error mark worker as having had an error
  if(response->return_code == AUTH_GSS_ERROR) {
    worker->error = TRUE;
    worker->error_code = response->return_code;
    worker->error_message = response->message;
  } else {
    worker->return_code = response->return_code;
  }

  // Free up structure
  if(call->challenge != NULL) free(call->challenge);
  free(call);
  free(response);
}

static Local<Value> _map_authGSSClientUnwrap(Worker *worker) {
  Nan::HandleScope scope;
  // Return the return code
  return Nan::New<Int32>(worker->return_code);
}

// Initialize method
NAN_METHOD(Kerberos::AuthGSSClientUnwrap) {
  // Ensure valid call
    if(info.Length() != 2 && info.Length() != 3) return Nan::ThrowError("Requires a GSS context, optional challenge string and callback function");
    if(info.Length() == 2 && (!KerberosContext::HasInstance(info[0]) || !info[1]->IsFunction())) return Nan::ThrowError("Requires a GSS context, optional challenge string and callback function");
    if(info.Length() == 3 && (!KerberosContext::HasInstance(info[0]) || !info[1]->IsString() || !info[2]->IsFunction())) return Nan::ThrowError("Requires a GSS context, optional challenge string and callback function");

  // Challenge string
  char *challenge_str = NULL;
  // Let's unpack the parameters
  Local<Object> object = info[0]->ToObject();
  KerberosContext *kerberos_context = KerberosContext::Unwrap<KerberosContext>(object);

  if (!kerberos_context->IsClientInstance()) {
      return Nan::ThrowError("GSS context is not a client instance");
  }

  // If we have a challenge string
  if(info.Length() == 3) {
    // Unpack the challenge string
    Local<String> challenge = info[1]->ToString();
    // Convert uri string to c-string
    challenge_str = (char *)calloc(challenge->Utf8Length() + 1, sizeof(char));
    if(challenge_str == NULL) die("Memory allocation failed");
    // Write v8 string to c-string
    challenge->WriteUtf8(challenge_str);
  }

  // Allocate a structure
  AuthGSSClientUnwrapCall *call = (AuthGSSClientUnwrapCall *)calloc(1, sizeof(AuthGSSClientUnwrapCall));
  if(call == NULL) die("Memory allocation failed");
  call->context = kerberos_context;
  call->challenge = challenge_str;

  // Unpack the callback
  Local<Function> callbackHandle = info.Length() == 3 ? Local<Function>::Cast(info[2]) : Local<Function>::Cast(info[1]);
  Nan::Callback *callback = new Nan::Callback(callbackHandle);

  // Let's allocate some space
  Worker *worker = new Worker();
  worker->error = false;
  worker->request.data = worker;
  worker->callback = callback;
  worker->parameters = call;
  worker->execute = _authGSSClientUnwrap;
  worker->mapper = _map_authGSSClientUnwrap;

  // Schedule the worker with lib_uv
  uv_queue_work(uv_default_loop(), &worker->request, Kerberos::Process, (uv_after_work_cb)Kerberos::After);

  // Return no value as it's callback based
  info.GetReturnValue().Set(Nan::Undefined());
}

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// authGSSClientWrap
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
static void _authGSSClientWrap(Worker *worker) {
  gss_client_response *response;
  char *user_name = NULL;

  // Unpack the parameter data struct
  AuthGSSClientWrapCall *call = (AuthGSSClientWrapCall *)worker->parameters;
  user_name = call->user_name;

  // Check what kind of challenge we have
  if(call->user_name == NULL) {
    user_name = (char *)"";
  }

  // Perform authentication step
  response = authenticate_gss_client_wrap(call->context->state, call->challenge, user_name);

  // If we have an error mark worker as having had an error
  if(response->return_code == AUTH_GSS_ERROR) {
    worker->error = TRUE;
    worker->error_code = response->return_code;
    worker->error_message = response->message;
  } else {
    worker->return_code = response->return_code;
  }

  // Free up structure
  if(call->challenge != NULL) free(call->challenge);
  if(call->user_name != NULL) free(call->user_name);
  free(call);
  free(response);
}

static Local<Value> _map_authGSSClientWrap(Worker *worker) {
  Nan::HandleScope scope;
  // Return the return code
  return Nan::New<Int32>(worker->return_code);
}

// Initialize method
NAN_METHOD(Kerberos::AuthGSSClientWrap) {
  // Ensure valid call
    if(info.Length() != 3 && info.Length() != 4) return Nan::ThrowError("Requires a GSS context, the result from the authGSSClientResponse after authGSSClientUnwrap, optional user name and callback function");
    if(info.Length() == 3 && (!KerberosContext::HasInstance(info[0]) || !info[1]->IsString() || !info[2]->IsFunction())) return Nan::ThrowError("Requires a GSS context, the result from the authGSSClientResponse after authGSSClientUnwrap, optional user name and callback function");
    if(info.Length() == 4 && (!KerberosContext::HasInstance(info[0]) || !info[1]->IsString() || !info[2]->IsString() || !info[3]->IsFunction())) return Nan::ThrowError("Requires a GSS context, the result from the authGSSClientResponse after authGSSClientUnwrap, optional user name and callback function");

  // Challenge string
  char *challenge_str = NULL;
  char *user_name_str = NULL;

  // Let's unpack the kerberos context
  Local<Object> object = info[0]->ToObject();
  KerberosContext *kerberos_context = KerberosContext::Unwrap<KerberosContext>(object);

  if (!kerberos_context->IsClientInstance()) {
      return Nan::ThrowError("GSS context is not a client instance");
  }

  // Unpack the challenge string
  Local<String> challenge = info[1]->ToString();
  // Convert uri string to c-string
  challenge_str = (char *)calloc(challenge->Utf8Length() + 1, sizeof(char));
  if(challenge_str == NULL) die("Memory allocation failed");
  // Write v8 string to c-string
  challenge->WriteUtf8(challenge_str);

  // If we have a user string
  if(info.Length() == 4) {
    // Unpack user name
    Local<String> user_name = info[2]->ToString();
    // Convert uri string to c-string
    user_name_str = (char *)calloc(user_name->Utf8Length() + 1, sizeof(char));
    if(user_name_str == NULL) die("Memory allocation failed");
    // Write v8 string to c-string
    user_name->WriteUtf8(user_name_str);
  }

  // Allocate a structure
  AuthGSSClientWrapCall *call = (AuthGSSClientWrapCall *)calloc(1, sizeof(AuthGSSClientWrapCall));
  if(call == NULL) die("Memory allocation failed");
  call->context = kerberos_context;
  call->challenge = challenge_str;
  call->user_name = user_name_str;

  // Unpack the callback
  Local<Function> callbackHandle = info.Length() == 4 ? Local<Function>::Cast(info[3]) : Local<Function>::Cast(info[2]);
  Nan::Callback *callback = new Nan::Callback(callbackHandle);

  // Let's allocate some space
  Worker *worker = new Worker();
  worker->error = false;
  worker->request.data = worker;
  worker->callback = callback;
  worker->parameters = call;
  worker->execute = _authGSSClientWrap;
  worker->mapper = _map_authGSSClientWrap;

  // Schedule the worker with lib_uv
  uv_queue_work(uv_default_loop(), &worker->request, Kerberos::Process, (uv_after_work_cb)Kerberos::After);

  // Return no value as it's callback based
  info.GetReturnValue().Set(Nan::Undefined());
}

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// authGSSClientClean
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
static void _authGSSClientClean(Worker *worker) {
  gss_client_response *response;

  // Unpack the parameter data struct
  AuthGSSClientCleanCall *call = (AuthGSSClientCleanCall *)worker->parameters;

  // Perform authentication step
  response = authenticate_gss_client_clean(call->context->state);

  // If we have an error mark worker as having had an error
  if(response->return_code == AUTH_GSS_ERROR) {
    worker->error = TRUE;
    worker->error_code = response->return_code;
    worker->error_message = response->message;
  } else {
    worker->return_code = response->return_code;
  }

  // Free up structure
  free(call);
  free(response);
}

static Local<Value> _map_authGSSClientClean(Worker *worker) {
  Nan::HandleScope scope;
  // Return the return code
  return Nan::New<Int32>(worker->return_code);
}

// Initialize method
NAN_METHOD(Kerberos::AuthGSSClientClean) {
  // Ensure valid call
  if(info.Length() != 2) return Nan::ThrowError("Requires a GSS context and callback function");
  if(!KerberosContext::HasInstance(info[0]) || !info[1]->IsFunction()) return Nan::ThrowError("Requires a GSS context and callback function");

  // Let's unpack the kerberos context
  Local<Object> object = info[0]->ToObject();
  KerberosContext *kerberos_context = KerberosContext::Unwrap<KerberosContext>(object);

  if (!kerberos_context->IsClientInstance()) {
      return Nan::ThrowError("GSS context is not a client instance");
  }

  // Allocate a structure
  AuthGSSClientCleanCall *call = (AuthGSSClientCleanCall *)calloc(1, sizeof(AuthGSSClientCleanCall));
  if(call == NULL) die("Memory allocation failed");
  call->context = kerberos_context;

  // Unpack the callback
  Local<Function> callbackHandle = Local<Function>::Cast(info[1]);
  Nan::Callback *callback = new Nan::Callback(callbackHandle);

  // Let's allocate some space
  Worker *worker = new Worker();
  worker->error = false;
  worker->request.data = worker;
  worker->callback = callback;
  worker->parameters = call;
  worker->execute = _authGSSClientClean;
  worker->mapper = _map_authGSSClientClean;

  // Schedule the worker with lib_uv
  uv_queue_work(uv_default_loop(), &worker->request, Kerberos::Process, (uv_after_work_cb)Kerberos::After);

  // Return no value as it's callback based
  info.GetReturnValue().Set(Nan::Undefined());
}

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// authGSSServerInit
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
static void _authGSSServerInit(Worker *worker) {
  gss_server_state *state;
  gss_client_response *response;

  // Allocate state
  state = (gss_server_state *)malloc(sizeof(gss_server_state));
  if(state == NULL) die("Memory allocation failed");

  // Unpack the parameter data struct
  AuthGSSServerInitCall *call = (AuthGSSServerInitCall *)worker->parameters;
  // Start the kerberos service
  response = authenticate_gss_server_init(call->service, call->constrained_delegation, call->username, state);

  // Release the parameter struct memory
  free(call->service);
  free(call->username);
  free(call);

  // If we have an error mark worker as having had an error
  if(response->return_code == AUTH_GSS_ERROR) {
    worker->error = TRUE;
    worker->error_code = response->return_code;
    worker->error_message = response->message;
    free(state);
  } else {
    worker->return_value = state;
  }

  // Free structure
  free(response);
}

static Local<Value> _map_authGSSServerInit(Worker *worker) {
  KerberosContext *context = KerberosContext::New();
  context->server_state = (gss_server_state *)worker->return_value;
  return context->handle();
}

// Server Initialize method
NAN_METHOD(Kerberos::AuthGSSServerInit) {
  // Ensure valid call
  if(info.Length() != 4) return Nan::ThrowError("Requires a service string, constrained delegation boolean, a username string (or NULL) for S4U2Self protocol transition and a callback function");

  if(!info[0]->IsString() ||
	  !info[1]->IsBoolean() ||
	  !(info[2]->IsString() || info[2]->IsNull()) ||
	  !info[3]->IsFunction()
     ) return Nan::ThrowError("Requires a service string, constrained delegation boolean, a username string (or NULL) for S4U2Self protocol transition and  a callback function");

  if (!info[1]->BooleanValue() && !info[2]->IsNull()) return Nan::ThrowError("S4U2Self only possible when constrained delegation is enabled");

  // Allocate a structure
  AuthGSSServerInitCall *call = (AuthGSSServerInitCall *)calloc(1, sizeof(AuthGSSServerInitCall));
  if(call == NULL) die("Memory allocation failed");

  Local<String> service = info[0]->ToString();
  // Convert service string to c-string
  char *service_str = (char *)calloc(service->Utf8Length() + 1, sizeof(char));
  if(service_str == NULL) die("Memory allocation failed");

  // Write v8 string to c-string
  service->WriteUtf8(service_str);

  call->service = service_str;

  call->constrained_delegation = info[1]->BooleanValue();

  if (info[2]->IsNull())
  {
      call->username = NULL;
  }
  else
  {
      Local<String> tmpString = info[2]->ToString();

      char *tmpCstr = (char *)calloc(tmpString->Utf8Length() + 1, sizeof(char));
      if(tmpCstr == NULL) die("Memory allocation failed");

      tmpString->WriteUtf8(tmpCstr);

      call->username = tmpCstr;
  }

  // Unpack the callback
  Local<Function> callbackHandle = Local<Function>::Cast(info[3]);
  Nan::Callback *callback = new Nan::Callback(callbackHandle);

  // Let's allocate some space
  Worker *worker = new Worker();
  worker->error = false;
  worker->request.data = worker;
  worker->callback = callback;
  worker->parameters = call;
  worker->execute = _authGSSServerInit;
  worker->mapper = _map_authGSSServerInit;

  // Schedule the worker with lib_uv
  uv_queue_work(uv_default_loop(), &worker->request, Kerberos::Process, (uv_after_work_cb)Kerberos::After);
  // Return no value as it's callback based
  info.GetReturnValue().Set(Nan::Undefined());
}

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// authGSSServerClean
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
static void _authGSSServerClean(Worker *worker) {
  gss_client_response *response;

  // Unpack the parameter data struct
  AuthGSSServerCleanCall *call = (AuthGSSServerCleanCall *)worker->parameters;

  // Perform authentication step
  response = authenticate_gss_server_clean(call->context->server_state);

  // If we have an error mark worker as having had an error
  if(response->return_code == AUTH_GSS_ERROR) {
    worker->error = TRUE;
    worker->error_code = response->return_code;
    worker->error_message = response->message;
  } else {
    worker->return_code = response->return_code;
  }

  // Free up structure
  free(call);
  free(response);
}

static Local<Value> _map_authGSSServerClean(Worker *worker) {
  Nan::HandleScope scope;
  // Return the return code
  return Nan::New<Int32>(worker->return_code);
}

// Initialize method
NAN_METHOD(Kerberos::AuthGSSServerClean) {
  // // Ensure valid call
  if(info.Length() != 2) return Nan::ThrowError("Requires a GSS context and callback function");
  if(!KerberosContext::HasInstance(info[0]) || !info[1]->IsFunction()) return Nan::ThrowError("Requires a GSS context and callback function");

  // Let's unpack the kerberos context
  Local<Object> object = info[0]->ToObject();
  KerberosContext *kerberos_context = KerberosContext::Unwrap<KerberosContext>(object);

  if (!kerberos_context->IsServerInstance()) {
      return Nan::ThrowError("GSS context is not a server instance");
  }

  // Allocate a structure
  AuthGSSServerCleanCall *call = (AuthGSSServerCleanCall *)calloc(1, sizeof(AuthGSSServerCleanCall));
  if(call == NULL) die("Memory allocation failed");
  call->context = kerberos_context;

  // Unpack the callback
  Local<Function> callbackHandle = Local<Function>::Cast(info[1]);
  Nan::Callback *callback = new Nan::Callback(callbackHandle);

  // Let's allocate some space
  Worker *worker = new Worker();
  worker->error = false;
  worker->request.data = worker;
  worker->callback = callback;
  worker->parameters = call;
  worker->execute = _authGSSServerClean;
  worker->mapper = _map_authGSSServerClean;

  // Schedule the worker with lib_uv
  uv_queue_work(uv_default_loop(), &worker->request, Kerberos::Process, (uv_after_work_cb)Kerberos::After);

  // Return no value as it's callback based
  info.GetReturnValue().Set(Nan::Undefined());
}

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// authGSSServerStep
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
static void _authGSSServerStep(Worker *worker) {
  gss_server_state *state;
  gss_client_response *response;
  char *auth_data;

  // Unpack the parameter data struct
  AuthGSSServerStepCall *call = (AuthGSSServerStepCall *)worker->parameters;
  // Get the state
  state = call->context->server_state;
  auth_data = call->auth_data;

  // Check if we got auth_data or not
  if(call->auth_data == NULL) {
    auth_data = (char *)"";
  }

  // Perform authentication step
  response = authenticate_gss_server_step(state, auth_data);

  // If we have an error mark worker as having had an error
  if(response->return_code == AUTH_GSS_ERROR) {
    worker->error = TRUE;
    worker->error_code = response->return_code;
    worker->error_message = response->message;
  } else {
    worker->return_code = response->return_code;
  }

  // Free up structure
  if(call->auth_data != NULL) free(call->auth_data);
  free(call);
  free(response);
}

static Local<Value> _map_authGSSServerStep(Worker *worker) {
  Nan::HandleScope scope;
  // Return the return code
  return Nan::New<Int32>(worker->return_code);
}

// Initialize method
NAN_METHOD(Kerberos::AuthGSSServerStep) {
  // Ensure valid call
  if(info.Length() != 3) return Nan::ThrowError("Requires a GSS context, auth-data string and callback function");
  if(!KerberosContext::HasInstance(info[0]))  return Nan::ThrowError("1st arg must be a GSS context");
  if (!info[1]->IsString())  return Nan::ThrowError("2nd arg must be auth-data string");
  if (!info[2]->IsFunction())  return Nan::ThrowError("3rd arg must be a callback function");

  // Auth-data string
  char *auth_data_str = NULL;
  // Let's unpack the parameters
  Local<Object> object = info[0]->ToObject();
  KerberosContext *kerberos_context = KerberosContext::Unwrap<KerberosContext>(object);

  if (!kerberos_context->IsServerInstance()) {
      return Nan::ThrowError("GSS context is not a server instance");
  }

  // Unpack the auth_data string
  Local<String> auth_data = info[1]->ToString();
  // Convert uri string to c-string
  auth_data_str = (char *)calloc(auth_data->Utf8Length() + 1, sizeof(char));
  if(auth_data_str == NULL) die("Memory allocation failed");
  // Write v8 string to c-string
  auth_data->WriteUtf8(auth_data_str);

  // Allocate a structure
  AuthGSSServerStepCall *call = (AuthGSSServerStepCall *)calloc(1, sizeof(AuthGSSServerStepCall));
  if(call == NULL) die("Memory allocation failed");
  call->context = kerberos_context;
  call->auth_data = auth_data_str;

  // Unpack the callback
  Local<Function> callbackHandle = Local<Function>::Cast(info[2]);
  Nan::Callback *callback = new Nan::Callback(callbackHandle);

  // Let's allocate some space
  Worker *worker = new Worker();
  worker->error = false;
  worker->request.data = worker;
  worker->callback = callback;
  worker->parameters = call;
  worker->execute = _authGSSServerStep;
  worker->mapper = _map_authGSSServerStep;

  // Schedule the worker with lib_uv
  uv_queue_work(uv_default_loop(), &worker->request, Kerberos::Process, (uv_after_work_cb)Kerberos::After);

  // Return no value as it's callback based
  info.GetReturnValue().Set(Nan::Undefined());
}

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// UV Lib callbacks
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void Kerberos::Process(uv_work_t* work_req) {
  // Grab the worker
  Worker *worker = static_cast<Worker*>(work_req->data);
  // Execute the worker code
  worker->execute(worker);
}

void Kerberos::After(uv_work_t* work_req) {
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
    #if defined(V8_MAJOR_VERSION) && (V8_MAJOR_VERSION > 4 ||                      \
      (V8_MAJOR_VERSION == 4 && defined(V8_MINOR_VERSION) && V8_MINOR_VERSION >= 3))
    Local<Value> info[2] = { Nan::Null(), result};
    #else
    Local<Value> info[2] = { Nan::Null(), Nan::New<v8::Value>(result)};
    #endif

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

// Exporting function
NAN_MODULE_INIT(init) {
  Kerberos::Initialize(target);
  KerberosContext::Initialize(target);
}

NODE_MODULE(kerberos, init);
